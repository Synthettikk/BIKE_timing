#!/usr/bin/env python3
"""Plot failure rate and decoding cost vs error weight t from a harness CSV.

Usage: python scripts/plot_results.py results/bitflip_v2.csv
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    csv = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else csv.parent
    out.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(csv)
    decoders = list(df["decoder"].unique())          # keeps CSV order
    title = f"r={df['r'].iloc[0]}, w/2={df['w_half'].iloc[0]}, {df['trials'].iloc[0]} trials"

    # 1) failure rate vs t, with 95% Wilson intervals
    fig, ax = plt.subplots(figsize=(8, 5))
    for i, name in enumerate(decoders):
        d = df[df["decoder"] == name].sort_values("t")
        x = d["t"] + 0.3 * (i - (len(decoders) - 1) / 2)   # small offset so the bars don't overlap
        yerr = [d["fail_rate"] - d["fail_lo"], d["fail_hi"] - d["fail_rate"]]
        ax.errorbar(x, d["fail_rate"], yerr=yerr, marker="o", capsize=3, label=name)
    ax.set_xlabel("error weight t")
    ax.set_ylabel("failure rate")
    ax.set_ylim(-0.02, 1.02)
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / (csv.stem + "_failrate.png"), dpi=150)

    # 2) cost (counters computed) vs t, successes only
    fig, ax = plt.subplots(figsize=(8, 5))
    for name in decoders:
        d = df[(df["decoder"] == name) & (df["failures"] < df["trials"])].sort_values("t")
        if len(d):
            ax.plot(d["t"], d["mean_counters"], marker="o", label=name)
    ax.set_xlabel("error weight t")
    ax.set_ylabel("mean counters computed (successes only)")
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / (csv.stem + "_cost.png"), dpi=150)

    plt.show()


if __name__ == "__main__":
    main()
    