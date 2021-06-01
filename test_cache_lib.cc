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

        unsigned test_size = 100;
        const auto max_mem = 400;
        float load_factor = 0.75;
        Cache cache("127.0.0.1", "8080");
        
        WHEN("a single thing is set and accessed") {
            cache.set("16", "val 16", test_size);
            const char* test_val = cache.get("16", test_size);

            THEN("that thing can be retrieved with get"){
                REQUIRE(strcmp(test_val, "val 16")==0); 
            }     
        }
        WHEN("the cache will try to store items exceeding max_mem") {
            cache.set("1", "val 1", test_size); 
            cache.set("2", "val 2", test_size);
            cache.set("3", "val 3", test_size);
            cache.set("4", "val 4", test_size);
            cache.set("5", "val 5", test_size);

            THEN("max_mem is not exceeded") {
                REQUIRE(cache.space_used() <= max_mem);
            }
        }

        WHEN("an object is set but that object's key is already in the cache") {
            cache.set("1", "val 1", test_size); 
            cache.set("1", "val 2", test_size);
            const char* test_val = cache.get("1", test_size);

            THEN("The value at that key is overwritten"){
                REQUIRE(strcmp(test_val,"val 2") == 0);
            }
        }

        WHEN("Multiple objects are set and one is retrieved"){
            cache.set("1", "val 1", test_size); 
            cache.set("2", "val 2", test_size);
            cache.set("3", "val 3", test_size); 
            cache.set("4", "val 4", test_size);
            cache.set("5", "val 5", test_size);
            cache.set("6", "val 6", test_size);
            cache.set("7", "val 7", test_size);
            auto test_val = cache.get("3", test_size);
            THEN("The correct thing is accessed"){
                REQUIRE(test_val == nullptr);
            }
        }

        WHEN("something is deleted and user attempts to get that key") {
            cache.set("1", "val 1", test_size);
            cache.del("1");
            const auto test_val = cache.get("1", test_size);

            THEN("that thing is deleted and so cannot be accessed by user") {
                REQUIRE(test_val  == nullptr);
            }
        }

        WHEN("Overwriting a key's value with the same value"){
            cache.set("1", "val 1", test_size);
            auto cache_size_before = cache.space_used();
            cache.set("1", "val 1", test_size);
            auto cache_size_after = cache.space_used();

            THEN("space used does not increase."){
                REQUIRE(cache_size_before == cache_size_after);
            }
        }

        WHEN("delete is attempted on empty cache"){
            auto cache_size_before = cache.space_used();
            cache.del("1");
            auto cache_size_after = cache.space_used();

            THEN("cache_size does not change."){
                REQUIRE(cache_size_before == cache_size_after);
            }
        }

        WHEN("an item is deleted"){
            cache.set("1", "val 1", test_size);
            cache.set("2", "val 21", test_size);
            auto cache_size_before = cache.space_used();
            cache.del("1");
            auto cach_size_after = cache.space_used();
            THEN("space used is decreased by the size of the item."){
                REQUIRE(cache_size_before-test_size == cach_size_after);
            }
        }
        WHEN("cache is used and then reset"){
            cache.set("3", "val 3", test_size);
            cache.set("4", "val 4", test_size);
            cache.set("5", "val 5", test_size);
            cache.reset();
            const unsigned NUM_KEYS = 3;
            const key_type test_keys[NUM_KEYS] = { "3", "4", "5"};
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
            cache.set("1", "val 1", test_size);
            THEN("Cache doesn't break"){
                REQUIRE(cache.space_used() == test_size);
            }
        }
    }
    GIVEN("cache with custom passed hash function") {
        unsigned test_size = 100; 
        const unsigned max_mem = 400;
        const unsigned load_factor = 0.75;
        std::function<std::size_t (key_type)> hasher = [](key_type key){
                return 1;
            };
        Cache cache(max_mem, load_factor, nullptr, hasher);
        WHEN("Something is set to the cache and get is called"){
            cache.set("1", "val 1", test_size);
            cache.set("2", "val 12", test_size);
            cache.set("3", "val 13", test_size);
            cache.set("4", "val 14", test_size);
            const auto test_val = cache.get("2", test_size);
            THEN("The cache retrieves the corresonding value"){
                REQUIRE(strcmp(test_val,"val 12")==0);
            }
        }
    }
}


SCENARIO("An Evictor is called", "[touch_key, evict]" ) {

    GIVEN("A Evictor") {
        FifoEvictor evictor; 
    
        WHEN("Evict is called on an empty evictor.") {
            key_type evicted_key = evictor.evict();
            THEN("The empty string is returned."){
                REQUIRE(evicted_key == "");
            }
        }
        WHEN("One item has been touched by a previously empty evictor and one item is evicted."){
            const auto test_key = "0";
            evictor.touch_key(test_key);
            key_type evicted_key = evictor.evict();
            THEN("The evicted key is the key that was stored."){
                REQUIRE(evicted_key == test_key);
            }
        }

        WHEN("The same key is touched twice by an empty evictor and then two keys are evicted."){
            const auto test_key = "23";
            evictor.touch_key(test_key);
            evictor.touch_key(test_key);
            THEN("The second key evicted is either empty str or test_key"){
                auto evicted_key1 = evictor.evict();
                auto evicted_key2 = evictor.evict();
                REQUIRE( ((evicted_key2 == "") || (evicted_key2 == test_key)) );
            }
        }

        WHEN("The key of the empty string is touched by an empty evictor and a key is evicted"){
            const auto test_key = "";
            evictor.touch_key(test_key);
            THEN("The empty string is evicted"){
                REQUIRE(evictor.evict() == "");
            }
        }

        WHEN("Five unique keys are added to an empty evictor, and evict method is called five times.") {
            const unsigned NUM_KEYS = 5;
            const key_type test_keys[NUM_KEYS] = {"0", "1", "2", "3", "4"};
            for (auto test_key : test_keys){
                evictor.touch_key(test_key);
            }

            THEN("The same five keys are evicted, possibly in a different order."){
                key_type evicted_keys[NUM_KEYS];

                //Require that evicted_key was one of the five keys touched.
                bool all_expected = true;
                for (unsigned i = 0; i < NUM_KEYS; i++){
                    auto evicted_key = evictor.evict();
                    evicted_keys[i] = evicted_key;
                    auto key_expected = std::find(std::begin(test_keys), std::end(test_keys), evicted_key) != std::end(test_keys);
                    if (!key_expected){
                        all_expected = false;
                    }
                }

                //Require that all five keys were evicted.
                bool all_unique = true;
                for (unsigned i = 0; i < NUM_KEYS-1; i++){
                    for (unsigned j = i+1; j < NUM_KEYS; j++){
                        if (evicted_keys[i] == evicted_keys[j]){
                            all_unique = false;
                        }
                    }   
                }

                //Require that the same Five keys were evicted.
                REQUIRE((all_unique && all_expected));
            }
        }
    }
}






