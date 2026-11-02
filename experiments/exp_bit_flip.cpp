// experiments/exp_bit_flip.cpp
#include "harness.h"
#include <iostream>

int main() {
    const int R = 12323, W_HALF = 71, N = 10, TRIALS = 100;
    const std::vector<int> ts = {134, 150, 160, 170, 180, 190, 200}; // error weights to sweep
    const std::vector<int> Ts = {39, 42, 44, 46, 48};  // thresholds to sweep

    ThresholdFn thr = [](int sw){ return threshold_bike(sw); };

    write_csv_header(std::cout);
    for (int t : ts) {
        std::vector<NamedDecoder> decs;
        for (int T : Ts)
            decs.push_back({"bitflip_T" + std::to_string(T),
                [=](const NTL::GF2E& s, const GF2E_bike& H, DecodeStats& st) {
                    return decoder(s, H, N, T, &st);
                }});
        decs.push_back({"bitflip_var",
            [=](const NTL::GF2E& s, const GF2E_bike& H, DecodeStats& st) {
                return decoder(s, H, N, thr, &st);
            }});
        Config cfg{R, W_HALF, t, TRIALS, 1234};
        write_csv_rows(std::cout, cfg, run_experiment(cfg, decs));
    }
}
