# Ciphertext Analysis Toolkit

## What this does

This automatically analyzes `.bin` ciphertexts where they already are and produces:

- `summary.csv`
- `pairwise_comparison.csv`
- `block_table.csv`
- `report.html`
- plot images in `plots/`

## How to use on Windows

1. Open the folder that already contains the ciphertext files.
2. Install dependencies:

```bash
pip install pandas matplotlib
```

3. Run:

```bash
python analyze_ciphertexts.py --input . --output report
```

4. Open:

```text
report/report.html
```

If you want to target only the internship files, use:

```bash
python analyze_ciphertexts.py --input . --pattern InternshipVIII_T*.bin --output report
```

## What to look for

- All sizes divisible by 16 means block-style encryption is likely.
- Repeated 16-byte blocks would be evidence for ECB-like behavior.
- Hamming distance near 50% between files suggests strong diffusion / different keys / different IVs.
- If changing a small plaintext detail changes only the same byte positions, think stream cipher.
- If it changes whole blocks or following blocks, think block cipher mode such as CBC/CFB.
