// experiments/harness.cpp
#include "harness.h"
#include "random_bike.h"
#include <cmath>
#include <iostream>

using namespace NTL;

static GF2E_bike sample_key(int r0, int w_half) {
    return GF2E_bike(fisher_yates(r0, w_half), fisher_yates(r0, w_half));
}

// e uniform among the weight-t vectors of length 2r (the weight per block is random)
static GF2E_bike sample_error(int r0, int t) {
    std::vector<int> e0, e1;
    for (int p : fisher_yates(2 * r0, t)) (p < r0 ? e0 : e1).push_back(p % r0);
    return GF2E_bike(e0, e1);
}

static void wilson(int k, int n, double& lo, double& hi) {
    const double z = 1.96;
    double p = (double)k / n;
    double denom = 1.0 + z * z / n;
    double centre = (p + z * z / (2.0 * n)) / denom;
    double half = z * std::sqrt(p * (1 - p) / n + z * z / (4.0 * n * n)) / denom;
    lo = std::max(0.0, centre - half);
    hi = std::min(1.0, centre + half);
}

std::vector<DecoderResult> run_experiment(const Config& cfg,
                                          const std::vector<NamedDecoder>& decoders) {
    init_gf2e(cfg.r);
    gen.seed(cfg.seed);

    const size_t D = decoders.size();
    std::vector<DecoderResult> res(D);
    std::vector<double> sum_it(D, 0), sum_ct(D, 0), sum_w(D, 0);
    for (size_t d = 0; d < D; d++) res[d].name = decoders[d].name;

    for (int trial = 0; trial < cfg.trials; trial++) {
        GF2E_bike H = sample_key(cfg.r, cfg.w_half);
        GF2E_bike e = sample_error(cfg.r, cfg.t);
        GF2E s = H.mult(e);
        auto e_supp = e.support();

        for (size_t d = 0; d < D; d++) {
            DecodeStats st;
            GF2E_bike e_hat = decoders[d].fn(s, H, st);
            res[d].trials++;
            if (e_hat.support() == e_supp) {      // exact recovery of e
                sum_it[d] += st.iters_to_success;
                sum_ct[d] += st.counters_to_success;
            } else {
                res[d].failures++;
                sum_w[d] += st.final_syndrome_weight;
            }
        }
        if ((trial + 1) % 50 == 0)
            std::cerr << "t=" << cfg.t << " trial " << (trial + 1) << "/" << cfg.trials << "\n";
    }

    for (size_t d = 0; d < D; d++) {
        int succ = res[d].trials - res[d].failures;
        wilson(res[d].failures, res[d].trials, res[d].fail_lo, res[d].fail_hi);
        res[d].mean_iters      = succ > 0 ? sum_it[d] / succ : 0;
        res[d].mean_counters   = succ > 0 ? sum_ct[d] / succ : 0;
        res[d].mean_final_weight = res[d].failures > 0 ? sum_w[d] / res[d].failures : 0;
    }
    return res;
}

void write_csv_header(std::ostream& os) {
    os << "r,w_half,t,trials,decoder,failures,fail_rate,fail_lo,fail_hi,"
          "mean_iters,mean_counters,mean_final_syndrome_weight\n";
}

void write_csv_rows(std::ostream& os, const Config& cfg,
                    const std::vector<DecoderResult>& res) {
    for (const auto& d : res) {
        os << cfg.r << ',' << cfg.w_half << ',' << cfg.t << ',' << d.trials << ','
           << d.name << ',' << d.failures << ','
           << (double)d.failures / d.trials << ',' << d.fail_lo << ',' << d.fail_hi << ','
           << d.mean_iters << ',' << d.mean_counters << ',' << d.mean_final_weight << '\n';
    }
    os.flush();
}
