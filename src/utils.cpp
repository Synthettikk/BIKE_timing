#include "NTL/GF2X.h"
#include "NTL/GF2E.h"
#include <cassert>
#include "utils.h"

using namespace NTL;

int r = 0;
int n = 0;

GF2E GF2E_bike::from_support(const std::vector<int>& pos) {
    GF2X poly;
    for (int k : pos) {
        assert(0 <= k && k < r);
        SetCoeff(poly, k);          
    }
    return to_GF2E(poly);
}

GF2E_bike::GF2E_bike() : v0(), v1() {}   // GF2E default is 0

// takes the positions of the 1s of v0, v1, and returns a couple GF2Es
GF2E_bike::GF2E_bike(const std::vector<int>& v0_pos, const std::vector<int>& v1_pos)
    : v0(from_support(v0_pos)), v1(from_support(v1_pos)) {}

// takes the support of (v0, v1), and returns a couple GF2Es
GF2E_bike::GF2E_bike(const Support& sup)
    : v0(from_support(sup[0])), v1(from_support(sup[1])) {}

Support GF2E_bike::support() const {
    Support sup;
    for (int b = 0; b < 2; b++) {
        const GF2X& p = rep(block(b));
        for (int k = 0; k <= deg(p); k++)
            if (IsOne(coeff(p, k))) sup[b].push_back(k);
    }
    return sup;
}

GF2E GF2E_bike::from_bits(const std::vector<int>& bit_vec) {
    GF2X poly;
    for(int i = 0; i < r; i++) {
        if(bit_vec[i]) SetCoeff(poly, i);
    }
    return to_GF2E(poly);
}

std::vector<int> GF2E_bike::to_bits(const GF2E& v) {
    const GF2X& poly = rep(v);
    std::vector<int> bits(r, 0);
    for(int i = 0; i < r; i++){
        bits[i] = coeff(poly, i) == GF2(1);
    }
    return bits;
}

std::vector<uint8_t> GF2E_bike::to_bytes(const GF2E& a) {
    const GF2X& p = rep(a);
    std::vector<uint8_t> out(static_cast<size_t>((r + 7) / 8), 0);
    for (int i = 0; i <= deg(p); i++)          // deg(0) = -1: the loop does nothing
        if (IsOne(coeff(p, i)))
            out[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
    return out;
}

GF2E GF2E_bike::from_bytes(const std::vector<uint8_t>& b) {
    assert(b.size() == static_cast<size_t>((r + 7) / 8));
    GF2X p;
    for (int i = 0; i < r; i++)
        if ((b[i / 8] >> (i % 8)) & 1)
            SetCoeff(p, i);
    return to_GF2E(p);
}

GF2 GF2E_bike::coef_poly(GF2E v, int i) {
    return coeff(rep(v), i);
}

GF2 GF2E_bike::coef_matrix(int i, int j) const {
    GF2E part = j < r ? v0 : v1;
    int j_local = j < r ? j : (j - r); 
    return coef_poly(part, (i - j_local + r) % r);
}

GF2E GF2E_bike::mult(const GF2E_bike& other) const {
    return v0 * other.v0 + v1 * other.v1; // s = e0·h0 + e1·h1
}

void GF2E_bike::flip_bit(int j) {
    GF2E& target = j < r ? v0 : v1;
    GF2X poly = rep(target);
    SetCoeff(poly, j % r, 1 - coeff(poly, j % r));
    target = to_GF2E(poly);
}

void GF2E_bike::print() const {
    std::vector<int> bits0 = to_bits(v0);
    std::vector<int> bits1 = to_bits(v1);

    for(int i = 0; i < r; i++){
        std::cout << bits0[i] << ", ";
    }
    std::cout << "| ";
    for(int i = 0; i < r; i++){
        std::cout << bits1[i] << ", ";
    }
    std::cout << "\n";
}


// init GF2E with P = X^r + 1
void init_gf2e(int r0){
    GF2X P;
    SetCoeff(P, 0);  // coef of 1 → P = ... + 1
    SetCoeff(P, r0);  // coeff of X^n → P = X^r + ...
    
    GF2E::init(P);  // init GF2E with P = X^r + 1 = X^r - 1 in GF(2)

    r = r0;
    n = 2 * r;
}

void print_gf2e(const GF2E& v) {
    for (int i = 0; i < r; i++) {
        std::cout << coeff(rep(v), i);
    }
}
