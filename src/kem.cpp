#include "kem.h"
 
#include <NTL/GF2X.h>
#include <cassert>
 
#include "decoder.h"
#include "utils.h"
 
using namespace NTL;
 
namespace kem {
 
namespace {
 
// domain-separation tags: H, L, K and the key generation expansion never hash the same input
constexpr uint8_t TAG_H = 0x00;
constexpr uint8_t TAG_L = 0x01;
constexpr uint8_t TAG_K = 0x02;
constexpr uint8_t TAG_KEYGEN = 0x03;
 
// decoder settings (BIKE level 1)
constexpr int NB_ITER = 5;
constexpr int TAU = 3;
 
Bytes xor_bytes(const Bytes& a, const Bytes& b) {
    assert(a.size() == b.size());
    Bytes out(a.size());
    for (size_t i = 0; i < a.size(); i++) out[i] = static_cast<uint8_t>(a[i] ^ b[i]);
    return out;
}

 
// H : m -> (e0, e1), with total weight t over the 2r positions.
// The positions come from the deterministic stream SHAKE256(TAG_H || m), so decapsulation
// can re-derive exactly the same error from m'.
GF2E_bike hash_H(const Params& P, const Bytes& m) {
    crypto::ShakeStream stream(TAG_H, m); // calls shake256
    std::vector<int> e0, e1;
    for (int p : crypto::sample_distinct(stream, 2 * P.r, P.t))
        (p < P.r ? e0 : e1).push_back(p % P.r);
    return GF2E_bike(e0, e1);
}
 
// L : (e0, e1) -> ELL_BYTES bytes (each block is serialized on ceil(r/8) bytes)
Bytes hash_L(const GF2E_bike& e) {
    return crypto::shake256({Bytes{TAG_L}, GF2E_bike::to_bytes(e.block(0)), GF2E_bike::to_bytes(e.block(1))}, ELL_BYTES);
}
 
// K : (m or sigma, c0, c1) -> shared secret, ELL_BYTES bytes
Bytes hash_K(const Bytes& key_material, const Ciphertext& ct) {
    return crypto::shake256({Bytes{TAG_K}, key_material, GF2E_bike::to_bytes(ct.c0), ct.c1}, ELL_BYTES);
}
 
} // namespace
 
// ---- KeyGen ----------------------------------------------------------------
 
KeyPair keygen_internal(const Params& P, const Bytes& seed) {
    assert(seed.size() == ELL_BYTES);
    assert(GF2E::degree() == P.r);   // init_gf2e(P.r) must have been called
 
    // everything is derived from the seed: h0, h1, then sigma
    crypto::ShakeStream stream(TAG_KEYGEN, seed);
    for (;;) {
        // two separate statements: the evaluation order of function arguments is unspecified
        std::vector<int> pos0 = crypto::sample_distinct(stream, P.r, P.d);
        std::vector<int> pos1 = crypto::sample_distinct(stream, P.r, P.d);
        GF2E_bike H(pos0, pos1);
 
        // h0 must be invertible. For level 1 this always holds (odd weight, and
        // X^r - 1 = (X - 1) * irreducible polynomial), the test only protects other parameters.
        GF2X inv_h0;
        if (InvModStatus(inv_h0, rep(H.block(0)), GF2E::modulus()) != 0)
            continue;
 
        PublicKey pk{H.block(1) * to_GF2E(inv_h0)};   // h = h1 * h0^-1
        SecretKey sk{H, stream.bytes(ELL_BYTES)};
        return {pk, sk};
    }
}
 
KeyPair keygen(const Params& P) {
    crypto::OsRng rng;
    return keygen_internal(P, rng.bytes(ELL_BYTES));
}
 
// ---- Encaps ----------------------------------------------------------------
 
EncapsResult encaps_internal(const Params& P, const PublicKey& pk, const Bytes& m) {
    assert(m.size() == ELL_BYTES);
 
    GF2E_bike e = hash_H(P, m);                    // (e0, e1) = H(m)
 
    Ciphertext ct;
    ct.c0 = e.block(0) + pk.h * e.block(1);        // c0 = e0 + h * e1
    ct.c1 = xor_bytes(m, hash_L(e));               // c1 = m xor L(e0, e1)
 
    return {ct, hash_K(m, ct)};                    // K = K(m, c0, c1)
}
 
EncapsResult encaps(const Params& P, const PublicKey& pk) {
    crypto::OsRng rng;
    return encaps_internal(P, pk, rng.bytes(ELL_BYTES));
}
 
// ---- Decaps ----------------------------------------------------------------
 
SharedSecret decaps(const Params& P, const SecretKey& sk, const Ciphertext& ct) {
    assert(sk.sigma.size() == ELL_BYTES && ct.c1.size() == ELL_BYTES);
 
    // s = c0 * h0 = e0*h0 + e1*h1: the syndrome of (e0, e1) for the sparse matrix
    GF2E s = ct.c0 * sk.H.block(0);
 
    // decoder: it returns the zero vector on failure, and a valid error has weight t > 0
    ThresholdFn thr = [](int sw) { return threshold_bike(sw); };
    GF2E_bike e_dec = bgf_decoder(s, sk.H, NB_ITER, thr, TAU);
 
    bool ok = !e_dec.is_zero();
    Bytes m_dec;
    if (ok) {
        m_dec = xor_bytes(ct.c1, hash_L(e_dec));   // m' = c1 xor L(e')
        ok = (hash_H(P, m_dec) == e_dec);  // does H(m') give back e' ?
    }

    // implicit rejection: on any failure the key is derived from sigma instead of m'
    return hash_K(ok ? m_dec : sk.sigma, ct);
}
 
} // namespace kem
 