#include "cache.hh"
#include "evictor.hh"
#include "fifo_evictor.hh"
#include "lru_evictor.hh"
#define CATCH_CONFIG_MAIN
#include "catch.hh"
#include <iostream>
#include <assert.h> 
#include <cstring>
#include <memory>

//CACHE TESTS
SCENARIO("the cache is being used", "[set, get, del]" ) {

    GIVEN("cache (maxmem=400, max_load_factor=0.75) with objects size 100") {

        unsigned test_size = 3;
        const auto max_mem = 400;
        float load_factor = 0.75;
        Cache cache("127.0.0.1", "8080");
        
        WHEN("a single thing is set and accessed") {
            cache.set("b", "v2", test_size);
            const char* test_val = cache.get("b", test_size);

            THEN("that thing can be retrieved with get"){
                REQUIRE(strcmp(test_val, "v2")==0); 
            }     
        }
        WHEN("the cache will try to store items exceeding max_mem") {
            cache.set("b", "v2", test_size); 
            cache.set("d", "v3", test_size);
            cache.set("f", "v4", test_size);
            cache.set("g", "v5", test_size);
            cache.set("h", "v6", test_size);

            THEN("max_mem is not exceeded") {
                REQUIRE(cache.space_used() <= max_mem);
            }
        }

        WHEN("an object is set but that object's key is already in the cache") {
            cache.set("x", "v1", test_size); 
            cache.set("x", "v2", test_size);
            const char* test_val = cache.get("x", test_size);

            THEN("The value at that key is overwritten"){
                REQUIRE(strcmp(test_val,"v2") == 0);
            }
        }

        WHEN("Multiple objects are set and one is retrieved"){
            cache.set("k1", "v1", test_size); 
            cache.set("k2", "v2", test_size);
            cache.set("k3", "v3", test_size); 
            cache.set("k4", "v4", test_size);
            cache.set("k5", "v5", test_size);
            cache.set("k6", "v6", test_size);
            cache.set("k7", "v7", test_size);
            auto test_val = cache.get("3", test_size);
            THEN("The correct thing is accessed"){
                REQUIRE(test_val == nullptr);
            }
        }

        WHEN("something is deleted and user attempts to get that key") {
            cache.set("xyz", "v1", test_size);
            cache.del("xyz");
            const auto test_val = cache.get("xyz", test_size);

            THEN("that thing is deleted and so cannot be accessed by user") {
                REQUIRE(test_val  == nullptr);
            }
        }

        WHEN("Overwriting a key's value with the same value"){
            cache.set("zyx", "v1", test_size);
            auto cache_size_before = cache.space_used();
            cache.set("zyx", "v1", test_size);
            auto cache_size_after = cache.space_used();

            THEN("space used does not increase."){
                REQUIRE(cache_size_before == cache_size_after);
            }
        }

        WHEN("delete is attempted on empty cache"){
            auto cache_size_before = cache.space_used();
            cache.del("xyz");
            auto cache_size_after = cache.space_used();

            THEN("cache_size does not change."){
                REQUIRE(cache_size_before == cache_size_after);
            }
        }

        WHEN("an item is deleted"){
            cache.set("be", "v1", test_size);
            cache.set("de", "v21", test_size);
            auto cache_size_before = cache.space_used();
            cache.del("be");
            auto cach_size_after = cache.space_used();
            THEN("space used is decreased by the size of the item."){
                REQUIRE(cache_size_before-test_size == cach_size_after);
            }
        }
        WHEN("cache is used and then reset"){
            cache.set("aa", "v3", test_size);
            cache.set("ba", "v4", test_size);
            cache.set("ca", "v5", test_size);
            cache.reset();
            const unsigned NUM_KEYS = 3;
            const key_type test_keys[NUM_KEYS] = { "aa", "ba", "ca"};
            bool nothing_in_cache = true;
            for (auto test_key : test_keys){
                if (cache.get(test_key, test_size) != nullptr){
                    nothing_in_cache = false;
                }
            }
            THEN("Nothing can be retrieved, space_used == 0"){
                REQUIRE(nothing_in_cache);
            }
            THEN("No space is used"){
                REQUIRE(cache.space_used() == 0);
            }

        }

        WHEN("space_used is called on an empty cache"){
            const auto cache_size = cache.space_used();
            THEN("Cache size is 0"){
                REQUIRE(cache_size == 0);
            }
        }

        WHEN("one thing is set and space_used is called"){;
            cache.set("aaa", "v1", test_size);
            THEN("Cache doesn't break"){
                REQUIRE(cache.space_used() == test_size);
            }
        }
    }
}









