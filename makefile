CXX=g++-8
CXXFLAGS= -std=c++17 -O3 -Wall
LDFLAGS=$(CXXFLAGS)
LIBS=-pthread -lboost_program_options
OBJ=$(SRC:.cc=.o)
all:  cache_server cache_benchmark 

cache_server: cache_server.o lib_cache.o lru_evictor.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
cache_benchmark: cache_benchmark.o generator.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
clean:
	rm -rf *.o cache_benchmark cache_server

valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all ./cache_benchmark
	valgrind --leak-check=full --show-leak-kinds=all ./cache_server