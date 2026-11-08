#include "harness.h"
#include <iostream>

int main() {
    const int R = 12323, W_HALF = 71, NB_ITER = 5, TAU = 3, TRIALS = 100;
    const std::vector<int> ts = {134, 150, 160, 170, 180, 190, 200};   // same t and seed as bitflip_v2

    ThresholdFn thr = [](int sw){ return threshold_bike(sw); };

    write_csv_header(std::cout);
    for (int t : ts) {
        std::vector<NamedDecoder> decs;
        decs.push_back({"bgf_5it",
            [=](const NTL::GF2E& s, const GF2E_bike& H, DecodeStats& st) {
                return bgf_decoder(s, H, NB_ITER, thr, TAU, &st).e;
            }});
        Config cfg{R, W_HALF, t, TRIALS, 1234};
        write_csv_rows(std::cout, cfg, run_experiment(cfg, decs));
    }
}
