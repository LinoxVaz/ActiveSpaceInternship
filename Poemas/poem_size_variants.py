from pathlib import Path
import unicodedata

INPUT_FILE = Path("Amor é um fogo que arde sem se ver;.txt")
TARGET_SIZE = 560
OUTPUT_DIR = Path("poem_variants")

OUTPUT_DIR.mkdir(exist_ok=True)

raw = INPUT_FILE.read_bytes()

print("=" * 60)
print("ORIGINAL FILE")
print("=" * 60)
print(f"Size:              {len(raw)} bytes")
print(f"UTF-8 BOM:         {raw.startswith(bytes.fromhex('EFBBBF'))}")
print(f"CRLF occurrences:  {raw.count(b'\\r\\n')}")
print(f"LF occurrences:    {raw.count(b'\\n')}")
print(f"CR occurrences:    {raw.count(b'\\r')}")
print(f"Final LF:          {raw.endswith(b'\\n')}")
print(f"Final CRLF:        {raw.endswith(b'\\r\\n')}")

try:
    text = raw.decode("utf-8-sig")
    print("Decoded as:        UTF-8")
except UnicodeDecodeError:
    text = raw.decode("cp1252")
    print("Decoded as:        Windows-1252")

print(f"Characters:        {len(text)}")
print(f"Lines:             {len(text.splitlines())}")
print()

# Convert all existing line endings to LF internally.
base_text = text.replace("\r\n", "\n").replace("\r", "\n")

normalizations = {
    "NFC": unicodedata.normalize("NFC", base_text),
    "NFD": unicodedata.normalize("NFD", base_text),
}

encodings = {
    "utf8": "utf-8",
    "cp1252": "cp1252",
    "latin1": "latin-1",
}

newline_modes = {
    "LF": "\n",
    "CRLF": "\r\n",
}

results = []

for normalization_name, normalized_text in normalizations.items():
    for newline_name, newline in newline_modes.items():

        converted = normalized_text.replace("\n", newline)

        for final_newline in (False, True):
            candidate_text = converted

            if final_newline:
                if not candidate_text.endswith(newline):
                    candidate_text += newline
            else:
                while candidate_text.endswith(newline):
                    candidate_text = candidate_text[:-len(newline)]

            for encoding_name, encoding in encodings.items():
                for bom in (False, True):

                    # BOM is meaningful only for UTF-8.
                    if bom and encoding != "utf-8":
                        continue

                    try:
                        candidate = candidate_text.encode(encoding)
                    except UnicodeEncodeError:
                        continue

                    if bom:
                        candidate = bytes.fromhex("EFBBBF") + candidate

                    name = (
                        f"{normalization_name}_"
                        f"{newline_name}_"
                        f"{encoding_name}_"
                        f"{'BOM' if bom else 'noBOM'}_"
                        f"{'finalNL' if final_newline else 'noFinalNL'}"
                    )

                    output_path = OUTPUT_DIR / f"{name}.txt"
                    output_path.write_bytes(candidate)

                    difference = len(candidate) - TARGET_SIZE
                    results.append((abs(difference), len(candidate), name, output_path))

results.sort()

print("=" * 60)
print("CLOSEST VARIANTS TO 560 BYTES")
print("=" * 60)

for _, size, name, path in results[:20]:
    marker = "  <== EXACT MATCH" if size == TARGET_SIZE else ""
    print(f"{size:4} bytes | {name}{marker}")

exact = [item for item in results if item[1] == TARGET_SIZE]

print()
if exact:
    print("Exact 560-byte variants found:")
    for _, size, name, path in exact:
        print(f"  {path}")
else:
    print("No exact 560-byte representation was produced.")
    print("This means the text itself probably differs:")
    print("- title or author included")
    print("- extra blank lines")
    print("- different punctuation")
    print("- trailing spaces")
    print("- different poem version")