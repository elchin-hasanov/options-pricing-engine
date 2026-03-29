import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.tri import Triangulation
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401


CSV_PATH = "data/processed/spy_iv_surface.csv"
OUTPUT_DIR = "data/processed"


def load_and_clean_data(csv_path: str) -> pd.DataFrame:
    df = pd.read_csv(csv_path)

    required_cols = [
        "asOfDate",
        "expiration",
        "contractSymbol",
        "spot",
        "strike",
        "maturity",
        "marketPrice",
        "impliedVol",
    ]

    missing = [col for col in required_cols if col not in df.columns]
    if missing:
        raise ValueError(f"Missing required CSV columns: {missing}")

    df = df.dropna(subset=["strike", "maturity", "marketPrice", "impliedVol"])
    df = df[df["strike"] > 0]
    df = df[df["maturity"] > 0]
    df = df[df["marketPrice"] > 0]
    df = df[df["impliedVol"] > 0]
    df = df[df["impliedVol"] < 5.0]

    df = df.sort_values(["expiration", "strike"]).reset_index(drop=True)

    if df.empty:
        raise ValueError("No valid rows remain after cleaning.")

    return df


def save_summary(df: pd.DataFrame) -> None:
    summary_path = os.path.join(OUTPUT_DIR, "spy_iv_surface_summary.txt")

    expiries = df["expiration"].nunique()
    points = len(df)
    strike_min = df["strike"].min()
    strike_max = df["strike"].max()
    mat_min = df["maturity"].min()
    mat_max = df["maturity"].max()
    iv_min = df["impliedVol"].min()
    iv_max = df["impliedVol"].max()

    with open(summary_path, "w") as f:
        f.write("SPY Implied Volatility Surface Summary\n")
        f.write("=" * 40 + "\n")
        f.write(f"Points: {points}\n")
        f.write(f"Unique expirations: {expiries}\n")
        f.write(f"Strike range: {strike_min:.4f} to {strike_max:.4f}\n")
        f.write(f"Maturity range: {mat_min:.6f} to {mat_max:.6f}\n")
        f.write(f"IV range: {iv_min:.6f} to {iv_max:.6f}\n")
        f.write("\nPoints by expiration:\n")
        counts = df.groupby("expiration").size()
        for exp, count in counts.items():
            f.write(f"  {exp}: {count}\n")


def plot_3d_scatter(df: pd.DataFrame):
    fig = plt.figure(figsize=(11, 8))
    ax = fig.add_subplot(111, projection="3d")

    x = df["strike"].to_numpy()
    y = df["maturity"].to_numpy()
    z = df["impliedVol"].to_numpy()

    ax.scatter(x, y, z, s=28)

    ax.set_title("SPY Implied Volatility Surface (3D Scatter)")
    ax.set_xlabel("Strike")
    ax.set_ylabel("Maturity (Years)")
    ax.set_zlabel("Implied Volatility")

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "spy_iv_surface_scatter.png"), dpi=220)
    return fig


def plot_3d_trisurface(df: pd.DataFrame):
    fig = plt.figure(figsize=(11, 8))
    ax = fig.add_subplot(111, projection="3d")

    x = df["strike"].to_numpy()
    y = df["maturity"].to_numpy()
    z = df["impliedVol"].to_numpy()

    if len(df) < 3:
        raise ValueError("Need at least 3 valid points for trisurface plot.")

    triang = Triangulation(x, y)
    ax.plot_trisurf(triang, z, alpha=0.9)

    ax.set_title("SPY Implied Volatility Surface")
    ax.set_xlabel("Strike")
    ax.set_ylabel("Maturity (Years)")
    ax.set_zlabel("Implied Volatility")

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "spy_iv_surface_trisurf.png"), dpi=220)
    return fig


def plot_smiles_by_expiry(df: pd.DataFrame, max_expiries: int = 12):
    expiries = list(df["expiration"].drop_duplicates())[:max_expiries]

    fig, ax = plt.subplots(figsize=(11, 7))

    for expiry in expiries:
        sub = df[df["expiration"] == expiry].sort_values("strike")
        if len(sub) < 2:
            continue
        ax.plot(sub["strike"], sub["impliedVol"], marker="o", label=expiry)

    ax.set_title("SPY Volatility Smiles by Expiration")
    ax.set_xlabel("Strike")
    ax.set_ylabel("Implied Volatility")
    ax.legend(fontsize=8)
    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "spy_iv_smiles.png"), dpi=220)
    return fig


def plot_term_structure_near_atm(df: pd.DataFrame):
    spot = float(df["spot"].iloc[0])

    sub = df.copy()
    sub["atmDistance"] = (sub["strike"] - spot).abs()
    atm = sub.sort_values(["expiration", "atmDistance"]).groupby("expiration").head(1)
    atm = atm.sort_values("maturity")

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(atm["maturity"], atm["impliedVol"], marker="o")

    for _, row in atm.iterrows():
        ax.annotate(
            row["expiration"],
            (row["maturity"], row["impliedVol"]),
            fontsize=8,
            xytext=(4, 4),
            textcoords="offset points",
        )

    ax.set_title("ATM-ish Implied Volatility Term Structure")
    ax.set_xlabel("Maturity (Years)")
    ax.set_ylabel("Implied Volatility")

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "spy_iv_term_structure.png"), dpi=220)
    return fig


def plot_heatmap_like_surface(df: pd.DataFrame):
    pivot = df.pivot_table(
        index="maturity",
        columns="strike",
        values="impliedVol",
        aggfunc="mean"
    )

    if pivot.shape[0] < 2 or pivot.shape[1] < 2:
        return None

    fig, ax = plt.subplots(figsize=(11, 7))
    im = ax.imshow(
        pivot.to_numpy(),
        aspect="auto",
        origin="lower",
        interpolation="nearest"
    )

    ax.set_title("SPY Implied Volatility Heatmap")
    ax.set_xlabel("Strike")
    ax.set_ylabel("Maturity Index")

    x_ticks = np.arange(len(pivot.columns))
    x_labels = [f"{v:.0f}" for v in pivot.columns]
    step_x = max(1, len(x_ticks) // 10)
    ax.set_xticks(x_ticks[::step_x])
    ax.set_xticklabels(x_labels[::step_x])

    y_ticks = np.arange(len(pivot.index))
    y_labels = [f"{v:.4f}" for v in pivot.index]
    step_y = max(1, len(y_ticks) // 10)
    ax.set_yticks(y_ticks[::step_y])
    ax.set_yticklabels(y_labels[::step_y])

    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label("Implied Volatility")

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "spy_iv_heatmap.png"), dpi=220)
    return fig


def write_clean_csv(df: pd.DataFrame) -> None:
    out_path = os.path.join(OUTPUT_DIR, "spy_iv_surface_clean.csv")
    df.to_csv(out_path, index=False)


def main() -> None:
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    df = load_and_clean_data(CSV_PATH)
    write_clean_csv(df)
    save_summary(df)

    print(f"Loaded {len(df)} cleaned rows.")
    print(f"Unique expirations: {df['expiration'].nunique()}")
    print(f"Strike range: {df['strike'].min()} to {df['strike'].max()}")
    print(f"Maturity range: {df['maturity'].min():.6f} to {df['maturity'].max():.6f}")
    print(f"IV range: {df['impliedVol'].min():.6f} to {df['impliedVol'].max():.6f}")

    figures = []
    figures.append(plot_3d_scatter(df))
    figures.append(plot_3d_trisurface(df))
    figures.append(plot_smiles_by_expiry(df))
    figures.append(plot_term_structure_near_atm(df))

    heatmap_fig = plot_heatmap_like_surface(df)
    if heatmap_fig is not None:
        figures.append(heatmap_fig)

    print("Saved:")
    print("  data/processed/spy_iv_surface_clean.csv")
    print("  data/processed/spy_iv_surface_summary.txt")
    print("  data/processed/spy_iv_surface_scatter.png")
    print("  data/processed/spy_iv_surface_trisurf.png")
    print("  data/processed/spy_iv_smiles.png")
    print("  data/processed/spy_iv_term_structure.png")
    if heatmap_fig is not None:
        print("  data/processed/spy_iv_heatmap.png")

    plt.show()


if __name__ == "__main__":
    main()