// tests/test_utils.cpp
// Tests of GF2E_bike: construction, support, bits, bytes, flip_bit, mult, coef_matrix.
// Exit code 0 = success (that is what ctest checks).

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "crypto_rng.h"
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

// deterministic source of positions: the tests are reproducible
static crypto::ShakeStream make_stream() {
    return crypto::ShakeStream(0x10, crypto::Bytes{1, 2, 3});
}

static std::vector<int> sorted(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    return v;
}

// ---- construction, support, bits, flip_bit, equality -----------------------

static void test_basics(int r0) {
    init_gf2e(r0);
    auto st = make_stream();

    CHECK(GF2E::degree() == r0);

    // default object = 0
    GF2E_bike z;
    CHECK(z.is_zero());
    CHECK(z.support()[0].empty() && z.support()[1].empty());

    // positions -> object -> support (sorted)
    std::vector<int> p0 = crypto::sample_distinct(st, r0, 11);
    std::vector<int> p1 = crypto::sample_distinct(st, r0, 7);
    GF2E_bike a(p0, p1);
    CHECK(!a.is_zero());
    Support sup = a.support();
    CHECK(sup[0] == sorted(p0));
    CHECK(sup[1] == sorted(p1));

    // constructor from a Support, == and !=
    GF2E_bike b(sup);
    CHECK(a == b);
    CHECK(!(a != b));
    GF2E_bike c(p0, {});
    CHECK(a != c);

    // to_bits, weight, coef_poly agree
    std::vector<int> bits = GF2E_bike::to_bits(a.block(0));
    CHECK(static_cast<int>(bits.size()) == r0);
    int w = 0;
    for (int bit : bits) w += bit;
    CHECK(w == 11);
    CHECK(weight(a.block(0)) == 11);
    CHECK(weight(a.block(1)) == 7);
    for (int i = 0; i < r0; i++)
        CHECK((IsOne(GF2E_bike::coef_poly(a.block(0), i)) ? 1 : 0) == bits[i]);

    // flip_bit: j < r acts on block 0, j >= r on block 1
    GF2E_bike f;
    f.flip_bit(5);
    CHECK(f.support()[0] == std::vector<int>{5} && f.support()[1].empty());
    f.flip_bit(r0 + 9);
    CHECK(f.support()[1] == std::vector<int>{9});
    f.flip_bit(5);
    f.flip_bit(r0 + 9);
    CHECK(f.is_zero());

    // flipping twice is the identity; flipping a set bit lowers the weight
    GF2E_bike a2 = a;
    a2.flip_bit(p0[0]);
    CHECK(weight(a2.block(0)) == 10);
    a2.flip_bit(p0[0]);
    CHECK(a2 == a);
    a2.flip_bit(r0 + p1[0]);
    CHECK(weight(a2.block(1)) == 6);
}

// ---- bytes --------------------------------------------------------------------

static void test_bytes(int r0) {
    init_gf2e(r0);
    auto st = make_stream();

    GF2E_bike a(crypto::sample_distinct(st, r0, 11), crypto::sample_distinct(st, r0, 7));

    std::vector<uint8_t> by = GF2E_bike::to_bytes(a.block(0));
    CHECK(by.size() == static_cast<size_t>((r0 + 7) / 8));
    CHECK(GF2E_bike::from_bytes(by) == a.block(0));

    // layout: X^0 = bit 0 of byte 0, X^9 = bit 1 of byte 1
    GF2E_bike b({0, 9}, {});
    std::vector<uint8_t> bb = GF2E_bike::to_bytes(b.block(0));
    CHECK(bb[0] == 0x01 && bb[1] == 0x02);

    // zero polynomial
    std::vector<uint8_t> zb = GF2E_bike::to_bytes(GF2E_bike().block(0));
    CHECK(std::all_of(zb.begin(), zb.end(), [](uint8_t x) { return x == 0; }));
    CHECK(IsZero(GF2E_bike::from_bytes(zb)));

    // padding bits of the last byte: 0 in the output, ignored in the input
    const int pad = 8 * static_cast<int>(by.size()) - r0;
    if (pad > 0) {
        CHECK((by.back() >> (8 - pad)) == 0);
        std::vector<uint8_t> dirty = by;
        dirty.back() |= static_cast<uint8_t>(0xFFu << (8 - pad));
        CHECK(GF2E_bike::from_bytes(dirty) == a.block(0));
    }
}

// ---- mult: reference product, identity, symmetry ------------------------------

// s[i] = XOR over b of XOR over j in supp(h_b) of e_b[(i - j) mod r]
static std::vector<int> naive_syndrome(const Support& h, const GF2E_bike& e, int r0) {
    std::vector<int> s(r0, 0);
    for (int b = 0; b < 2; b++) {
        std::vector<int> eb = GF2E_bike::to_bits(e.block(b));
        for (int j : h[b])
            for (int i = 0; i < r0; i++)
                s[i] ^= eb[((i - j) % r0 + r0) % r0];
    }
    return s;
}

static void test_mult(int r0, int dh, int de) {
    init_gf2e(r0);
    auto st = make_stream();

    GF2E_bike H(crypto::sample_distinct(st, r0, dh), crypto::sample_distinct(st, r0, dh));
    GF2E_bike e(crypto::sample_distinct(st, r0, de), crypto::sample_distinct(st, r0, de));

    // mult against the naive product
    GF2E s = H.mult(e);
    CHECK(GF2E_bike::to_bits(s) == naive_syndrome(H.support(), e, r0));

    // symmetry: e0*h0 + e1*h1 does not depend on who calls
    CHECK(H.mult(e) == e.mult(H));

    // identity: h = (1, 0) gives e0
    GF2E_bike one({0}, {});
    CHECK(one.mult(e) == e.block(0));

    // zero error gives zero syndrome
    CHECK(IsZero(H.mult(GF2E_bike())));
}

// ---- coef_matrix: H[i][j] = h_b[(i - j) mod r] ---------------------------------

static void test_coef_matrix(int r0) {
    init_gf2e(r0);
    auto st = make_stream();

    // one single bit: H[i][j] = 1 iff (i - j) mod r == 3, in block 0 only
    GF2E_bike S({3}, {});
    for (int i = 0; i < r0; i++)
        for (int j = 0; j < 2 * r0; j++) {
            bool expected = (j < r0) && (((i - j) % r0 + r0) % r0 == 3);
            CHECK(IsOne(S.coef_matrix(i, j)) == expected);
        }

    // general case: coef_matrix agrees with the coefficients of h0 and h1
    GF2E_bike H(crypto::sample_distinct(st, r0, 11), crypto::sample_distinct(st, r0, 11));
    std::vector<int> h0 = GF2E_bike::to_bits(H.block(0));
    std::vector<int> h1 = GF2E_bike::to_bits(H.block(1));
    for (int i = 0; i < r0; i++)
        for (int j = 0; j < 2 * r0; j++) {
            int expected = j < r0 ? h0[((i - j) % r0 + r0) % r0]
                                  : h1[((i - (j - r0)) % r0 + r0) % r0];
            CHECK((IsOne(H.coef_matrix(i, j)) ? 1 : 0) == expected);
        }

    // s[i] = sum_j H[i][j] e[j] (the check of the old main)
    GF2E_bike e(crypto::sample_distinct(st, r0, 7), crypto::sample_distinct(st, r0, 5));
    std::vector<int> s = GF2E_bike::to_bits(H.mult(e));
    std::vector<int> e0 = GF2E_bike::to_bits(e.block(0));
    std::vector<int> e1 = GF2E_bike::to_bits(e.block(1));
    for (int i = 0; i < r0; i++) {
        int acc = 0;
        for (int j = 0; j < 2 * r0; j++) {
            int ej = j < r0 ? e0[j] : e1[j - r0];
            acc ^= (IsOne(H.coef_matrix(i, j)) ? 1 : 0) & ej;
        }
        CHECK(acc == s[i]);
    }
}

int main() {
    // small sizes for the exhaustive checks, the real size for the others
    for (int r0 : {37, 101, 12323}) {
        test_basics(r0);
        test_bytes(r0);
    }
    test_mult(37, 11, 7);
    test_mult(101, 11, 7);
    test_mult(12323, 71, 67);
    test_coef_matrix(37);
    test_coef_matrix(101);

    if (failures == 0) std::cout << "utils: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
