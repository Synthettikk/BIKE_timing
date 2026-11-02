// We will need the following operations in the ring F2[X]/(Xn - 1):
// — Addition,
// — Multiplication,
// — Inversion.
// All of these operations can be performed using the GF2E type from the NTL library, which allows
// us to represent polynomials with coefficients in F2 modulo a polynomial P
// however NTL is not constant time 