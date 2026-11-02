// Bit-Flipping Decoder for QC-MDPC Codes 

#include "NTL/GF2E.h"
#include "NTL/GF2X.h"
#include "utils.h"
#include "decoder.h"
#include <vector>

using namespace NTL;
using Support = std::array<std::vector<int>, 2>;

int threshold_bike(int s_weight, double c0, double c1, int t_min){
    return std::max((int)std::floor(c0 * s_weight + c1), t_min);
}


// Count violations involving position j: count[j] ← #{i : Hi,j = 1 and s[i] = 1}
int ctr(const GF2E_bike& H, const GF2E& s, int j){
    int count = 0;

    for(int i = 0; i < r; i++){
        GF2 coef_H = H.coef_matrix(i, j);
        GF2 coef_s = GF2E_bike::coef_poly(s, i);
        count += IsOne(coef_H * coef_s);
    }
    return count;
}

// Count violations involving position j: count[j] ← #{i : Hi,j = 1 and s[i] = 1}
// for all j, and return the counters 
std::vector<int> all_counters(const Support& supp, const std::vector<int>& s_bits){
    std::vector<int> count(n, 0);
    for(int bloc = 0; bloc < 2; bloc++)
        for(int k : supp[bloc])
            for(int j = 0; j < r; j++){
                int idx = (j + k);
                if (idx >= r) idx -= r;
                count[bloc * r + j] += s_bits[idx];
            }
    return count;
}

// BIT FLIP 
GF2E_bike bit_flip(const GF2E& s, GF2E_bike e, const Support& supp, int T, DecodeStats* st){
    std::vector<int> counts = all_counters(supp, GF2E_bike::to_bits(s));
    if (st) st->counters += n;  // all n counters computed here
    for(int j = 0; j < n; j++){
        if(counts[j] >= T) e.flip_bit(j);
    }
    return e;
}

// DECODER bit-flip classic
GF2E_bike decoder(const GF2E& s, const GF2E_bike& H, int N,
                  const ThresholdFn& thr, DecodeStats* st){
    Support supp_H = H.support();
    GF2E_bike e;
    GF2E s2 = s;

    for(int i = 0; i < N; i++){
        int T = thr(weight(s2));          // your function from utils.h
        e = bit_flip(s2, e, supp_H, T, st);
        s2 = s + H.mult(e);
        if (st && st->iters_to_success < 0 && IsZero(s2)) {
            st->iters_to_success = i + 1;
            st->counters_to_success = st->counters;
        }
    }
    if (st) st->final_syndrome_weight = weight(s2);
    if (IsZero(s2)) return e;
    return GF2E_bike();
}

GF2E_bike decoder(const GF2E& s, const GF2E_bike& H, int N, int T, DecodeStats* st){
    return decoder(s, H, N, ThresholdFn([T](int){ return T; }), st);
}

// BFMaskedIter: counters on the current syndrome, only for the positions of `mask`,
// then all flips applied at once (like bit_flip)
static GF2E_bike masked_iter(const GF2E& sigma, GF2E_bike e, const Support& supp,
                             const std::vector<int>& mask, int T, DecodeStats* st){
    std::vector<int> bits = GF2E_bike::to_bits(sigma);
    std::vector<int> to_flip;
    for (int j : mask)
        if (counter_at(supp, bits, j) >= T) to_flip.push_back(j);
    if (st) st->counters += (long)mask.size();
    for (int j : to_flip) e.flip_bit(j);         // a black position flipped again = flip reverted
    return e;
}

GF2E_bike bgf_decoder(const GF2E& s, const GF2E_bike& H, int NbIter,
                      const ThresholdFn& thr, int tau, DecodeStats* st){
    Support supp = H.support();
    const int d = (int)supp[0].size();           // w/2
    const int T_masked = (d + 1) / 2 + 1;        // 37 for d = 71
    GF2E_bike e;
    GF2E sigma = s;                              // current syndrome = s + H*e

    for (int it = 1; it <= NbIter; it++){
        int T = thr(weight(sigma)); 

        // BFIter: all counters on the same syndrome, then parallel flip
        std::vector<int> counts = all_counters(supp, GF2E_bike::to_bits(sigma));
        if (st) st->counters += n;
        std::vector<int> black, gray;
        for (int j = 0; j < n; j++){
            if (counts[j] >= T) { e.flip_bit(j); black.push_back(j); }
            else if (counts[j] >= T - tau) gray.push_back(j);
        }
        sigma = s + H.mult(e);

        if (it == 1){   // masked steps: first iteration only
            e = masked_iter(sigma, e, supp, black, T_masked, st);
            sigma = s + H.mult(e);
            e = masked_iter(sigma, e, supp, gray, T_masked, st);
            sigma = s + H.mult(e);
        }

        if (st && st->iters_to_success < 0 && IsZero(sigma)) {   // record only, no early exit
            st->iters_to_success = it;
            st->counters_to_success = st->counters;
        }
    }
    if (st) st->final_syndrome_weight = weight(sigma);
    if (IsZero(sigma)) return e;
    return GF2E_bike();
}
