// experiments/harness.h
#pragma once
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>
#include <NTL/GF2E.h>
#include "utils.h"   // adapt to your header name
#include "decoder.h"

using DecodeFn = std::function<GF2E_bike(const NTL::GF2E& s, const GF2E_bike& H, DecodeStats& st)>;

struct NamedDecoder {
    std::string name;
    DecodeFn fn;
};

struct Config {
    int r, w_half, t;      // code length r, weight of each h_b, weight of e
    int trials;
    unsigned seed;
};

struct DecoderResult {
    std::string name;
    int trials = 0, failures = 0;
    double fail_lo = 0, fail_hi = 0;      // 95% Wilson interval on the failure rate
    double mean_iters = 0;                // over successes
    double mean_counters = 0;             // over successes
    double mean_final_weight = 0;         // |s| at the end, over failures
};

// Same (H, e) samples for every decoder in the list.
std::vector<DecoderResult> run_experiment(const Config& cfg,
                                          const std::vector<NamedDecoder>& decoders);

void write_csv_header(std::ostream& os);
void write_csv_rows(std::ostream& os, const Config& cfg,
                    const std::vector<DecoderResult>& res);
