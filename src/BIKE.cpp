#include <NTL/GF2E.h>
#include <array>
#include <cassert>
#include <iostream>
#include "random_bike.h"
#include <iostream>
#include <vector>
#include "utils.h"
#include "decoder.h"

using namespace NTL;

// Check that coef_matrix, mult and all_counters describe the same matrix
static void test_consistency(int r0, int w, int t) {
    init_gf2e(r0);

    // h0 and h1 of weight w each
    GF2E_bike H(fisher_yates(r0, w), fisher_yates(r0, w));

    // error of weight t for each block
    GF2E_bike e(fisher_yates(r0, t), fisher_yates(r0, t));

    GF2E s = H.mult(e);
    std::vector<int> s_bits = GF2E_bike::to_bits(s);

    // 1) mult vs coef_matrix: s[i] = sum_j H[i][j] * e[j] (mod 2)
    std::vector<int> e_bits0 = GF2E_bike::to_bits(e.block(0));
    std::vector<int> e_bits1 = GF2E_bike::to_bits(e.block(1));
    for (int i = 0; i < r0; i++) {
        int acc = 0;
        for (int j = 0; j < 2 * r0; j++) {
            int ej = j < r0 ? e_bits0[j] : e_bits1[j - r0];
            acc ^= (IsOne(H.coef_matrix(i, j)) && ej);
        }
        assert(acc == s_bits[i]);
    }

    // 2) all_counters vs slow ctr
    auto sup = H.support();
    std::vector<int> cnt = all_counters(sup, s_bits);
    for (int j = 0; j < 2 * r0; j++)
        assert(cnt[j] == ctr(H, s, j));

    std::cout << "consistency test OK (r = " << r0 << ")\n";

    // 3) counter_at vs slow ctr
    // auto sup = H.support();
    std::vector<int> bits = GF2E_bike::to_bits(s);
    for (int j = 0; j < 2 * r; j++)
    assert(counter_at(sup, bits, j) == ctr(H, s, j));
}

// Real decoding run
static void test_decoding(int r0, int w, int t, int N) {
    init_gf2e(r0);

    GF2E_bike H(fisher_yates(r0, w), fisher_yates(r0, w));

    GF2E_bike e(fisher_yates(r0, w), fisher_yates(r0, w));

    GF2E s = H.mult(e);
    GF2E_bike e_dec = decoder(s, H, N);

    bool ok = (e_dec.support() == e.support());
    std::cout << "decoding (r = " << r0 << ", t = " << t << "): "
              << (ok ? "SUCCESS" : "FAILURE") << "\n";
}


int main(){
    // init_gf2e(5);

    test_consistency(17, 3, 2);
    test_decoding(12323, 62, 53, 5);

    return 0;
}
