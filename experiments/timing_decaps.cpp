// experiments/timing_decaps.cpp
// dudect-style timing test of kem::decaps: two input classes are timed in random order,
// then compared with Welch's t-test (raw, and cropped at a percentile to remove outliers).
//
// usage: timing_decaps <fixed_vs_random|valid_vs_bad_c0|valid_vs_bad_c1> <N> [times.csv]
// stdout: CSV curve  test,n,t_raw,t_crop     stderr: summary

#include <time.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <NTL/GF2E.h>

#include "kem.h"
#include "utils.h"

using namespace kem;

static long long now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Welch's t statistic, updated one measurement at a time (Welford's algorithm)
struct Welch {
    long n[2] = {0, 0};
    double mean[2] = {0, 0}, m2[2] = {0, 0};

    void add(int c, double x) {
        n[c]++;
        double d = x - mean[c];
        mean[c] += d / n[c];
        m2[c] += d * (x - mean[c]);
    }
    double t() const {
        if (n[0] < 2 || n[1] < 2) return 0.0;
        double v0 = m2[0] / (n[0] - 1), v1 = m2[1] / (n[1] - 1);
        return (mean[0] - mean[1]) / std::sqrt(v0 / n[0] + v1 / n[1]);
    }
};

static double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

static const char* verdict(double t) {
    double a = std::fabs(t);
    if (a > 10.0) return "LEAK (|t| > 10)";
    if (a > 4.5) return "inconclusive (4.5 < |t| <= 10)";
    return "no leak detected (|t| <= 4.5)";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <fixed_vs_random|valid_vs_bad_c0|valid_vs_bad_c1> <N> [times.csv]\n";
        return 2;
    }
    const std::string test = argv[1];
    const int N = std::atoi(argv[2]);
    const std::string times_path = argc > 3 ? argv[3] : "";
    if ((test != "fixed_vs_random" && test != "fixed_vs_fixed2" && test != "random_vs_random" &&
        test != "valid_vs_bad_c0" && test != "valid_vs_bad_c1") || N < 200) {
        std::cerr << "unknown test or N < 200\n";
        return 2;
    }
    const double CROP = 0.95;   // keep the measurements below the 95th percentile

    const Params P = LEVEL1;
    init_gf2e(P.r);
    KeyPair kp = keygen(P);                      // one fixed secret key

    // ---- prepare every input before timing anything -----------------------------
    std::mt19937 rng(12345);                     // class order: reproducible
    std::vector<int> cls(N);
    for (int& c : cls) c = static_cast<int>(rng() & 1);

    Ciphertext fixed = encaps(P, kp.pk).ct;
    Ciphertext fixed2 = encaps(P, kp.pk).ct;
    std::vector<Ciphertext> cts;
    cts.reserve(N);
    for (int i = 0; i < N; i++) {
        Ciphertext ct;
        if (test == "fixed_vs_fixed2") ct = (cls[i] == 0 ? fixed : fixed2);
        else if (test == "fixed_vs_random" && cls[i] == 0) ct = fixed;
        else ct = encaps(P, kp.pk).ct;                         // fresh valid ciphertext
        if (test == "valid_vs_bad_c0" && cls[i] == 1) ct.c0 = NTL::random_GF2E();
        if (test == "valid_vs_bad_c1" && cls[i] == 1) ct.c1[0] ^= 1;
        cts.push_back(std::move(ct));
    }

    // ---- measure -------------------------------------------------------------------
    for (int i = 0; i < 50; i++) decaps(P, kp.sk, fixed);       // warm-up

    std::vector<double> times(N);
    unsigned sink = 0;                                          // keeps the calls observable
    for (int i = 0; i < N; i++) {
        long long t0 = now_ns();
        SharedSecret k = decaps(P, kp.sk, cts[i]);
        long long t1 = now_ns();
        times[i] = static_cast<double>(t1 - t0);
        sink ^= k[0];
    }

    // ---- analyse: t against the number of measurements ---------------------------------
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const double thr = sorted[static_cast<size_t>(CROP * (N - 1))];

    Welch raw, crop;
    long next = 100;
    std::cout << "test,n,t_raw,t_crop\n";
    for (int i = 0; i < N; i++) {
        raw.add(cls[i], times[i]);
        if (times[i] <= thr) crop.add(cls[i], times[i]);
        if (i + 1 >= next || i + 1 == N) {
            std::printf("%s,%d,%.4f,%.4f\n", test.c_str(), i + 1, raw.t(), crop.t());
            while (next <= i + 1) next = next * 5 / 4 + 1;      // roughly geometric steps
        }
    }

    if (!times_path.empty()) {
        std::ofstream f(times_path);
        f << "i,class,ns\n";
        for (int i = 0; i < N; i++) f << i << "," << cls[i] << "," << times[i] << "\n";
    }

    // ---- summary ---------------------------------------------------------------------------
    std::vector<double> c0, c1;
    for (int i = 0; i < N; i++) (cls[i] == 0 ? c0 : c1).push_back(times[i]);
    std::cerr << "test " << test << ", N = " << N << " (sink " << sink << ")\n"
              << "  median time, class 0: " << median(c0) / 1000.0 << " us"
              << ", class 1: " << median(c1) / 1000.0 << " us\n"
              << "  t (raw)     = " << raw.t() << "  " << verdict(raw.t()) << "\n"
              << "  t (cropped) = " << crop.t() << "  " << verdict(crop.t()) << "\n";
    return 0;
}
