#ifndef DECODER_H
#define DECODER_H

// Bit-Flipping Decoder for QC-MDPC Codes 

#include "NTL/GF2E.h"
#include "NTL/GF2X.h"
#include "utils.h"
#include <functional>

using ThresholdFn = std::function<int(int)>;   // |s| -> T

struct DecodeStats {
    int  iters_to_success = -1;    // 1-based iteration where s became 0 (-1 = never)
    long counters = 0;             // number of counters computed so far
    long counters_to_success = 0;  // value of `counters` at that iteration
    int  final_syndrome_weight = 0;
};

// Threshold of BIKE level 1: T = max(floor(c0*|s| + c1), t_min).
// The constants depend on (r, w, t)
int threshold_bike(int s_weight, double c0 = 0.0069722, double c1 = 13.530, int t_min = 36);

// Count violations involving position j: count[j] ← #{i : Hi,j = 1 and s[i] = 1}
int ctr(const GF2E_bike& H, const NTL::GF2E& s, int j);

// Count violations involving position j: count[j] ← #{i : Hi,j = 1 and s[i] = 1}
// for all j, and return the counters 
std::vector<int> all_counters(const Support& supp, const std::vector<int>& s_bits);

// Count violations involving position j: count[j] ← #{i : Hi,j = 1 and s[i] = 1}
int ctr(const GF2E_bike& H, const NTL::GF2E& s, int j);

// counter of a single position j in [0, 2r): O(d) instead of O(w*r)
static inline int counter_at(const Support& supp, const std::vector<int>& s_bits, int j){
    int bloc = j / r;
    int jl = j - bloc * r;                       // index inside the block
    int c = 0;
    for (int k : supp[bloc]) c += s_bits[(jl + k) % r];   // same formula as all_counters
    return c;
}

GF2E_bike bit_flip(const NTL::GF2E& s, GF2E_bike e, const Support& supp, int T,
                   DecodeStats* st = nullptr);

struct DecodeResult {
    GF2E_bike e;      // last estimate of the error: valid only if success is true
    bool success;     // true iff H*e = s (final syndrome is zero)
};

// variable threshold: T recomputed at each iteration from |s|
DecodeResult decoder(const NTL::GF2E& s, const GF2E_bike& H, int N,
                     const ThresholdFn& thr, DecodeStats* st = nullptr);

// fixed threshold (reference)
DecodeResult decoder(const NTL::GF2E& s, const GF2E_bike& H, int N,
                     int T = 39, DecodeStats* st = nullptr);

DecodeResult bgf_decoder(const NTL::GF2E& s, const GF2E_bike& H, int NbIter,
                         const ThresholdFn& thr, int tau = 3, DecodeStats* st = nullptr);


#endif
