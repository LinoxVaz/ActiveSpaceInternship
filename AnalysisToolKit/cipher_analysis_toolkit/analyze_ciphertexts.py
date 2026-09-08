#!/usr/bin/env python3
"""
Ciphertext analysis toolkit.

Usage:
    python analyze_ciphertexts.py --input . --output ./report

The script can analyze ciphertext files where they already are. Pass one or more
files and/or folders, and it will collect matching files recursively.

The script creates:
    - summary.csv
    - pairwise_comparison.csv
    - block_table.csv
    - report.html
    - plots/*.png
"""

from pathlib import Path
import argparse, math, hashlib, itertools, html
from collections import Counter
import pandas as pd
import matplotlib.pyplot as plt

def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    n = len(data)
    return -sum((c/n) * math.log2(c/n) for c in counts.values())

def hamming_bytes(a: bytes, b: bytes) -> int:
    return sum((x ^ y).bit_count() for x, y in zip(a, b))

def printable_ratio(data: bytes) -> float:
    if not data:
        return 0.0
    return sum(32 <= b <= 126 or b in (9, 10, 13) for b in data) / len(data)

def split_blocks(data: bytes, size: int = 16):
    return [data[i:i+size] for i in range(0, len(data), size)]

def pkcs7_guess(data: bytes):
    if not data:
        return "empty"
    pad = data[-1]
    if 1 <= pad <= 16 and len(data) >= pad and data[-pad:] == bytes([pad]) * pad:
        return f"possible PKCS#7 padding: {pad} byte(s)"
    return "no obvious PKCS#7 padding"

def repeated_overlapping(data: bytes, n: int):
    if len(data) < n:
        return 0
    seqs = [data[i:i+n] for i in range(0, len(data)-n+1)]
    counts = Counter(seqs)
    return sum(1 for c in counts.values() if c > 1)

def analyze_file(path: Path):
    data = path.read_bytes()
    blocks16 = split_blocks(data, 16)
    unique16 = len(set(blocks16))
    return {
        "file": path.name,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "entropy_bits_per_byte": round(entropy(data), 4),
        "ones_percent": round(100 * sum(b.bit_count() for b in data) / (len(data)*8), 2) if data else 0,
        "printable_ascii_percent": round(100 * printable_ratio(data), 2),
        "divisible_by_16": len(data) % 16 == 0,
        "blocks_16": len(blocks16),
        "unique_16_blocks": unique16,
        "repeated_16_blocks": len(blocks16) - unique16,
        "pkcs7_guess": pkcs7_guess(data),
        "repeated_2byte_overlapping": repeated_overlapping(data, 2),
        "repeated_3byte_overlapping": repeated_overlapping(data, 3),
        "repeated_4byte_overlapping": repeated_overlapping(data, 4),
        "repeated_8byte_overlapping": repeated_overlapping(data, 8),
        "repeated_16byte_overlapping": repeated_overlapping(data, 16),
    }

def compare_pair(p1: Path, p2: Path):
    a = p1.read_bytes()
    b = p2.read_bytes()
    n = min(len(a), len(b))
    ap, bp = a[:n], b[:n]
    xor = bytes(x ^ y for x, y in zip(ap, bp))
    hd = hamming_bytes(ap, bp)
    total_bits = n * 8
    equal_bytes = sum(x == y for x, y in zip(ap, bp))
    a16 = split_blocks(a, 16)
    b16 = split_blocks(b, 16)
    prefix_blocks = min(len(a16), len(b16))
    same_position_16 = sum(a16[i] == b16[i] for i in range(prefix_blocks))
    shared_16_values = len(set(a16).intersection(set(b16)))
    return {
        "file_a": p1.name,
        "file_b": p2.name,
        "bytes_a": len(a),
        "bytes_b": len(b),
        "compared_prefix_bytes": n,
        "equal_byte_positions": equal_bytes,
        "equal_byte_percent": round(100 * equal_bytes / n, 2) if n else 0,
        "hamming_distance_bits": hd,
        "hamming_distance_percent": round(100 * hd / total_bits, 2) if total_bits else 0,
        "xor_entropy": round(entropy(xor), 4),
        "xor_printable_ascii_percent": round(100 * printable_ratio(xor), 2),
        "same_position_16_blocks": same_position_16,
        "shared_16_block_values": shared_16_values,
    }

def collect_input_files(inputs, pattern: str = "*.bin"):
    files = []
    seen = set()
    for raw_input in inputs:
        path = Path(raw_input)
        if path.is_dir():
            candidates = sorted(path.rglob(pattern))
        elif path.is_file():
            candidates = [path]
        else:
            raise SystemExit(f"Input path not found: {path}")

        for candidate in candidates:
            if not candidate.is_file():
                continue
            resolved = candidate.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            files.append(candidate)

    return sorted(files)

def make_block_table(files):
    rows = []
    for path in files:
        data = path.read_bytes()
        for i, block in enumerate(split_blocks(data, 16)):
            rows.append({
                "file": path.name,
                "block_index": i,
                "offset": i * 16,
                "hex": block.hex(),
                "entropy": round(entropy(block), 4),
                "printable_ascii_percent": round(100 * printable_ratio(block), 2),
            })
    return pd.DataFrame(rows)

def plot_summary(summary_df, pair_df, block_df, outdir: Path):
    plotdir = outdir / "plots"
    plotdir.mkdir(exist_ok=True)

    # File sizes
    plt.figure(figsize=(9, 5))
    plt.bar(summary_df["file"], summary_df["bytes"])
    plt.title("Ciphertext sizes")
    plt.ylabel("Bytes")
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(plotdir / "file_sizes.png", dpi=180)
    plt.close()

    # Entropy
    plt.figure(figsize=(9, 5))
    plt.bar(summary_df["file"], summary_df["entropy_bits_per_byte"])
    plt.title("Entropy by file")
    plt.ylabel("Bits per byte")
    plt.ylim(0, 8)
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(plotdir / "entropy_by_file.png", dpi=180)
    plt.close()

    # Hamming distances
    if not pair_df.empty:
        pair_df = pair_df.copy()
        pair_df["pair"] = pair_df["file_a"].str.replace(".bin", "", regex=False) + " vs " + pair_df["file_b"].str.replace(".bin", "", regex=False)
        plt.figure(figsize=(10, 5))
        plt.bar(pair_df["pair"], pair_df["hamming_distance_percent"])
        plt.title("Pairwise Hamming distance")
        plt.ylabel("Changed bits (%)")
        plt.ylim(0, 100)
        plt.xticks(rotation=35, ha="right")
        plt.tight_layout()
        plt.savefig(plotdir / "pairwise_hamming.png", dpi=180)
        plt.close()

    # Block entropy scatter/line by file
    for file, df in block_df.groupby("file"):
        plt.figure(figsize=(9, 4))
        plt.plot(df["block_index"], df["entropy"], marker="o")
        plt.title(f"16-byte block entropy: {file}")
        plt.xlabel("Block index")
        plt.ylabel("Entropy")
        plt.ylim(0, 4.1)
        plt.tight_layout()
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in file)
        plt.savefig(plotdir / f"block_entropy_{safe}.png", dpi=180)
        plt.close()

def make_html(summary_df, pair_df, block_df, outdir: Path):
    plot_files = sorted((outdir / "plots").glob("*.png"))
    imgs = "\n".join(f'<h3>{html.escape(p.stem)}</h3><img src="plots/{html.escape(p.name)}" style="max-width:900px;width:100%;border:1px solid #ddd;">' for p in plot_files)
    conclusion = """
    <ul>
      <li>If every file length is divisible by 16, treat the data as block-aligned ciphertext.</li>
      <li>If there are no repeated 16-byte blocks, ECB is not proven and simple repeated-block leakage is not visible.</li>
      <li>Pairwise Hamming distances near 50% suggest strong diffusion or unrelated keys/IVs.</li>
      <li>No obvious PKCS#7 padding does not eliminate AES/CBC/ECB, but it means the final byte pattern does not reveal padding directly.</li>
      <li>The next practical step is to test candidate algorithms/modes with any key/password hints, not to brute-force random AES keys.</li>
    </ul>
    """
    html_text = f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>Ciphertext Analysis Report</title>
<style>
body {{ font-family: Arial, sans-serif; margin: 32px; line-height: 1.45; }}
table {{ border-collapse: collapse; width: 100%; margin-bottom: 24px; font-size: 13px; }}
th, td {{ border: 1px solid #ddd; padding: 6px 8px; text-align: left; }}
th {{ background: #f2f2f2; }}
code {{ background: #f5f5f5; padding: 2px 4px; }}
</style>
</head>
<body>
<h1>Ciphertext Analysis Report</h1>
<h2>File summary</h2>
{summary_df.to_html(index=False, escape=True)}
<h2>Pairwise comparison</h2>
{pair_df.to_html(index=False, escape=True)}
<h2>Interpretation checklist</h2>
{conclusion}
<h2>Plots</h2>
{imgs}
<h2>16-byte block table</h2>
<p>Full table is also saved as <code>block_table.csv</code>.</p>
{block_df.head(80).to_html(index=False, escape=True)}
</body>
</html>"""
    (outdir / "report.html").write_text(html_text, encoding="utf-8")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        nargs="+",
        default=["."],
        help="One or more files and/or folders to analyze",
    )
    parser.add_argument(
        "--pattern",
        default="*.bin",
        help="Glob pattern used when an input path is a folder",
    )
    parser.add_argument("--output", required=True, help="Output report folder")
    args = parser.parse_args()

    outdir = Path(args.output)
    outdir.mkdir(parents=True, exist_ok=True)

    files = collect_input_files(args.input, args.pattern)
    if not files:
        raise SystemExit("No matching files found in the provided input path(s)")

    summary_df = pd.DataFrame([analyze_file(p) for p in files])
    pair_df = pd.DataFrame([compare_pair(a, b) for a, b in itertools.combinations(files, 2)])
    block_df = make_block_table(files)

    summary_df.to_csv(outdir / "summary.csv", index=False)
    pair_df.to_csv(outdir / "pairwise_comparison.csv", index=False)
    block_df.to_csv(outdir / "block_table.csv", index=False)

    plot_summary(summary_df, pair_df, block_df, outdir)
    make_html(summary_df, pair_df, block_df, outdir)

    print(f"Report written to: {outdir.resolve()}")
    print(f"Open: {outdir.resolve() / 'report.html'}")

if __name__ == "__main__":
    main()
