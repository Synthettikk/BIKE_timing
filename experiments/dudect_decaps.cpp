// Timing test of kem::decaps with the dudect library (third_party/dudect/dudect.h).
// usage: dudect_decaps <fixed_vs_random|valid_vs_bad_c0|random_vs_random> [batches]
// Each batch = BATCH measurements; dudect concludes only after 10000 measurements in total.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "kem.h"
#include "utils.h"

#define DUDECT_IMPLEMENTATION
#include "dudect.h"

static const kem::Params P = kem::LEVEL1;
static const size_t BATCH = 500;

static std::string g_test;
static kem::KeyPair g_kp;                 // one fixed secret key
static kem::Ciphertext g_fixed;           // the "fixed" ciphertext
static std::vector<kem::Ciphertext> g_cts; // inputs of the current batch, refilled by prepare_inputs

// Called by dudect before each batch: draws the classes and builds the inputs (not timed).
extern "C" void prepare_inputs(dudect_config_t* c, uint8_t* input_data, uint8_t* classes) {
    g_cts.clear();
    g_cts.reserve(c->number_measurements);
    for (size_t i = 0; i < c->number_measurements; i++) {
        classes[i] = randombit();
        kem::Ciphertext ct;
        if (g_test == "fixed_vs_random" && classes[i] == 0) ct = g_fixed;
        else ct = kem::encaps(P, g_kp.pk).ct;                      // fresh valid ciphertext
        if (g_test == "valid_vs_bad_c0" && classes[i] == 1) ct.c0 = NTL::random_GF2E();
        g_cts.push_back(std::move(ct));

        const uint32_t idx = static_cast<uint32_t>(i);             // the chunk holds only an index
        std::memcpy(input_data + i * c->chunk_size, &idx, sizeof idx);
    }
}

// Called once per measurement, between two readings of the cycle counter.
extern "C" uint8_t do_one_computation(uint8_t* data) {
    uint32_t idx;
    std::memcpy(&idx, data, sizeof idx);
    kem::SharedSecret k = kem::decaps(P, g_kp.sk, g_cts[idx]);
    return k[0];                                                    // keeps the call observable
}

int main(int argc, char** argv) {
    g_test = argc > 1 ? argv[1] : "";
    const int max_batches = argc > 2 ? std::atoi(argv[2]) : 100;
    if (g_test != "fixed_vs_random" && g_test != "valid_vs_bad_c0" && g_test != "random_vs_random") {
        std::cerr << "usage: " << argv[0]
                  << " <fixed_vs_random|valid_vs_bad_c0|random_vs_random> [batches]\n";
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IOLBF, 0);                      // one line per batch, even in a pipe

    init_gf2e(P.r);
    g_kp = kem::keygen(P);
    g_fixed = kem::encaps(P, g_kp.pk).ct;

    dudect_config_t config = {sizeof(uint32_t), BATCH};
    dudect_ctx_t ctx;
    dudect_init(&ctx, &config);

    std::printf("test %s, batch = %zu measurements\n", g_test.c_str(), BATCH);
    dudect_state_t state = DUDECT_NO_LEAKAGE_EVIDENCE_YET;
    for (int b = 0; b < max_batches && state == DUDECT_NO_LEAKAGE_EVIDENCE_YET; b++)
        state = dudect_main(&ctx);                                 // prints one line per batch
    dudect_free(&ctx);

    std::printf("result: %s\n", state == DUDECT_LEAKAGE_FOUND ? "leakage found" : "no leakage evidence");
    return 0;
}
