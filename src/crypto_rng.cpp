#include "crypto_rng.h"
 
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
 
#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>
#include <stdexcept>

namespace crypto {

namespace {
 
// EVP_MD_CTX is a C object that must be released with EVP_MD_CTX_free.
// unique_ptr + custom deleter does that automatically, even if we throw.
struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* c) const { EVP_MD_CTX_free(c); }
};
using MdCtx = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;
 
// Overwrite a buffer in a way the compiler is not allowed to optimize away
// (a plain memset on a buffer that is about to be freed may be removed).
void wipe(Bytes& b) {
    if (!b.empty()) OPENSSL_cleanse(b.data(), b.size());
}
 
// uniform random 32bits from openSSL CSPRNG 
uint32_t rand_u32(ByteSource& src) {
    uint8_t buf[4];
    src.fill(buf, sizeof buf);
    return  static_cast<uint32_t>(buf[0])
         | (static_cast<uint32_t>(buf[1]) << 8)
         | (static_cast<uint32_t>(buf[2]) << 16)
         | (static_cast<uint32_t>(buf[3]) << 24);
}

} // namespace


// ---- hashing ---------------------------------------------------------------
 
Bytes shake256(const std::vector<Bytes>& parts, size_t out_len) {
    if (out_len == 0) return {};
 
    MdCtx ctx(EVP_MD_CTX_new());
    if (!ctx) throw std::runtime_error("SHAKE256: EVP_MD_CTX_new failed");
 
    // 1) choose the algorithm
    if (EVP_DigestInit_ex(ctx.get(), EVP_shake256(), nullptr) != 1)
        throw std::runtime_error("SHAKE256: EVP_DigestInit_ex failed");
 
    // 2) absorb the input, in as many pieces as we like
    for (const Bytes& p : parts) {
        if (p.empty()) continue;
        if (EVP_DigestUpdate(ctx.get(), p.data(), p.size()) != 1)
            throw std::runtime_error("SHAKE256: EVP_DigestUpdate failed");
    }
 
    // 3) squeeze: SHAKE256 is an extendable-output function (XOF), so we choose
    //    the output length. (A fixed-length hash such as SHA3-256 would use EVP_DigestFinal_ex.)
    Bytes out(out_len);
    if (EVP_DigestFinalXOF(ctx.get(), out.data(), out_len) != 1)
        throw std::runtime_error("SHAKE256: EVP_DigestFinalXOF failed");
    return out;
}

// ---- OsRng ---------------------------------------------------------------
 
void OsRng::fill(uint8_t* out, size_t n) {
    while (n > 0) {
        // RAND_priv_bytes takes an int: cut very large requests into chunks
        const size_t chunk = std::min<size_t>(n, size_t(1) << 30);
        // Returns 1 on success. Never ignore it: on failure the buffer is not random.
        if (RAND_priv_bytes(out, static_cast<int>(chunk)) != 1)
            throw std::runtime_error("RAND_priv_bytes failed");
        out += chunk;
        n -= chunk;
    }
}

// ---- ShakeStream ---------------------------------------------------------
 
ShakeStream::ShakeStream(uint8_t tag, const Bytes& seed) : tag_(tag), seed_(seed) {}
 
ShakeStream::~ShakeStream() {
    wipe(seed_);
    wipe(buf_);
}
 
void ShakeStream::fill(uint8_t* out, size_t n) {
    if (n == 0) return;
 
    if (pos_ + n > buf_.size()) {
        // Not enough output yet: recompute SHAKE256 with a longer output. This is consistent
        // because the first L bytes of SHAKE256(x, 2L) are exactly SHAKE256(x, L): the bytes
        // already handed out do not change. Doubling keeps the total work linear.
        const size_t new_len = std::max({buf_.size() * 2, pos_ + n, size_t(4096)});
        wipe(buf_);
        buf_ = shake256({Bytes{tag_}, seed_}, new_len);
    }
    std::copy(buf_.data() + pos_, buf_.data() + pos_ + n, out);
    pos_ += n;
}


// ---- sampling ---------------------------------------------------------------

// returns a uniform int(32 bits) between 0 and n-1 , with n > 0
int uniform_int(ByteSource& src, int n) {
    assert(n > 0);
    uint32_t bound = static_cast<uint32_t>(n);
    // x = nq + r -> [0, r-1] is the non uniform part
    uint32_t rejected = (0u - bound) % bound;   // = 2^32 mod bound
    while (true) {
        uint32_t x = rand_u32(src);
        if (x >= rejected)
            return static_cast<int>(x % bound);
    }
}

// returns an uniform rand list of 0 <= t <= n different integers between 0 and n-1
// (not constant time)
std::vector<int> sample_distinct(ByteSource& src, int n, int t){
    assert(0 <= t && t <= n);
    // init [0, 1, 2, ..., n-1]
    std::vector<int> wlist(n);
    for(int i = 0; i < n; i++) wlist[i] = i;

    for(int i = 0; i < t; i++){
        int pos = i + uniform_int(src, n - i);
        std::swap(wlist[i], wlist[pos]);
    }

    // keep only t elements
    wlist.resize(t);
    return wlist;
}

} // namespace crypto
