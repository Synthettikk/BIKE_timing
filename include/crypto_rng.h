#pragma once
// Randomness, hashing and sampling for the KEM layer (built on OpenSSL).
//
// The decoder experiments keep using random_bike (mt19937) so that their results stay
// reproducible from a seed. Everything that touches keys, messages or errors of the KEM
// goes through this module instead.
 
#include <cstddef>
#include <cstdint>
#include <vector>
 
namespace crypto {
 
using Bytes = std::vector<uint8_t>;
 
// ---- hashing ---------------------------------------------------------------
 
// SHAKE256 of the concatenation of `parts`, producing `out_len` bytes.
// No length prefix is added between parts, so callers must pass parts of fixed, agreed
// lengths (true for m, c0, c1 in BIKE). By convention the first part is a one-byte
// domain-separation tag, so that H, L and K never hash the same input.
Bytes shake256(const std::vector<Bytes>& parts, size_t out_len);
 
// ---- sources of random bytes ------------------------------------------------
 
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual void fill(uint8_t* out, size_t n) = 0;
 
    Bytes bytes(size_t n) {
        Bytes b(n);
        if (n > 0) fill(b.data(), n);
        return b;
    }
};
 
// Fresh secret randomness from the operating system, through OpenSSL's CSPRNG.
// Used for sigma, m and the seed of the key generation.
class OsRng : public ByteSource {
public:
    void fill(uint8_t* out, size_t n) override;
};
 
// Deterministic stream: the output of SHAKE256(tag || seed), read as far as needed.
// The same (tag, seed) always gives the same bytes. Used for H(m), and to expand a
// key generation seed.
class ShakeStream : public ByteSource {
public:
    ShakeStream(uint8_t tag, const Bytes& seed);
    ~ShakeStream() override;
    ShakeStream(const ShakeStream&) = delete;
    ShakeStream& operator=(const ShakeStream&) = delete;
 
    void fill(uint8_t* out, size_t n) override;
 
private:
    uint8_t tag_;
    Bytes seed_;
    Bytes buf_;      // SHAKE256 output computed so far
    size_t pos_ = 0; // bytes already handed out
};
 
// ---- sampling ---------------------------------------------------------------
 
// Uniform integer in [0, n), without modulo bias (rejection sampling). Requires n > 0.
int uniform_int(ByteSource& src, int n);
 
// t distinct integers of [0, n), uniformly among all such sets (partial Fisher-Yates).
// These are the positions of the 1s, which is what the GF2E_bike constructors take.
// Not constant-time: memory accesses and the number of rejections depend on the values.
std::vector<int> sample_distinct(ByteSource& src, int n, int t);
 
} // namespace crypto