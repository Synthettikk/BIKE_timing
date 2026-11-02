# BIKE Implementation and Timing Leaks

This project aims to implement the BIKE cryptosystem (based on the Bit-Flipping algorithm) without constant time and show that a timing leak appears, using dudect. We also compare the Bit-Flipping algorithm and the Black-Gray-Flip decoder. 

The documentation we used can be found here : 
BIKE specification v4.2, <https://bikesuite.org/files/v4.2/BIKE_Spec.2021.07.26.1.pdf> (threshold function, BGF parameters).

## Status

| | |
|---|---|
| Circulant arithmetic in F₂[X]/(Xʳ − 1) (NTL) | done |
| Bit-flip decoder (fixed or variable threshold) | done |
| Black-Gray-Flip (BGF) decoder | done |
| Experiment harness and decoder comparison | done ([report](docs/report.md)) |
| KeyGen / Encaps / Decaps | planned |
| Unit tests, timing-leakage check with dudect | planned |
| Own GF(2)[X] arithmetic (replacing NTL) | planned |

> This implementation is not constant-time, not bit-exact with the
> BIKE specification (no KAT), and must not be used to protect real data.


## Results 

### Decoders comparison 

![Failure rate versus error weight](docs/figures/compare_v1_failrate.png)

At BIKE's error weight (t = 134), BGF decodes all 100 samples with 26% fewer
counters than bit-flipping with the same threshold function. Details, limits and
reproduction steps are in [docs/report.md](docs/report.md).

### Timing leakage (dudect)

(In progress)

## Implementation notes

- **Representation.** A vector of length 2r is a pair $(v0, v1)$ of elements of
  $\mathbb{F}_2[X]/(X^r − 1)$, stored as NTL `GF2E`. The syndrome of an error $e$ is
  $s = e0·h0 + e1·h1$, with $h0, h1$ the first columns of the circulant blocks of $H$.
- **Counters.** The number of unsatisfied checks of position j is a cyclic
  correlation between the support of $h$ (71 ones) and the syndrome, computed over
  the integers. Using the sparse support costs about w·r operations per pass,
  instead of $2r^2$ with a dense matrix.
- **Decoders.** Parallel bit-flip (all counters on the same syndrome, then flip),
  with a fixed threshold or the BIKE threshold function of the syndrome weight, and
  BGF with 5 iterations and black/gray masked steps.
- **Evaluation.** All decoders are run on identical random samples. Cost is counted
  in counters computed, so the comparison does not depend on the machine or on NTL.


## Installation

### Build

Requirements: a C++17 compiler, CMake ≥ 3.10, [NTL](https://libntl.org/) and GMP.

```bash
# Debian / Ubuntu / Linux Mint
sudo apt install build-essential cmake libntl-dev libgmp-dev

git clone <repo-url>
cd BIKE_timing

cmake -S . -B build        # configure (creates build/, Release mode by default)
cmake --build build        # compile everything
```

Executables are written to `build/`.

### Run

```bash
./build/main                                   # demo
./build/exp_bit_flip > results/bitflip_v2.csv  # bit-flip experiment (CSV on stdout)
./build/exp_bgf      > results/bgf_v1.csv      # BGF experiment
```

The experiments take several minutes. Progress is printed on stderr, so the CSV stays clean.

### Tests

```bash
ctest --test-dir build --output-on-failure
```

### Plots (optional)

```bash
sudo apt install python3-pandas python3-matplotlib
python3 scripts/plot_results.py results/compare_v1.csv docs/figures
```

See [docs/report.md](docs/report.md) for the analysis and how to reproduce it.

## Projet structure 

```python
BIKE_timing/
├── docs/
│   ├── figures/
│   │   ├── compare_v1_cost.png
│   │   └── compare_v1_failrate.png
│   └── report.md
├── experiments/            #  decoder comparison etc
│   ├── exp_bgf.cpp
│   ├── exp_bit_flip.cpp
│   ├── harness.cpp
│   └── harness.h
├── include/
│   ├── decoder.h           # bit_flip, bgf_decoder, threshold functions
│   ├── random_bike.h
│   └── utils.h             # GF2E_bike, Support, hamming_weight
├── results/                # raw CSV behind the report
│   ├── bgf_v1.csv
│   ├── bitflip_v1.csv
│   ├── bitflip_v2.csv
│   └── compare_v1.csv
├── scripts/
│   └── plot_results.py     # CSV -> PNG
├── src/
│   ├── BIKE.cpp            #   main()
│   ├── decoder.cpp
│   ├── random_bike.cpp
│   └── utils.cpp
├── tests/                  # unit tests, run with ctest
├── .gitignore
├── CMakeLists.txt
├── LICENSE
└── README.md
```






