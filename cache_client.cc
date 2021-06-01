#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <boost/algorithm/string.hpp>
#include "cache.hh"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>


class Cache::Impl{
 private:
    // Controls the initial size of the hash map in terms of buckets.
    const unsigned DEFAULT_CAPACITY = 2;
    using triple_t = std::tuple<key_type, size_type, val_type>;
    using ptr_t = std::unique_ptr<triple_t>;
    using bucket_t = std::vector<triple_t>;
    using cache_t = std::vector<bucket_t>;
    size_type maxmem_;
    cache_t dict_;
    Evictor* evictor_;
    hash_func hasher_;
    std::string host_;
    std::string port_;
    float max_load_factor_;
    size_type space_used_ = 0;
    unsigned num_elements_ = 0;
    // The io_context is required for all I/O


 public:

Impl(size_type maxmem, float max_load_factor,
        Evictor* evictor,
        hash_func hasher)
        : maxmem_(maxmem), dict_(DEFAULT_CAPACITY), evictor_(evictor), hasher_(hasher), max_load_factor_(max_load_factor)
    {

    }

Impl(std::string host, std::string port)
        : host_(host), port_(port)
    {

        
    }

    val_type get(key_type& key, size_type& val_size) const
    {
        std::cout << "GET REQUEST" << std::endl;
        
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const target = '/' + key;
        // Look up the domain name
        auto const results = resolver.resolve(host_, port_);
 
        // Make the connection on the IP address we get from a lookup
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }
        // Set up an HTTP GET request message
        auto version = 11;
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);
        std::string s = boost::beast::buffers_to_string(res.body().data());

        std::string s2 = s.substr(0,1);

        bool should_parse_json = true;
        if(s2.compare("K") == 0){
            std::cout << "WARNING: Key not found in cache" << std::endl;
            should_parse_json = false;
        }
        // Gracefully close the socket
        beast::error_code ec;
        
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};
        // If we get here then the connection is closed gracefully

        std::stringstream ss(s); //simulating an response stream
        const unsigned int BUFFERSIZE = 256;

        //temporary buffer
        char buf[BUFFERSIZE];
        memset(buf, 0, BUFFERSIZE * sizeof(char));

        //returnValue.first holds the variables name
        //returnValue.second holds the variables value
        std::pair<std::string, std::string> returnValue;

        //read until the opening bracket appears
        

        //get response values until the closing bracket appears
        if(should_parse_json) {
            while(ss.peek() != '{')         
            {
                std::cout << ss.peek() << std::endl;
                //ignore the { sign and go to next position
                ss.ignore();
            }
            for(int i = 0; i < 2; i++)
            {
                //read until a opening variable quote sign appears
                ss.get(buf, BUFFERSIZE, '\"'); 
                //and ignore it (go to next position in stream)
                ss.ignore();

                //read variable token excluding the closing variable quote sign
                ss.get(buf, BUFFERSIZE, '\"');
                //and ignore it (go to next position in stream)
                ss.ignore();
                //store the variable name
                returnValue.first = buf;

                //read until opening value quote appears(skips the : sign)
                ss.get(buf, BUFFERSIZE, '\"');
                //and ignore it (go to next position in stream)
                ss.ignore();

                //read value token excluding the closing value quote sign
                ss.get(buf, BUFFERSIZE, '\"');
                //and ignore it (go to next position in stream)
                ss.ignore();
                //store the variable name
                returnValue.second = buf;

                //do something with those extracted values
                
                if(i == 1){
                    std::cout << "Value at key " << key << ": " << returnValue.second << std::endl;
                    const char *val = returnValue.second.c_str();
                    return val;

                }
            }
        }
        return nullptr;

        
    }


    void set(key_type key, val_type val, size_type size) const
    {
        std::cout << "SET REQUEST" << std::endl;
        
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const target = '/' + key + '/' + val;
        // Look up the domain name
        auto const results = resolver.resolve(host_, port_);
 
        // Make the connection on the IP address we get from a lookup
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }
        // Set up an HTTP PUT request message
        auto version = 11;
        http::request<http::string_body> req{http::verb::put, target, version};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        std::cout << target << std::endl;
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);
        std::string s = boost::beast::buffers_to_string(res.body().data());
        
        // Write the message to standard out

        // Gracefully close the socket
        beast::error_code ec;
        
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};
            

        // If we get here then the connection is closed gracefully

        
    }

    bool del(key_type key)
   {
       std::cout << "DELETE_ REQUEST" << std::endl;
       net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const target = '/' + key;
        // Look up the domain name
        auto const results = resolver.resolve(host_, port_);
 
        // Make the connection on the IP address we get from a lookup
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }
        // Set up an HTTP PUT request message
        auto version = 11;
        http::request<http::string_body> req{http::verb::delete_, target, version};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);
        std::string s = boost::beast::buffers_to_string(res.body().data());

        std::string s2 = s.substr(0,1);
        auto return_val = true;
        if(s2.compare("f") == 0){
            return_val = false;
        }
        
        // Write the message to standard out
        std::cout << s << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};
            

        // If we get here then the connection is closed gracefully
        return return_val;
   }

   void reset() const
    {
        std::cout << "RESET REQUEST" << std::endl;
        
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const target = "/reset";
        // Look up the domain name
        auto const results = resolver.resolve(host_, port_);
        // Make the connection on the IP address we get from a lookup
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }
        // Set up an HTTP GET request message
        auto version = 11;
        http::request<http::string_body> req{http::verb::post, target, version};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);
        

        // Gracefully close the socket
        beast::error_code ec;
        
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
        
    }
    
    size_type space_used() const
    {
        std::cout << "SPACE_USED REQUEST " << std::endl;
        
        std::cout << "SPACE_USED: ";
        
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        beast::error_code ec;
        boost::string_view const target = "/b";
        // Look up the domain name
        auto const results = resolver.resolve(host_, port_);
 
        // Make the connection on the IP address we get from a lookup
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }
        // Set up an HTTP GET request message
        auto version = 11;
        http::request<http::empty_body> req{http::verb::head, target, version};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "test");
        
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::multi_buffer buffer;

        // Declare a container to hold the response
        http::response_parser<http::empty_body> res;
        res.skip(true);
        // Receive the HTTP response
        http::read(stream, buffer, res);
        // std::cout << res.get() << std::endl;
        auto return_val = res.get()["Space-Used"].data();
        std::cout << return_val << std::endl;


        // Gracefully close the socket
        
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        //return_val = static_cast<unsigned int>(return_val);

        std::stringstream strValue;
        strValue << return_val;

        unsigned int intValue;
        strValue >> intValue;
        return intValue;

        // If we get here then the connection is closed gracefully
        
    }

    
    
};
    
        
    

// int main(int argc, char** argv)
// {
//     unsigned test_size = 100;
//     Cache cache("127.0.0.1", "8080");
//     cache.set("b","v2",test_size);
//     cache.set("c","v3",test_size);
//     cache.get("b",test_size);
//     cache.get("b",test_size);
//     cache.get("c",test_size);
//     cache.space_used();
//     cache.reset();
//     cache.space_used();
    

// }

Cache::Cache(size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        hash_func hasher)
        : pImpl_(new Impl(maxmem, max_load_factor, evictor, hasher))
        {

        }

Cache::Cache(std::string host, std::string port)
        : pImpl_(new Impl(host, port))
        {

        }

Cache::val_type Cache::get(key_type key, size_type& val_size) const
  {
      return pImpl_->get(key, val_size);
  }

void Cache::set(key_type key, val_type val, size_type size)
  {
      pImpl_->set(key, val, size);
  }

void Cache::reset()
  {
      pImpl_->reset();
  }

Cache::size_type Cache::space_used() const
  {
      return pImpl_->space_used();
  }

bool Cache::del(key_type key)
  {
    return pImpl_->del(key);
  }



Cache::~Cache(){}