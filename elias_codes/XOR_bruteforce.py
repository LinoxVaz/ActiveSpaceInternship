import os
import string


INPUT_FILE = "D:\\UC_library\\Active_Space\\Encrypted\\InternshipVIII_T001.bin"
OUTPUT_DIR = "D:\\UC_library\\Active_Space\\Encrypted\\XOR_results"

# Number of best candidates to save for each attack
TOP_RESULTS = 20


# ---------------------------------------------------------
# XOR decryption
# ---------------------------------------------------------

def xor_decrypt(data, key):
    """
    XOR the data with a repeating key.
    """
    key_len = len(key)

    return bytes(
        data[i] ^ key[i % key_len]
        for i in range(len(data))
    )


# ---------------------------------------------------------
# Plaintext scoring
# ---------------------------------------------------------

def score_plaintext(data):
    """
    Give a score indicating how likely the data is readable text.

    Higher score = more likely to be plaintext.
    """

    if not data:
        return 0

    score = 0

    printable = set(bytes(string.printable, "ascii"))

    # Printable ASCII characters
    for byte in data:
        if byte in printable:
            score += 1

    # Penalize null bytes and other control characters
    for byte in data:
        if byte == 0:
            score -= 5
        elif byte < 9:
            score -= 3

    # Common characters
    common_chars = b" etaoinshrdluETAOINSHRDLU"

    for byte in data:
        if byte in common_chars:
            score += 2

    # Common Portuguese words / sequences
    common_patterns = [ b" que ", 
                        b" de ", 
                        b" do ", 
                        b" da ", 
                        b" dos ", 
                        b" das ", 
                        b" em ", 
                        b" um ", 
                        b" uma ", 
                        b" para ", 
                        b" por ", 
                        b" com ", 
                        b" no ", 
                        b" na ", 
                        b" nos ", 
                        b" nas ", 
                        b" se ", 
                        b" ao ", 
                        b" aos ", 
                        b" ou ", 
                        b" e ", 
                        b" mas ", 
                        b" como ", 
                        b" mais ", 
                        b" muito "]

    for pattern in common_patterns:
        score += data.lower().count(pattern.lower()) * 20

    return score


# ---------------------------------------------------------
# Save candidate
# ---------------------------------------------------------

def save_candidate(key, plaintext, score, key_length):
    """
    Save a candidate plaintext to disk.
    """

    key_hex = key.hex()

    filename = (
        f"xor_{key_length}byte_"
        f"key_{key_hex}_"
        f"score_{score}.bin"
    )

    path = os.path.join(OUTPUT_DIR, filename)

    with open(path, "wb") as f:
        f.write(plaintext)

    return path


# ---------------------------------------------------------
# Single-byte brute force
# ---------------------------------------------------------

def brute_force_one_byte(data):
    print("\n===================================")
    print(" ONE-BYTE XOR BRUTE FORCE")
    print("===================================\n")

    results = []

    for key_value in range(256):

        key = bytes([key_value])

        plaintext = xor_decrypt(data, key)

        score = score_plaintext(plaintext)

        results.append(
            (score, key, plaintext)
        )

    # Highest scores first
    results.sort(reverse=True, key=lambda x: x[0])

    print("Top candidates:\n")

    for rank, (score, key, plaintext) in enumerate(
        results[:TOP_RESULTS], 1
    ):

        print(
            f"{rank:2d}. "
            f"Key = 0x{key.hex()} "
            f"({key[0]:3d}) "
            f"Score = {score}"
        )

        # Show first 100 bytes
        preview = plaintext[:100]

        try:
            print(
                "    ",
                preview.decode("utf-8", errors="replace")
            )
        except Exception:
            pass

        path = save_candidate(
            key,
            plaintext,
            score,
            1
        )

        print("    Saved:", path)
        print()

    return results


# ---------------------------------------------------------
# Two-byte brute force
# ---------------------------------------------------------

def brute_force_two_byte(data):
    print("\n===================================")
    print(" TWO-BYTE XOR BRUTE FORCE")
    print("===================================\n")

    results = []

    # 0x0000 -> 0xFFFF
    for key_value in range(65536):

        # Big-endian key:
        # 0x1234 -> b'\x12\x34'
        key = key_value.to_bytes(2, "big")

        plaintext = xor_decrypt(data, key)

        score = score_plaintext(plaintext)

        results.append(
            (score, key, plaintext)
        )

    # Highest scores first
    results.sort(reverse=True, key=lambda x: x[0])

    print("Top candidates:\n")

    for rank, (score, key, plaintext) in enumerate(
        results[:TOP_RESULTS], 1
    ):

        print(
            f"{rank:2d}. "
            f"Key = 0x{key.hex()} "
            f"Score = {score}"
        )

        preview = plaintext[:100]

        try:
            print(
                "    ",
                preview.decode("utf-8", errors="replace")
            )
        except Exception:
            pass

        path = save_candidate(
            key,
            plaintext,
            score,
            2
        )

        print("    Saved:", path)
        print()

    return results


# ---------------------------------------------------------
# Main
# ---------------------------------------------------------

def main():

    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: {INPUT_FILE} not found.")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    with open(INPUT_FILE, "rb") as f:
        data = f.read()

    print("===================================")
    print(" XOR BRUTE-FORCE")
    print("===================================")

    print(f"Input file : {INPUT_FILE}")
    print(f"File size  : {len(data)} bytes")
    print(f"Output dir : {OUTPUT_DIR}")

    # One-byte XOR
    brute_force_one_byte(data)

    # Two-byte XOR
    brute_force_two_byte(data)

    print("\n===================================")
    print("Finished.")
    print("===================================")


if __name__ == "__main__":
    main()

