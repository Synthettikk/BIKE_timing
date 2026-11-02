#ifndef RANDBIKE_H
#define RANDBIKE_H

#include <random>
#include <vector>

extern std::mt19937 gen;


// returns a rand int(32 bits) between 0 and n-1 
int randint(int n);

// returns a uniform rand list of t different integers between 0 and n-1
std::vector<int> fisher_yates(int n, int t);

#endif 
