
#include <iostream> 
#include <vector>
#include <random>
#include <fstream>
#include <numeric>
#include <chrono>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <random>
#include "generator.hh"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

std::string Key_Generator::generate_key_(unsigned length)
{   
    std::string new_key;

    for(unsigned i = 0; i < length; i++){
        new_key += ALPHANUMERIC[a_dist(gen)];
    }

    return new_key;
}

unsigned Key_Generator::generate_key_length_()
{
    return unsigned(dist(gen));
}


std::vector<std::string> Key_Generator::generate_key_pool_(){
    std::vector<std::string> res;
    for(unsigned i = 0; i < KEY_POOL_SIZE; i++){
        res.push_back(generate_key_(generate_key_length_()));
    }
    return res;
}

std::string Key_Generator::get_key_from_pool_(){
    std::uniform_int_distribution<> distribution1(0, KEY_POOL_SIZE - 1);
    return key_pool[distribution1(gen)];
}
std::string Key_Generator::generate_key()
{
    return get_key_from_pool_();
}

void Key_Generator::add_key_to_cache(std::string key){
    key_cache.push_back(key);
    //std::cout << "ADDED KEY " << key << "\n";


}

bool Key_Generator::is_key_in_cache(std::string key){

    if (std::count(key_cache.begin(),key_cache.end(),key))
    {
        return true;
    }
    else
    {
        return false;
    }

}

std::string Key_Generator::get_key_from_cache(){
    auto cache_size = static_cast<int> (key_cache.size());
    auto rand_idx = rand() % cache_size;
    //std::cout << "\n Index of key retrieved from key_cache: " << rand_idx << " key: " << key_cache[rand_idx] << "\n";
    return key_cache[rand_idx];
}

unsigned Key_Generator::get_key_cache_length(){
    return key_cache.size();
}

void Key_Generator::delete_key_from_cache(std::string key){
    key_cache.erase(std::remove(key_cache.begin(), key_cache.end(), key), key_cache.end());
}
unsigned Value_Generator::generate_value_length_()
{
    return unsigned(dist(gen));
}

    std::string Value_Generator::generate_value_(unsigned length)
{   
    std::string new_val;
    unsigned length_to_use = (length % 500) + 1;


    for(unsigned i = 0; i < length_to_use; i++){
        new_val += ALPHANUMERIC[a_dist(gen)];
    }
    return new_val;
}

std::string Value_Generator::generate_value()
{
    return generate_value_(generate_value_length_());
}

double Method_Generator::generate_probability()
{  
return dist(gen);
}

std::string Method_Generator::generate_method()
{ 
    auto probability = Method_Generator::generate_probability();
    if (probability <= get_cutoff)
    {
        return std::string("GET");
    }
    else if (probability <= set_cutoff)
    {
        return std::string("PUT");
    }
    else
    {
        return std::string("DELETE");
    }
}

double Request_Generator::generate_probability()
{  
return dist(gen);
}

http::request<http::string_body> Request_Generator::generate_request()
{
    
    std::string target = "/";
    auto version = 11;
    std::string method = method_generator_.generate_method();
    std::string random_key = key_generator_.generate_key();
    

    if (method == "GET")
    {   
        if(key_generator_.get_key_cache_length() > 0){
            //Pick a known key 80% of the time, 20% of the time totally random
            auto probability = generate_probability();
            //std::cout << "PROBABILITY: " << probability << "\n";
            if (probability <= known_cutoff){
                target = key_generator_.get_key_from_cache();
                //std::cout << "TARGET: " << target << "\n";
                http::request<http::string_body> req{http::verb::get, target, version};
                return req;
            }   
            
        }
        target = "/" + random_key;
            http::request<http::string_body> req{http::verb::get, target, version};
            return req;
        return req;
        
    }
    else if (method == "PUT")
    {
        std::string value = value_generator_.generate_value();
        target = "/"+ random_key + "/" + value;
        std::string key_for_cache = "/" + random_key;
        http::request<http::string_body> req{http::verb::put, target, version};
         if (key_generator_.is_key_in_cache(key_for_cache) == false){
            key_generator_.add_key_to_cache(key_for_cache);
        }
        return req;

        //if not in key cache, add to key cache
       
    }
    else if (method == "DELETE")
    {
        target = "/" + random_key;
        if(key_generator_.get_key_cache_length() > 0){
            //Pick a known key 80% of the time, 20% of the time totally random
            auto probability = generate_probability();
            if (probability <= known_cutoff){
                auto key = key_generator_.get_key_from_cache();
                key_generator_.delete_key_from_cache(key);
            }
        }
        http::request<http::string_body> req{http::verb::delete_, target, version};
        return req;
    }
    else
    {
        std::cout << "error invalid method\n";
        http::request<http::string_body> default_req;
        return default_req;
    }
    
}


