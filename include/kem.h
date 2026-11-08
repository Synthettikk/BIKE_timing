#pragma once
// BIKE KEM: types and keygen, encaps, decaps functions.
// need call init_gf2e(params.r) once before creating any key.
 
#include <cstddef>
#include <NTL/GF2E.h>
#include "crypto_rng.h"
#include "utils.h"          // GF2E_bike
 
namespace kem {
 
using crypto::Bytes;
 
// BIKE parameters. r must be given to init_gf2e() before creating any key.
struct Params {
    int r;   // block length (the code has length n = 2r)
    int d;   // weight of h0 and of h1 (d = w/2, odd)
    int t;   // weight of the error vector (over the 2r positions)
};
 
constexpr Params LEVEL1{12323, 71, 134};      // NIST level 1: w = 142
constexpr std::size_t ELL_BYTES = 32;         // ell = 256 bits: sizes of m, sigma, c1 and K
 
struct PublicKey {
    NTL::GF2E h;       // h = h1 * h0^-1
};
 
struct SecretKey {
    GF2E_bike H;       // (h0, h1), the first columns of the circulant blocks
    Bytes sigma;       // ELL_BYTES bytes, used for implicit rejection
};
 
struct Ciphertext {
    NTL::GF2E c0;      // e0 + h * e1
    Bytes c1;          // ELL_BYTES bytes: m xor L(e0, e1)
};
 
using SharedSecret = Bytes;   // ELL_BYTES bytes
 
struct KeyPair {
    PublicKey pk;
    SecretKey sk;
};
 
struct EncapsResult {
    Ciphertext ct;
    SharedSecret key;
};
 
// ---- KeyGen ----------------------------------------------------------------
// `seed` (ELL_BYTES bytes) determines the key pair completely: same seed, same keys.
KeyPair keygen_internal(const Params& P, const Bytes& seed);
KeyPair keygen(const Params& P);   // fresh seed from the operating system
 
// ---- Encaps ----------------------------------------------------------------
// `m` (ELL_BYTES bytes) is the message: same (pk, m), same ciphertext and key.
EncapsResult encaps_internal(const Params& P, const PublicKey& pk, const Bytes& m);
EncapsResult encaps(const Params& P, const PublicKey& pk);   // fresh m from the OS
 
// ---- Decaps ----------------------------------------------------------------
// No randomness is involved (sigma is part of the secret key), so there is no
// internal version. Always returns a key: on failure it is derived from sigma.
SharedSecret decaps(const Params& P, const SecretKey& sk, const Ciphertext& ct);
 
} // namespace kem