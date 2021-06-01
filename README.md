# Crank It Up - Kai Pinckard & Max Rosenberg
In this project, we attempt to maximize our cache server's performance with multithreading.

## Part 1: multi-threaded Benchmark
Our first step was to implement multithreading into the client. To do this, we encapsulated ```baseline_latencies``` into ```encapsulated_baseline_latencies```. We then created a wrapper function ```threaded_performance(unsigned nthreads, unsigned nreqs)``` which runs ```encapsulated_baseline_latencies``` using nthreads threads. This function computes and returns a pair containing the 95th percentile latency and the throughput. The client then produces a graph displaying these statistics and how they change as nthreads increases. Before we added multi threading to the server as well as to the client, the 95th percentile latency and throughput graph looked like this:

![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputs.png)

After adding 2 threads the throughput generally decreases with increases in the number of threads, ignoring other potential performance impactors. This is because each thread must wait for other threads to stop accessing data that is protected by a mutex lock. Thus, the saturation point is 2 threads with a throughput of 21,042 requests per second and a 95th percentile latency of 147us.

## Part 2: multithreaded server
After we added support for multithreading to the server we ran the same benchmarks again for different server thread counts.

Note: All graphs were produced while running the client and the server on a single machine. Thus, the dramatic decrease in performance by adding more threads can be explained by server and client threads competing with each other for CPU time. 

### Server with Two Threads:
![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputs2tserver.png)

With 2 server threads we see that the saturation point for client threads remained at 2. The throughput however, decreased to 20,440 and the 95th percentile latency increased to 182us. 

### Server with Four Threads:
![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputs4tserver.png)

With 4 server threads we see that the saturation point is shifted to 8 threads. The throughput at the saturation point decreased to 17,662 and the 95th percentile latency increased to 997us. 

## Part 3: Optimization 

In an attempt to improve the performance of the key-value store a several optimizations were made. First, in order to address the issue of server threads having to wait access to the lock guarded cache, shared_locks were used for read only cache operations. Furthermore, the cache was modified to no longer create a std::string object each time it gets a value from the cache, instead it works with the cstring directly. Additionally, the server was reorganized so that the method types are checked for in the order of most common to least common, thereby removing many unnecessary comparisons. 

### Server with One Thread
![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputsi0.png)

The saturation point with only one server thread occured at 2 client threads. At this point, the througput was 22,835 requests per second and the 95th percentile latency was 136 us. An 8.5 percent improvement on the baseline single threaded throughput and a 7.5% reduction in 95th percentile latency. 

### Server with Two Threads
![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputsi2.png)

The saturation point remained at two client threads when the server was run with only two threads. At the saturation point the throughput was 22,015 requests per second and the 95th percentile latency was 111us. 

### Server with Four Threads
![alt text](https://github.com/kai-pinckard/crank-it-up/blob/master/throughputsi.png)

After adding the shared locks the saturation point has shifted to about 7 threads, however performance is still worse than the baseline because the client threads are competing with the 4 server threads. Although performance does not appear to have noticably improved by adding the shared lock, if the client and the server were run on separate computers the client would be able to make more requests and the unique locks would have become more of a bottleneck. The throughput at this point was 17,178 requests per second, while the 95th percentile latency was 785.



