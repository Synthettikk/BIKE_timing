#include "random_bike.h"
#include <cstdint>

std::mt19937 gen(std::random_device{}());

// returns a rand int(32 bits) between 0 and n-1 
int randint(int n){
    uint32_t x = gen();
    return (int)(n * (((double)x / (double)(1ULL << 32))));
}

// returns a uniform rand list of t different integers between 0 and n-1
// (not constant time)
std::vector<int> fisher_yates(int n, int t){
    // init [0, 1, 2, ..., n-1]
    std::vector<int> wlist(n);
    for(int i = 0; i < n; i++) wlist[i] = i;

    for(int i = 0; i < t; i++){
        int pos = i + randint(n - i);
        std::swap(wlist[i], wlist[pos]);
    }

    // keep only t elements
    wlist.resize(t);
    return wlist;
}
