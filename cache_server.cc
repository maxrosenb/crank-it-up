#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <ctime>
#include <unistd.h>

#include "cache.hh"
#include "evictor.hh"
#include "fifo_evictor.hh"
#include "lru_evictor.hh"

#include <mutex>
#include <shared_mutex>


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


Cache* cache_ = nullptr;
std::shared_mutex mutex_;

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{

    http::response<http::dynamic_body> res;
    res.version(req.version());

    // Respond to GET request
    if(req.method() == http::verb::get)
    {
        auto uri = std::string(req.target());
        assert(uri.length() > 1);
        //remove the / in the uri
        auto key = uri.substr(1);
        Cache::size_type size = 0;
        assert(cache_);
        Cache::val_type val;
        {
            std::shared_lock lock(mutex_);
            val = cache_->get(key, size);
        }

        if (val)
        {
            if (std::string(val).length() > 0)
            {
                assert(size > 0);
                res.set(http::field::content_type, "application/json"); 
                beast::ostream(res.body())
                << "{\n"
                << "    \"key\": \"" << key << "\",\n" 
                << "    \"value\": \"" << val << "\"\n"
                << "}\n";
            }
        }
        else
        {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/plain");
            beast::ostream(res.body()) << "Key not found\r\n";
        }

        return send(std::move(res));
    }

    // Respond to PUT request
    if(req.method() == http::verb::put)
    {
        auto uri = std::string(req.target()).substr(1);
        //std::cout << uri << "\n";
        auto sep_index = uri.find("/");
        assert(sep_index != -1);
        //std::cout << sep_index << "\n";
        auto key = uri.substr(0, sep_index);
        assert(!key.empty());
        auto valstr = uri.substr(sep_index + 1);
        assert(!valstr.empty());
        //std::cout << key << " " << val << "\n";
        
        auto size = valstr.length() + 1;
        Cache::val_type val = strcpy(new char[size], valstr.c_str());
        assert(size > 0);
        
        {
            std::unique_lock lock(mutex_);
            cache_->set(key, val, size);
        }
        assert(size > 0);
        res.result(http::status::created);
        res.set(http::field::content_type, "application/json"); 
        beast::ostream(res.body())
            << "{\n"
            << "    \"key\": \"" << key << "\",\n" 
            << "    \"value\": \"" << val << "\"\n"
            << "}\n";
        return send(std::move(res));
    }


        // Respond to Post request
    if (req.method() == http::verb::delete_)
    {

        auto uri = std::string(req.target());
        assert(uri.length() > 1);
        //remove the / in the uri
        auto key = uri.substr(1);
        bool deleted;
        {
            std::shared_lock lock(mutex_);
            deleted = cache_->del(key);
        }

        res.set(http::field::version, "1.1");
        res.set(http::field::content_type, "application/json");\
        if(deleted)
        {
            beast::ostream(res.body()) << "true\n";
            return send(std::move(res));
        }
        else
        {
            beast::ostream(res.body()) << "false\n";
            return send(std::move(res));
        }
    }

    if (req.method() == http::verb::head)
    {

        size_t space_used;
        {
            std::shared_lock lock(mutex_);
            space_used = cache_->space_used();
        }
        
        res.set(http::field::content_type, "application/json");
        res.result(http::status::accepted);
        //res.insert("{\"Space-Used\"", std::string("\"") + std::to_string(space_used) + std::string("\"}"));
        res.insert("Space-Used",  std::to_string(space_used));
        return send(std::move(res));

    }

    // Respond to Post request
    if (req.method() == http::verb::post)
    {

        auto uri = std::string(req.target());
        assert(uri.length() > 1);
        //remove the / in the uri
        auto resource = uri.substr(1);
        if (resource == "reset")
        {
            {
                std::unique_lock lock(mutex_);
                cache_->reset();
            }
            res.set(http::field::content_type, "application/json");
            res.result(http::status::ok);
            res.set(http::field::version, "1.1");
            beast::ostream(res.body()) << "Cache Reset" << "\n";
            return send(std::move(res));
        }
        else
        {
            res.result(http::status::not_found);
            res.set(http::field::version, "1.1");
            res.set(http::field::content_type, "application/json");\
            beast::ostream(res.body()) << "Incorrect Target reset not performed." << "\n";
            return send(std::move(res));
        }
    }
}   
//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit
        send_lambda(session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                beast::bind_front_handler(
                    &session::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

public:
    // Take ownership of the stream
    session(
        tcp::socket&& socket,
        std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket))
        , doc_root_(doc_root)
        , lambda_(*this)
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(
                          &session::do_read,
                          shared_from_this()));
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return fail(ec, "read");

        // Send the response
        handle_request(*doc_root_, std::move(req_), lambda_);
    }

    void
    on_write(
        bool close,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> const& doc_root)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , doc_root_(doc_root)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(
                std::move(socket),
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
     using server_t = std::string;
    using port_t = short unsigned;

    const size_t DEFAULT_MAX_MEM = 4000000;
    const int DEFAULT_THREAD_COUNT = 1;
    const float LOAD_FACTOR = 0.75;
    //consider adding a default port number as well.

    size_t max_mem = DEFAULT_MAX_MEM;
    int thread_count = DEFAULT_THREAD_COUNT;
    server_t server = "127.0.0.1";
    port_t port_arg = 8080;
    float load_factor = LOAD_FACTOR;

    /*
    -m maxmem in bytes
    -s server
    -p port
    -t thread_count
    */
    int opt;
    while ((opt = getopt(argc, argv, "m:s:p:t:l:")) != -1)
    {
        switch(opt)
        {
            case 'm':
            {
                max_mem = atoi(optarg);
            }break;

            case 's':
            {
                server = optarg;  
            }break;

            case 'p':
            {
                port_arg = atoi(optarg);
            }break;

            case 't':
            {
                thread_count = atoi(optarg);
            }break;

            case 'l':
            {
                load_factor = atof(optarg);
            }break;

            default:
            {
                std::cout << "invalid parameters";
                exit(EXIT_FAILURE);
            }
        }
    }
    std::cout << "-------------------------------------------\n";
    std::cout << "arguments set:\n";
    std::cout << "-------------------------------------------\n";
    std::cout << "maxmem: " << max_mem << "\n";
    std::cout << "server: " << server << "\n";
    std::cout << "port: " << port_arg << "\n";
    std::cout << "thread_count: " << thread_count << "\n";
    std::cout << "load factor: " << load_factor << "\n";
    std::cout << "-------------------------------------------\n";

    LRUEvictor evictor;


    cache_ = new Cache(max_mem, load_factor, &evictor);
    auto const address = net::ip::make_address(server);
    unsigned short port = static_cast<unsigned short>(port_arg);
    auto const doc_root = std::make_shared<std::string>("");

    // The io_context is required for all I/O
    net::io_context ioc{thread_count};

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port},
        doc_root)->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(thread_count - 1);
    for(auto i = thread_count - 1; i > 0; --i)
    {
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    }
    ioc.run();

    return EXIT_SUCCESS;
}