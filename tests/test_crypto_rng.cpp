// tests/test_crypto_rng.cpp
// Unit tests for the crypto_rng module. Exit code 0 = success (that is what ctest checks).
// CHECK does not rely on assert(), so the tests still work if NDEBUG is defined.
 
#include "crypto_rng.h"
 
#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
 
using namespace crypto;
 
static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "FAILED (line " << __LINE__ << "): " #cond << "\n";          \
            failures++;                                                               \
        }                                                                             \
    } while (0)
 
static std::string to_hex(const Bytes& b) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    for (uint8_t x : b) {
        s += digits[x >> 4];
        s += digits[x & 15];
    }
    return s;
}
 
int main() {
    // 1. SHAKE256 known answers (reference values computed with Python's hashlib.shake_256)
    CHECK(to_hex(shake256({}, 32)) ==
          "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f");
    CHECK(to_hex(shake256({Bytes{'a', 'b', 'c'}}, 32)) ==
          "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739");
 
    // 2. a longer output extends a shorter one (the property ShakeStream relies on)
    const Bytes m(32, 0x42);
    Bytes shorter = shake256({m}, 64);
    Bytes longer = shake256({m}, 300);
    CHECK(std::equal(shorter.begin(), shorter.end(), longer.begin()));
 
    // 3. hashing several parts is the same as hashing their concatenation
    const Bytes x(10, 1), y(20, 2);
    Bytes xy = x;
    xy.insert(xy.end(), y.begin(), y.end());
    CHECK(shake256({x, y}, 32) == shake256({xy}, 32));
 
    // 4. different domain-separation tags give different outputs
    CHECK(shake256({Bytes{0}, m}, 32) != shake256({Bytes{1}, m}, 32));
 
    // 5. ShakeStream gives the same bytes as one big SHAKE256, across refill boundaries
    const Bytes seed(32, 7);
    {
        ShakeStream st(0x00, seed);
        Bytes got;
        for (int chunk : {1, 7, 100, 5000, 9000, 3}) {
            Bytes c = st.bytes(static_cast<size_t>(chunk));
            got.insert(got.end(), c.begin(), c.end());
        }
        CHECK(got == shake256({Bytes{0x00}, seed}, got.size()));
    }
 

    // 6. sample_distinct: determinism, size, range, distinctness
    {
        ShakeStream s1(0x10, seed), s2(0x10, seed), s3(0x10, Bytes(32, 8));
        std::vector<int> p1 = sample_distinct(s1, 24646, 134);
        std::vector<int> p2 = sample_distinct(s2, 24646, 134);
        std::vector<int> p3 = sample_distinct(s3, 24646, 134);
        CHECK(p1 == p2);  // same seed, same positions
        CHECK(p1 != p3);  // different seed, different positions
        CHECK(p1.size() == 134);
 
        std::vector<int> sorted = p1;
        std::sort(sorted.begin(), sorted.end());
        CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
        CHECK(sorted.front() >= 0 && sorted.back() < 24646);
    }
 
    // 7. edge cases of sample_distinct
    {
        ShakeStream s(0x11, seed);
        CHECK(sample_distinct(s, 100, 0).empty());
 
        std::vector<int> all = sample_distinct(s, 5, 5);  // a random permutation of 0..4
        std::sort(all.begin(), all.end());
        CHECK((all == std::vector<int>{0, 1, 2, 3, 4}));
    }
 
    // 8. uniform_int: n = 1 is always 0, and n = 10 is roughly uniform
    {
        ShakeStream s(0x20, seed);
        bool all_zero = true;
        for (int i = 0; i < 100; i++) all_zero = all_zero && (uniform_int(s, 1) == 0);
        CHECK(all_zero);
 
        const int N = 100000;
        int counts[10] = {0};
        for (int i = 0; i < N; i++) counts[uniform_int(s, 10)]++;
        for (int c : counts) CHECK(std::abs(c - N / 10) < 500);  // about 5 standard deviations
    }
 
    // 9. OsRng: two draws differ and are not all zero
    {
        OsRng os;
        Bytes r1 = os.bytes(32), r2 = os.bytes(32);
        CHECK(r1 != r2);
        CHECK(r1 != Bytes(32, 0));
    }
 
    if (failures == 0) std::cout << "crypto_rng: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
