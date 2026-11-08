// tests/test_decoder.cpp
// Tests of the counters, the threshold, bit_flip, decoder and bgf_decoder.
// Exit code 0 = success (that is what ctest checks).

#include <iostream>
#include <vector>

#include "crypto_rng.h"
#include "decoder.h"
#include "utils.h"

using namespace NTL;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "FAILED (line " << __LINE__ << "): " #cond << "\n";          \
            failures++;                                                               \
        }                                                                             \
    } while (0)

static crypto::ShakeStream make_stream() {
    return crypto::ShakeStream(0x20, crypto::Bytes{4, 5, 6});
}

// an error of total weight t over the 2r positions
static GF2E_bike make_error(crypto::ByteSource& src, int r0, int t) {
    std::vector<int> e0, e1;
    for (int p : crypto::sample_distinct(src, 2 * r0, t))
        (p < r0 ? e0 : e1).push_back(p % r0);
    return GF2E_bike(e0, e1);
}

static GF2E_bike make_key(crypto::ByteSource& src, int r0, int d) {
    std::vector<int> h0 = crypto::sample_distinct(src, r0, d);
    std::vector<int> h1 = crypto::sample_distinct(src, r0, d);
    return GF2E_bike(h0, h1);
}

// ---- counters ------------------------------------------------------------------

// all_counters, counter_at and the slow reference ctr describe the same counters
static void test_counters_consistency(int r0, int d, int t) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    GF2E_bike e = make_error(st, r0, t);

    GF2E s = H.mult(e);
    std::vector<int> bits = GF2E_bike::to_bits(s);
    Support sup = H.support();
    std::vector<int> cnt = all_counters(sup, bits);
    CHECK(static_cast<int>(cnt.size()) == 2 * r0);

    for (int j = 0; j < 2 * r0; j++) {
        CHECK(counter_at(sup, bits, j) == cnt[j]);
        CHECK(ctr(H, s, j) == cnt[j]);
    }
}

// the same, without the slow reference, at the real size
static void test_counters_fast(int r0, int d, int t) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    GF2E_bike e = make_error(st, r0, t);

    std::vector<int> bits = GF2E_bike::to_bits(H.mult(e));
    Support sup = H.support();
    std::vector<int> cnt = all_counters(sup, bits);
    for (int j = 0; j < 2 * r0; j += 7)
        CHECK(counter_at(sup, bits, j) == cnt[j]);
}

// known values: a single error bit gives s = a column of H, so its counter is the column weight
static void test_counters_known(int r0, int d) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    Support sup = H.support();

    // no error: syndrome 0, every counter is 0
    std::vector<int> zero_cnt = all_counters(sup, GF2E_bike::to_bits(H.mult(GF2E_bike())));
    for (int c : zero_cnt) CHECK(c == 0);

    // one bit in block 0, one bit in block 1
    GF2E_bike e0({17}, {});
    CHECK(all_counters(sup, GF2E_bike::to_bits(H.mult(e0)))[17] == d);
    GF2E_bike e1({}, {23});
    CHECK(all_counters(sup, GF2E_bike::to_bits(H.mult(e1)))[r0 + 23] == d);
}

// ---- threshold -----------------------------------------------------------------

static void test_threshold() {
    CHECK(threshold_bike(0) == 36);                      // floor of the minimum
    CHECK(threshold_bike(10000) == 83);                  // floor(69.722 + 13.530)
    CHECK(threshold_bike(100, 0.01, 0.5, 0) == 1);       // floor(1.5) = 1, not 2
    CHECK(threshold_bike(100, 0.0, 50.0, 0) == 50);
    int prev = threshold_bike(0);
    for (int sw = 0; sw <= 20000; sw += 100) {           // non-decreasing in |s|
        int cur = threshold_bike(sw);
        CHECK(cur >= prev);
        prev = cur;
    }
}

// ---- one bit_flip iteration ----------------------------------------------------

static void test_bit_flip(int r0, int d) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    Support sup = H.support();

    GF2E_bike e({17}, {});
    GF2E s = H.mult(e);

    // the counter of bit 17 is d: a threshold of d+1 flips nothing
    CHECK(bit_flip(s, GF2E_bike(), sup, d + 1).is_zero());

    // with threshold d the bit 17 is flipped
    GF2E_bike out = bit_flip(s, GF2E_bike(), sup, d);
    Support out_sup = out.support();
    bool has17 = false;
    for (int p : out_sup[0]) has17 |= (p == 17);
    CHECK(has17);

    // statistics: all n counters are computed
    DecodeStats stats;
    bit_flip(s, GF2E_bike(), sup, d, &stats);
    CHECK(stats.counters == n);
}

// ---- decoders ---------------------------------------------------------------------

static void test_decoders_success(int r0, int d, int t) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    GF2E_bike e = make_error(st, r0, t);
    GF2E s = H.mult(e);

    ThresholdFn thr = [](int sw) { return threshold_bike(sw); };

    // bit-flip, fixed threshold
    DecodeStats s1;
    DecodeResult a = decoder(s, H, 10, 39, &s1);
    CHECK(a.success);
    CHECK(a.e == e);
    CHECK(s1.iters_to_success == 1);
    CHECK(s1.final_syndrome_weight == 0);
    CHECK(s1.counters >= n);

    // bit-flip, variable threshold
    DecodeResult b = decoder(s, H, 10, thr);
    CHECK(b.success);
    CHECK(b.e == e);

    // BGF
    DecodeStats s3;
    DecodeResult c = bgf_decoder(s, H, 5, thr, 3, &s3);
    CHECK(c.success);
    CHECK(c.e == e);
    CHECK(s3.iters_to_success == 1);
    CHECK(s3.final_syndrome_weight == 0);
    CHECK(s3.counters >= n);
}

static void test_decoders_zero_syndrome(int r0, int d) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    ThresholdFn thr = [](int sw) { return threshold_bike(sw); };

    GF2E s;                                    // 0
    DecodeResult a = decoder(s, H, 5, thr);
    CHECK(a.success && a.e.is_zero());
    DecodeResult b = bgf_decoder(s, H, 5, thr);
    CHECK(b.success && b.e.is_zero());
}

// a weight far beyond the correction capacity: the decoders must report the failure
static void test_decoders_failure(int r0, int d, int t) {
    init_gf2e(r0);
    auto st = make_stream();
    GF2E_bike H = make_key(st, r0, d);
    GF2E_bike e = make_error(st, r0, t);
    GF2E s = H.mult(e);
    ThresholdFn thr = [](int sw) { return threshold_bike(sw); };

    DecodeStats s1;
    DecodeResult a = decoder(s, H, 10, 39, &s1);
    CHECK(!a.success);
    CHECK(s1.iters_to_success == -1);
    CHECK(s1.final_syndrome_weight > 0);

    DecodeStats s2;
    DecodeResult b = bgf_decoder(s, H, 5, thr, 3, &s2);
    CHECK(!b.success);
    CHECK(s2.iters_to_success == -1);
    CHECK(s2.final_syndrome_weight > 0);
}

int main() {
    test_counters_consistency(101, 11, 7);
    test_counters_fast(12323, 71, 134);
    test_counters_known(101, 11);
    test_counters_known(12323, 71);
    test_threshold();
    test_bit_flip(12323, 71);
    test_decoders_success(12323, 71, 10);
    test_decoders_zero_syndrome(12323, 71);
    test_decoders_failure(12323, 71, 600);

    if (failures == 0) std::cout << "decoder: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
