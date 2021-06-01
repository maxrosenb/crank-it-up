#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include "cache.hh"
#include "evictor.hh"
#include "fifo_evictor.hh"
#include "lru_evictor.hh"
#include <iostream>
#include <assert.h> 
#include <cstring>
#include <numeric>
#include <cmath>
#include <string>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <memory>
#include <thread>
#include <sstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include "generator.hh"

#include <utility>
#include <future>
#include <thread>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

using data_t = double;

void data_to_csv(std::vector<data_t> data)
{
    std::ofstream output_file;
    output_file.open("nreqs.csv");

    for (unsigned i = 0; i < data.size(); i++)
    {
        output_file << i << "," << data[i] << "\n";
    }

    output_file.close();
}

int create_cdf_graph(std::vector<data_t> data)
{
    std::sort(data.begin(), data.end());
    std::ofstream output_file;
    output_file.open("nreqs.csv");

    for (unsigned i = 0; i < data.size(); i++)
    {
        output_file << data[i] << "," << double(i)/double(data.size())<< "\n";
    }
    output_file.close();
    return system("gnuplot baseline_plot.p");
}

int create_95th_graph(std::vector<double> ninety_fifths)
{
    std::ofstream output_file;
    output_file.open("percents.csv");
    for (unsigned i = 0; i < ninety_fifths.size(); i++){
        output_file << i+1 << "\t" << ninety_fifths[i] << "\n";

    }
    output_file.close();
    return system("gnuplot percents.p");
    
}

int create_throughput_graph(std::vector<double> throughputs)
{
    std::ofstream output_file;
    output_file.open("throughputs.csv");
    for (unsigned i = 0; i < throughputs.size(); i++){
        output_file << i+1 << "\t" << throughputs[i] * (i+1)<< "\n";

    }
    output_file.close();
    return system("gnuplot throughputs.p");
    
}



class Driver
{
   
    private:
    double num_requests = 0;
    double num_hits = 0;
    Request_Generator* request_gen;
    std::mutex rg_mutex;
    std::mutex beast_mutex;

   
    public:
    Driver(Request_Generator* rg)
    {
        request_gen = rg;
    }

    void warm_cache(unsigned num_to_put)
    {

        for (unsigned num_put = 0; num_put < num_to_put; num_put++)
        {
            rg_mutex.lock();
            http::request<http::string_body> request = request_gen->generate_request();
            rg_mutex.unlock();
            if (request.method() == http::verb::put)
            {
                num_put += 1;
                send_request(request);

                if (num_put % 5000 == 0)
                {
                    std::cout << "added " << num_put << " items to the cache in warmup mode.\n";
                }
            }

        }
       
    }

    double get_hit_rate()
    {
        return num_hits/num_requests;
    }



    std::pair<double, double> baseline_performance(unsigned nthreads, unsigned nreq)
    {
        auto measurements = baseline_latencies(nreq);


        

        //create graph
        create_cdf_graph(measurements);
        

        auto m_copy {measurements};

        // calculating the 95th percentile
        std::sort(m_copy.begin(), m_copy.end());
        unsigned percentile_index = std::round(double(m_copy.size()) * 0.95);
        double percentile_latency = m_copy[percentile_index];
        // calculating the mean throughput
        double latency_sum = std::accumulate(measurements.begin(), measurements.end(), 0);
        double average_latency = latency_sum/double(measurements.size());
        double mean_throughput = 1000000.0/average_latency;
        mean_throughput *= nthreads;

        return std::pair<double, double> {percentile_latency, mean_throughput};

    }

    std::future<std::vector<double>> encapsulated_baseline_latencies(unsigned nreq)
    {

        return std::async(&Driver::baseline_latencies, this, nreq);
    }

    std::pair<double, double> threaded_performance(unsigned nthreads, unsigned nreq)
    {
        unsigned requests_per_thread = nreq/nthreads;
        std::vector<std::future<std::vector<double>>> future_results;
        for (unsigned i = 0; i < nthreads; i++)
        {
            future_results.push_back(encapsulated_baseline_latencies(requests_per_thread));
        }
        std::cout << "all threads started\n"; 
        std::vector<double> measurements;
        for (unsigned i = 0; i < nthreads; i++)
        {
            auto thread_latencies = (future_results[i]).get();
            std::cout << "pushed back " << i << "\n";;
            for (unsigned j = 0; j < requests_per_thread; j++)
            {
                measurements.push_back(thread_latencies[j]);
            }
        }

        create_cdf_graph(measurements);

        auto m_copy {measurements};

        // calculating the 95th percentile
        std::sort(m_copy.begin(), m_copy.end());
        unsigned percentile_index = std::round(double(m_copy.size()) * 0.95);
        double percentile_latency = m_copy[percentile_index];
        // calculating the mean throughput
        double latency_sum = std::accumulate(measurements.begin(), measurements.end(), 0);
        std::cout << "latency_sum: " << latency_sum << "\n";
        double average_latency = latency_sum/double(measurements.size());
        double mean_throughput = 1000000.0/average_latency;

        return std::pair<double, double> {percentile_latency, mean_throughput};

    }

    std::vector<double> baseline_latencies(unsigned nreq)
    {
        std::vector<double> measurements {};

        for (unsigned i = 0; i < nreq; i++)
        {
            rg_mutex.lock();
            http::request<http::string_body> request = request_gen->generate_request();
            rg_mutex.unlock();
            //std::cout <<"sending "<< request << "\n";
            std::pair<http::response<http::dynamic_body>,float> response_pair = send_request(request);

            num_requests += 1;
            if (request.method() == http::verb::get)
            {
                
                if (!(response_pair.first.result() == http::status::not_found))
                {
                    num_hits += 1;
                }
            }

            auto response = response_pair.first;
            auto response_time = response_pair.second;
            //std::cout << response << "\n";
            //std::cout << "TIME ELAPSED: " << response_time << "us \n";
            measurements.push_back(response_time);
            

            if (i % 5000 == 0)
            {
                std::cout << "simulated " << i << " requests\n";
            }

        }
        //std::cout << "---------------------------------\n";
        return measurements;
    }

    std::pair<http::response<http::dynamic_body>,float> send_request(http::request<http::string_body> request)
    {
        net::io_context ioc;
        tcp::resolver resolver {ioc};
        beast::tcp_stream stream {ioc};
        beast::flat_buffer buffer;
        std::string host = "127.0.0.1";
        std::string  port = "8080";
        auto const results = resolver.resolve(host, port);
        try 
        {
            stream.connect(results);
        }
        catch (boost::system::system_error const& e)
        {
            std::cout << "WARNING: Could not connect : " << e.what() << std::endl;
        }

        request.set(http::field::host, "127.0.0.1");
        // Send the HTTP request to the remote host
        auto t1 = std::chrono::high_resolution_clock::now();
        
        http::write(stream, request);

        // Declare a container to hold the response
        http::response<http::dynamic_body> response;

        // Receive the HTTP response
        http::read(stream, buffer, response);
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration = static_cast<float> (std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count());
        return std::make_pair(response,duration);
    }

};


int main(int argc, char* argv[])
{

    const double DEFAULT_PERCENT_GET = 0.60;
    const double DEFAULT_PERCENT_SET = 0.30;
    const double DEFAULT_PERCENT_DELETE = 0.10;
    const unsigned DEFAULT_NUM_WARM = 0;
    const unsigned DEFAULT_NUM_BENCH = 100;
    const unsigned DEFAULT_NUM_THREADS = 2;

    double percent_get = DEFAULT_PERCENT_GET;
    double percent_set = DEFAULT_PERCENT_SET;
    double percent_delete = DEFAULT_PERCENT_DELETE;
    auto num_warm = DEFAULT_NUM_WARM;
    auto num_bench =  DEFAULT_NUM_BENCH;
    auto num_threads = DEFAULT_NUM_THREADS;


    int opt;
    while ((opt = getopt(argc, argv, "g:p:d:w:b:t:")) != -1)
    {
        switch(opt)
        {
            case 'g':
            {
                percent_get = double(atof(optarg));
            }break;

            case 'd':
            {
                percent_delete = double(atof(optarg));
            }break;

            case 'w':
            {
                //the number of warm up requests only includes puts
                num_warm = unsigned(atoi(optarg));
            }break;

            case 'b':
            {
                //the number of benchmarking requests includes get put delete
                num_bench = unsigned(atoi(optarg));
            }break;

            case 'p':
            {
                percent_set = double(atof(optarg));
            }break;
            case 't':
            {
                num_threads = unsigned(atoi(optarg));
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
    std::cout << "percent_get: " << percent_get << "\n";
    std::cout << "percent_set: " << percent_set << "\n";
    std::cout << "percent_delete: " << percent_delete << "\n";
    std::cout << "num warmup put requests: " << num_warm << "\n";
    std::cout << "num benchmarking requests: " << num_bench << "\n";
    std::cout << "num client threads: " << num_threads << "\n";
    std::cout << "-------------------------------------------\n";

    Request_Generator Request_Generator {percent_get, percent_set, percent_delete};
    Request_Generator.print_params();
    Driver driver {&Request_Generator};
    driver.warm_cache(num_warm);
    std::vector <double> ninety_fifths;
    std::vector <double> throughputs;
    for(unsigned n = 1; n <= num_threads; n++){
        auto stats = driver.threaded_performance(n, num_bench);
        ninety_fifths.push_back(stats.first);
        throughputs.push_back(stats.second);
        std::cout << "hit rate: " << driver.get_hit_rate() << "\n";
        std::cout << "95th percentile latency in us: " << stats.first << "\n"
                << "average throughput per second: " << stats.second << "\n";

    }
    std::cout << "NINETY FIFTHS SIZE " << ninety_fifths.size() << "\n";
    create_95th_graph(ninety_fifths);
    create_throughput_graph(throughputs);
}