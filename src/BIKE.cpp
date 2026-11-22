// Demo of the educational BIKE KEM: key generation, encapsulation, decapsulation (with the
// decoder's statistics), implicit rejection, and a few round trips.
// usage: bike_demo [--fixed]     --fixed: fixed seeds, so the output (except timings) is
//                                the same at every run

#include <chrono>
#include <cstdio>
#include <string>

#include "decoder.h"
#include "kem.h"
#include "utils.h"

using Clock = std::chrono::steady_clock;

static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// hexadecimal string of the first max_bytes bytes (all of them if max_bytes = 0);
// "..." is appended when the output is truncated
static std::string hex(const kem::Bytes& b, size_t max_bytes = 0) {
    static const char* digits = "0123456789abcdef";
    const size_t n = (max_bytes != 0 && b.size() > max_bytes) ? max_bytes : b.size();
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += digits[b[i] >> 4];
        s += digits[b[i] & 15];
    }
    if (n < b.size()) s += "...";
    return s;
}

static void section(const char* title) { std::printf("\n== %s ==\n", title); }

int main(int argc, char** argv) {
    const bool fixed = argc > 1 && std::string(argv[1]) == "--fixed";
    const kem::Params P = kem::LEVEL1;
    init_gf2e(P.r);

    section("Parameters (BIKE level 1)");
    std::printf("r = %d, block weight d = %d (w = %d), error weight t = %d, ell = %zu bits\n",
                P.r, P.d, 2 * P.d, P.t, 8 * kem::ELL_BYTES);

    // ---- KeyGen ----------------------------------------------------------------
    section("KeyGen");
    kem::Bytes seed(kem::ELL_BYTES), msg(kem::ELL_BYTES);   // used only with --fixed
    for (size_t i = 0; i < kem::ELL_BYTES; i++) {
        seed[i] = static_cast<uint8_t>(i);
        msg[i] = static_cast<uint8_t>(0x80 + i);
    }
    auto t0 = Clock::now();
    kem::KeyPair kp = fixed ? kem::keygen_internal(P, seed) : kem::keygen(P);
    const double t_keygen = ms_since(t0);

    const kem::Bytes pk_bytes = GF2E_bike::to_bytes(kp.pk.h);
    const Support sup = kp.sk.H.support();
    std::printf("public key h: %zu bytes, starts with %s\n", pk_bytes.size(), hex(pk_bytes, 16).c_str());
    std::printf("secret key: h0 and h1 of weight %zu and %zu, sigma of %zu bytes (not printed)\n",
                sup[0].size(), sup[1].size(), kp.sk.sigma.size());
    std::printf("time: %.1f ms\n", t_keygen);

    // ---- Encaps ----------------------------------------------------------------
    section("Encaps");
    t0 = Clock::now();
    kem::EncapsResult er = fixed ? kem::encaps_internal(P, kp.pk, msg) : kem::encaps(P, kp.pk);
    const double t_encaps = ms_since(t0);

    const size_t c0_bytes = GF2E_bike::to_bytes(er.ct.c0).size();
    std::printf("ciphertext: c0 = %zu bytes, c1 = %zu bytes (total %zu)\n",
                c0_bytes, er.ct.c1.size(), c0_bytes + er.ct.c1.size());
    std::printf("shared secret (encaps): %s\n", hex(er.key).c_str());
    std::printf("time: %.1f ms\n", t_encaps);

    // ---- Decaps, step by step ---------------------------------------------------------
    section("Decaps");
    // The same first steps as kem::decaps, to show what the decoder does.
    const NTL::GF2E s = er.ct.c0 * kp.sk.H.block(0);
    std::printf("syndrome s = c0 * h0: weight %d (out of %d)\n", weight(s), P.r);

    ThresholdFn thr = [](int sw) { return threshold_bike(sw); };
    DecodeStats st;
    DecodeResult dec = bgf_decoder(s, kp.sk.H, 5, thr, 3, &st);
    std::printf("BGF decoder (5 iterations, no early exit): %s\n", dec.success ? "success" : "failure");
    std::printf("  syndrome reached 0 at iteration %d, final syndrome weight %d\n",
                st.iters_to_success, st.final_syndrome_weight);
    std::printf("  counters computed: %ld\n", st.counters);
    std::printf("  weight of the recovered error: %d (t = %d)\n",
                weight(dec.e.block(0)) + weight(dec.e.block(1)), P.t);

    t0 = Clock::now();
    const kem::SharedSecret key = kem::decaps(P, kp.sk, er.ct);   // decodes again, then re-encrypts
    const double t_decaps = ms_since(t0);
    std::printf("shared secret (decaps): %s\n", hex(key).c_str());
    std::printf("keys match: %s\n", key == er.key ? "yes" : "NO");
    std::printf("time: %.1f ms\n", t_decaps);

    // ---- Implicit rejection --------------------------------------------------------------
    section("Implicit rejection");
    kem::Ciphertext bad1 = er.ct;
    bad1.c1[0] ^= 1;                                         // one bit of c1 changed
    const kem::SharedSecret k1 = kem::decaps(P, kp.sk, bad1);
    std::printf("c1 modified:  key %s (%s)\n", hex(k1, 8).c_str(),
                k1 == er.key ? "same as the real key: BUG" : "different, no error reported");

        kem::Ciphertext bad2 = er.ct;
    {
        // c0 replaced by a random element: a fixed stream with --fixed, the OS generator otherwise
        crypto::ShakeStream det(0x30, seed);
        crypto::OsRng os;
        crypto::ByteSource& src = fixed ? static_cast<crypto::ByteSource&>(det)
                                        : static_cast<crypto::ByteSource&>(os);
        bad2.c0 = GF2E_bike::from_bytes(src.bytes(c0_bytes));
    }
    const kem::SharedSecret k2 = kem::decaps(P, kp.sk, bad2);
    std::printf("c0 replaced:  key %s (%s)\n", hex(k2, 8).c_str(),
                k2 == er.key ? "same as the real key: BUG" : "different, no error reported");

    // ---- Round trips -------------------------------------------------------------------------
    section("Round trips with fresh randomness");
    const int TRIALS = 20;
    int ok = 0;
    t0 = Clock::now();
    for (int i = 0; i < TRIALS; i++) {
        kem::KeyPair k = kem::keygen(P);
        kem::EncapsResult e = kem::encaps(P, k.pk);
        if (kem::decaps(P, k.sk, e.ct) == e.key) ok++;
    }
    std::printf("%d/%d keys agree (%.1f s)\n", ok, TRIALS, ms_since(t0) / 1000.0);
    std::printf("A decoding failure is negligibly rare with these parameters; one here would point to a bug.\n");

    return (ok == TRIALS && key == er.key && k1 != er.key && k2 != er.key) ? 0 : 1;
}
