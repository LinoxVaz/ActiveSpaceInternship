from pathlib import Path
from collections import Counter
import string
import math

KNOWN_PLAINTEXT = "InternshipVIII_T004.txt"
KNOWN_CIPHERTEXT = "InternshipVIII_T004.bin"

DIRECTORY = "."

# ------------------------------------------------------------

pt = Path(KNOWN_PLAINTEXT).read_bytes()
ct = Path(KNOWN_CIPHERTEXT).read_bytes()

keystream = bytes(a ^ b for a, b in zip(pt, ct))

print("=" * 80)
print(f"Recovered keystream length: {len(keystream)} bytes")
print("=" * 80)

# ------------------------------------------------------------

COMMON_WORDS = [
    " de ",
    " que ",
    " e ",
    " o ",
    " a ",
    " para ",
    " com ",
    " uma ",
    " um ",
    " não ",
    " por ",
    " os ",
    " as ",
    " se ",
    " do ",
    " da ",
    " dos ",
    " das ",
]

PRINTABLE = set(bytes(string.printable, "ascii"))

# ------------------------------------------------------------


def printable_ratio(data):

    good = 0

    for b in data:

        if (
            b in PRINTABLE
            or b >= 0x80          # allow UTF-8 continuation bytes
            or b in (10, 13, 9)
        ):
            good += 1

    return good / len(data)


def utf8_score(data):

    try:
        data.decode("utf-8")
        return 1.0
    except:
        return 0.0


def portuguese_score(data):

    try:
        text = data.decode("utf-8", errors="ignore").lower()
    except:
        return 0

    score = 0

    for word in COMMON_WORDS:

        score += text.count(word) * 20

    letters = "aeiosrndmtuclp"

    for c in letters:

        score += text.count(c)

    return score


# ------------------------------------------------------------

for file in sorted(Path(DIRECTORY).glob("*.bin")):

    if file.name == KNOWN_CIPHERTEXT:
        continue

    print()
    print("=" * 80)
    print(file.name)
    print("=" * 80)

    cipher = file.read_bytes()

    maximum = min(len(cipher), len(keystream))

    best = None

    for offset in range(0, len(keystream) - maximum + 1):

        decrypted = bytes(
            cipher[i] ^ keystream[offset + i]
            for i in range(maximum)
        )

        score = (
            printable_ratio(decrypted) * 100
            + utf8_score(decrypted) * 100
            + portuguese_score(decrypted)
        )

        if best is None or score > best[0]:

            best = (
                score,
                offset,
                decrypted,
            )

    score, offset, decrypted = best

    print(f"Best offset : {offset}")
    print(f"Score       : {score:.2f}")

    print()

    try:
        preview = decrypted.decode("utf-8", errors="replace")
    except:
        preview = str(decrypted)

    print(preview[:500])