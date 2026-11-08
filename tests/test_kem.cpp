// Tests of the KEM. Exit code 0 = success (that is what ctest checks).
 
#include <iostream>
#include <vector>
 
#include "crypto_rng.h"
#include "kem.h"
#include "utils.h"
 
using namespace kem;
 
static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "FAILED (line " << __LINE__ << "): " #cond << "\n";          \
            failures++;                                                               \
        }                                                                             \
    } while (0)
 
int main() {
    const Params P = LEVEL1;
    init_gf2e(P.r);
    crypto::OsRng os;
 
    // 1. serialization: size, round trip, bit layout
    {
        std::vector<int> p0 = crypto::sample_distinct(os, P.r, P.d);
        std::vector<int> p1 = crypto::sample_distinct(os, P.r, P.d);
        GF2E_bike a(p0, p1);
        Bytes ba = GF2E_bike::to_bytes(a.block(0));
        CHECK(ba.size() == static_cast<size_t>((P.r + 7) / 8));
        CHECK(GF2E_bike::from_bytes(ba) == a.block(0));
 
        GF2E_bike b({0, 9}, {});               // 1 + X^9
        Bytes bb = GF2E_bike::to_bytes(b.block(0));
        CHECK(bb[0] == 0x01 && bb[1] == 0x02); // X^0 = bit 0 of byte 0, X^9 = bit 1 of byte 1
    }
 
    // 2. keygen_internal: determinism
    const Bytes seed = os.bytes(ELL_BYTES);
    KeyPair k1 = keygen_internal(P, seed);
    KeyPair k2 = keygen_internal(P, seed);
    CHECK(k1.pk.h == k2.pk.h);
    CHECK(k1.sk.sigma == k2.sk.sigma);
    CHECK(k1.sk.H.block(0) == k2.sk.H.block(0));
    CHECK(k1.sk.H.block(1) == k2.sk.H.block(1));
    KeyPair k3 = keygen_internal(P, os.bytes(ELL_BYTES));
    CHECK(k1.pk.h != k3.pk.h);
 
    // 3. a well-formed key: weights d, sigma size, and h * h0 = h1
    {
        auto sup = k1.sk.H.support();
        CHECK(static_cast<int>(sup[0].size()) == P.d);
        CHECK(static_cast<int>(sup[1].size()) == P.d);
        CHECK(k1.sk.sigma.size() == ELL_BYTES);
        CHECK(k1.pk.h * k1.sk.H.block(0) == k1.sk.H.block(1));
    }
 
    // 4. encaps_internal is deterministic, and decaps agrees with it
    {
        const Bytes m = os.bytes(ELL_BYTES);
        EncapsResult a = encaps_internal(P, k1.pk, m);
        EncapsResult b = encaps_internal(P, k1.pk, m);
        CHECK(a.ct.c0 == b.ct.c0);
        CHECK(a.ct.c1 == b.ct.c1);
        CHECK(a.key == b.key);
        CHECK(a.ct.c1.size() == ELL_BYTES && a.key.size() == ELL_BYTES);
        CHECK(decaps(P, k1.sk, a.ct) == a.key);
    }
 
    // 5. round trips with fresh randomness
    for (int i = 0; i < 10; i++) {
        KeyPair kp = keygen(P);
        EncapsResult er = encaps(P, kp.pk);
        CHECK(decaps(P, kp.sk, er.ct) == er.key);
    }
 
    // 6. implicit rejection: a modified ciphertext gives another key, without failing
    {
        KeyPair kp = keygen(P);
        EncapsResult er = encaps(P, kp.pk);
 
        Ciphertext bad1 = er.ct;
        bad1.c1[0] ^= 1;                               // flip one bit of c1
        SharedSecret kb1 = decaps(P, kp.sk, bad1);
        CHECK(kb1.size() == ELL_BYTES);
        CHECK(kb1 != er.key);
        CHECK(kb1 == decaps(P, kp.sk, bad1));          // deterministic
 
        Ciphertext bad2 = er.ct;
        bad2.c0 += NTL::to_GF2E(1);                    // flip the coefficient of X^0 in c0
        CHECK(decaps(P, kp.sk, bad2) != er.key);
    }
 
    if (failures == 0) std::cout << "kem: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
