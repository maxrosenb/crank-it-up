
#include <iostream> 
#include <vector>
#include <random>
#include <fstream>
#include <numeric>
#include <chrono>
#include <math.h>
#include <assert.h>
#include <random>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>


class Key_Generator
{
    private:
    const int SEED = 101;
    const double LOCATION = 30.7984;
    const double SCALE = 8.20449;

    std::vector<std::string> key_cache;
    
    
    std::mt19937 gen{SEED};
    std::extreme_value_distribution<double> dist {LOCATION, SCALE};
    std::uniform_int_distribution<> key_cache_dist {0, static_cast<int> (key_cache.size())};

    const std::string ALPHANUMERIC ="0123456789abcdefghijklmnopqrstuwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::uniform_int_distribution<> a_dist {0, int(ALPHANUMERIC.size() - 1)};
    //change this to use modulus instead of using so many distribution
    std::uniform_int_distribution<> distribution1 {0, int(KEY_POOL_SIZE - 1)};


    
    unsigned KEY_POOL_SIZE = 10000;
    std::vector<std::string> key_pool = generate_key_pool_();
    
    unsigned generate_key_length_();

    std::string generate_key_(unsigned length);

    std::vector<std::string> generate_key_pool_();
    
    std::string get_key_from_pool_();

    

    public:
    std::string generate_key();
    unsigned get_key_cache_length();
    std::string get_key_from_cache();
    void delete_key_from_cache(std::string key);
    void add_key_to_cache(std::string key);
    bool is_key_in_cache(std::string key);
};

class Value_Generator
{
    private:
    const int SEED = 101;
    const double LOCATION = 214.476;
    const double SCALE = 200;
    
    std::mt19937 gen{SEED};
    std::extreme_value_distribution<double> dist {LOCATION, SCALE};

    const std::string ALPHANUMERIC ="0123456789abcdefghijklmnopqrstuwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::uniform_int_distribution<> a_dist {0, int(ALPHANUMERIC.size() - 1)};
    
    unsigned generate_value_length_();

    std::string generate_value_(unsigned length);

    public:
    std::string generate_value();
};

class Method_Generator
{
    private:
    const int SEED = 101;
    
    double PERCENT_GET = 0.60;
    double PERCENT_SET = 0.30;
    double PERCENT_DELETE = 0.10;
    
    double sum = PERCENT_GET + PERCENT_SET + PERCENT_DELETE;

    double get_cutoff = PERCENT_GET;
    double set_cutoff = get_cutoff + PERCENT_SET;
    double delete_cutoff = set_cutoff + PERCENT_DELETE;

    std::mt19937 gen{SEED};
    std::uniform_real_distribution<double> dist{0, 1};
    double generate_probability();
    
    
    public:
    

    std::string generate_method();


    void set_percents(double get_percent, double set_percent, double delete_percent)
    {
        PERCENT_GET = get_percent;
        PERCENT_SET = set_percent;
        PERCENT_DELETE = delete_percent;
        get_cutoff = PERCENT_GET;
        set_cutoff = get_cutoff + PERCENT_SET;
        delete_cutoff = set_cutoff + PERCENT_DELETE;
    }

    void print_percents()
    {
        std::cout << "percent get "  << PERCENT_GET << "\n";
        std::cout << "percent set " << PERCENT_SET << "\n";
        std::cout << "percent delete " << PERCENT_DELETE << "\n";
    }

};

class Request_Generator
{
    private:

    Method_Generator method_generator_{};
    Key_Generator key_generator_{};
    Value_Generator value_generator_{};
    const int SEED = 101;
    std::mt19937 gen{SEED};
    std::uniform_real_distribution<double> dist{0, 1};
    const double PERCENT_KNOWN = 0.95;
    double known_cutoff = PERCENT_KNOWN;
    double generate_probability();

    
    public:
    http::request<http::string_body> generate_request();

    Request_Generator(double percent_get, double percent_set, double percent_delete)
    {
        method_generator_.set_percents(percent_get, percent_set, percent_delete);
    }
    void print_params()
    {
        method_generator_.print_percents();
    }
};
