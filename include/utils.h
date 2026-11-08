// v0, v1 = h0, h1 = first COLUMNS of the circulant blocks of H, i.e.
// H = (C(h0)^T | C(h1)^T) with C(h) the row-circulant of h.
// Syndrome: s = e0*h0 + e1*h1 (polynomial product in F_2[X]/(X^r - 1)).
// H[i][j] = h_b[(i - j) mod r].

#ifndef UTILS_H
#define UTILS_H

#include "NTL/GF2X.h"
#include "NTL/GF2E.h"
#include <array>
#include <vector>

extern int r, n;

using Support = std::array<std::vector<int>, 2>;

class GF2E_bike {
private:
    NTL::GF2E v0, v1;

    static NTL::GF2E from_bits(const std::vector<int>& bit_vec);
    
public:
    bool is_zero() const { return NTL::IsZero(v0) && NTL::IsZero(v1); }
    bool operator==(const GF2E_bike& o) const { return v0 == o.v0 && v1 == o.v1; } // not CT
    bool operator!=(const GF2E_bike& o) const { return !(*this == o); }

    static std::vector<int> to_bits(const NTL::GF2E& v);

    // Polynomial <-> bytes (ceil(r/8) bytes). The coefficient of X^i is bit (i % 8)
    // of byte i / 8; the padding bits of the last byte are 0.
    static std::vector<uint8_t> to_bytes(const NTL::GF2E& a);
    static NTL::GF2E from_bytes(const std::vector<uint8_t>& b);

    const NTL::GF2E& block(int b) const { return b == 0 ? v0 : v1; }
    GF2E_bike(); // default is 0
    GF2E_bike(const std::vector<int>& v0_pos, const std::vector<int>& v1_pos); 
    GF2E_bike(const Support& sup); 
    NTL::GF2E from_support(const std::vector<int>& pos);
    Support support() const;
    NTL::GF2 coef_matrix(int i, int j) const;
    static NTL::GF2 coef_poly(NTL::GF2E v, int i);
    NTL::GF2E mult(const GF2E_bike& other) const;
    void flip_bit(int j);
    void print() const;
};

static inline int weight(const NTL::GF2E& s){
    int w = 0;
    for (int b : GF2E_bike::to_bits(s)) w += b;
    return w;
}

// init GF2E with P = X^r + 1
void init_gf2e(int r0);

void print_gf2e(const NTL::GF2E& v);

// void test_GF2E_bike();

// fct rot qui fait tourner la matrice QC 
// eventuellement des fcts qui encapsulent les types GF2E, la generation de vecteurs etc

#endif
