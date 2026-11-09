import glob
import matplotlib.pyplot as plt
import pandas as pd

fig, ax = plt.subplots(figsize=(7, 4.5))
for f in sorted(glob.glob("results/timing_*_curve.csv")):
    df = pd.read_csv(f)
    t = df[["t_raw", "t_crop"]].abs().max(axis=1)      # keeps the largest |t|
    ax.plot(df["n"], t, marker="o", ms=3, label=df["test"].iloc[0])
ax.axhline(4.5, ls=":", c="gray")
ax.axhline(10, ls="--", c="gray")
ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xlabel("number of measurements")
ax.set_ylabel("|t| (Welch)")
ax.set_title("timing on decaps")
ax.legend()
fig.savefig("docs/figures/timing_decaps.png", dpi=150, bbox_inches="tight")
