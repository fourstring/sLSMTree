//
// Created by fourstring on 2020/4/18.
//
#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <sys/unistd.h>
#include <ctime>
#include "test.h"

using std::cout, std::cin, std::endl;

class PerformanceTest : public Test {
private:
    const uint64_t SIMPLE_TEST_MAX = 512;
    const uint64_t LARGE_TEST_MAX = 1024 * 64;

    void regular_test(uint64_t max) {
        uint64_t i;

        // Test a single key
        EXPECT(not_found, store.get(1));
        store.put(1, "SE");
        EXPECT("SE", store.get(1));
        EXPECT(true, store.del(1));
        EXPECT(not_found, store.get(1));
        EXPECT(false, store.del(1));

        phase();

        // Test multiple key-value pairs
        for (i = 0; i < max; ++i) {
            store.put(i, std::string(i + 1, 's'));
            EXPECT(std::string(i + 1, 's'), store.get(i));
        }
        phase();

        // Test after all insertions
        for (i = 0; i < max; ++i)
            EXPECT(std::string(i + 1, 's'), store.get(i));
        phase();

        // Test deletions
        for (i = 0; i < max; i += 2)
            EXPECT(true, store.del(i));

        phase();

        for (i = 0; i < max; ++i)
            EXPECT((i & 1) ? std::string(i + 1, 's') : not_found,
                   store.get(i));

        phase();

        for (i = 1; i < max; ++i)
            EXPECT(i & 1, store.del(i));

        phase();

        report();
    }

    void print_timespec(timespec *t) {
        cout << t->tv_sec << ' ' << t->tv_nsec << endl;
    }

    void small_test() {
        uint64_t i;
        auto put_ps = 0, get_ps = 0, delete_ps = 0;
        auto max = 40000;
        auto start = new timespec{};
        auto end = new timespec{};
        clock_gettime(CLOCK_MONOTONIC, start);
        for (i = 0; i < max; ++i) {
            store.put(i, std::string(i + 1, 's'));
        }
        clock_gettime(CLOCK_MONOTONIC, end);
        print_timespec(start);
        print_timespec(end);
        phase();

        // Test after all insertions
        clock_gettime(CLOCK_MONOTONIC, start);
        for (i = 0; i < max; ++i)
            EXPECT(std::string(i + 1, 's'), store.get(i));
        clock_gettime(CLOCK_MONOTONIC, end);
        print_timespec(start);
        print_timespec(end);
        phase();

        // Test deletions
        clock_gettime(CLOCK_MONOTONIC, start);
        for (i = 0; i < max; i += 2)
            EXPECT(true, store.del(i));
        clock_gettime(CLOCK_MONOTONIC, end);
        print_timespec(start);
        print_timespec(end);
        phase();

    }

    void long_test() {
        auto max = 10000000;
        auto put_ps = 0;
        auto start_time = time(nullptr);
        auto cur_time = start_time;
        auto new_time = start_time;
        auto os = std::ofstream{"long_test.txt"};
        for (int i = 0; i < max; i++) {
            store.put(i, std::string(10000, 's'));
            put_ps++;
            new_time = time(nullptr);
            if (new_time - cur_time == 1) {
                os << new_time - start_time << ' ' << put_ps << '\n';
                put_ps = 0;
                cur_time = new_time;
            }
        }
    }

public:
    PerformanceTest(const std::string &dir, bool v = true) : Test(dir, v) {
    }

    void start_test(void *args = NULL) override {
        std::cout << "KVStore Performance Test" << std::endl;

        std::cout << "[Long Test]" << std::endl;
        long_test();
    }
};

int main(int argc, char *argv[]) {
    bool verbose = (argc == 2 && std::string(argv[1]) == "-v");

    std::cout << "Usage: " << argv[0] << " [-v]" << std::endl;
    std::cout << "  -v: print extra info for failed tests [currently ";
    std::cout << (verbose ? "ON" : "OFF") << "]" << std::endl;
    std::cout << std::endl;
    std::cout.flush();

    PerformanceTest test("./data", verbose);

    test.start_test();

    return 0;
}

