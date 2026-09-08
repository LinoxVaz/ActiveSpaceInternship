from pathlib import Path
from collections import Counter
import math

BLOCK = 16

# ------------------------------------------------------------
# CONFIGURATION
# ------------------------------------------------------------

PLAINTEXT_FILE = "Amor é um fogo que arde.txt"
CIPHERTEXT_FILE = "InternshipVIII_T004.bin"

# ------------------------------------------------------------

pt = Path(PLAINTEXT_FILE).read_bytes()
ct = Path(CIPHERTEXT_FILE).read_bytes()

print("="*80)
print("AES FORENSICS TOOLKIT")
print("="*80)

print(f"Plaintext length : {len(pt)}")
print(f"Ciphertext length: {len(ct)}")

# ------------------------------------------------------------

def entropy(data):

    if len(data) == 0:
        return 0

    freq = Counter(data)

    e = 0

    for c in freq.values():
        p = c/len(data)
        e -= p*math.log2(p)

    return e

print(f"\nPlaintext entropy : {entropy(pt):.3f}")
print(f"Ciphertext entropy: {entropy(ct):.3f}")

# ------------------------------------------------------------

print("\n" + "="*80)
print("FILE LENGTH ANALYSIS")
print("="*80)

delta = len(ct)-len(pt)

print(f"Ciphertext - Plaintext = {delta} bytes")

if delta == 0:
    print("✓ Same size")
    print("  Compatible with:")
    print("    - AES CTR")
    print("    - AES OFB")
    print("    - AES CFB")
    print("    - AES XTS (unlikely)")
    print("  Less likely:")
    print("    - CBC with PKCS7")
    print("    - ECB with PKCS7")

elif delta == 16:
    print("Ciphertext is one block larger.")
    print("Could indicate:")
    print("   - GCM authentication tag")
    print("   - CBC padding")

else:
    print("Unusual size difference.")

# ------------------------------------------------------------

print("\n" + "="*80)
print("ECB TEST")
print("="*80)

blocks = [ct[i:i+BLOCK] for i in range(0,len(ct),BLOCK)]

dup = False

counter = Counter(blocks)

for b,n in counter.items():

    if n>1:

        dup=True

        print(f"Repeated block ({n}): {b.hex()}")

if not dup:

    print("✓ No repeated ciphertext blocks.")
    print("ECB is unlikely.")

# ------------------------------------------------------------

print("\n" + "="*80)
print("POSSIBLE FILE LAYOUTS")
print("="*80)

layouts = [

    ("Raw ciphertext",0,0),

    ("16-byte IV + ciphertext",16,0),

    ("ciphertext + 16-byte tag",0,16),

    ("16-byte IV + ciphertext + 16-byte tag",16,16),

    ("12-byte nonce + ciphertext + 16-byte tag",12,16)

]

for name,head,tail in layouts:

    usable = len(ct)-head-tail

    print()

    print(name)

    print(f" Header : {head}")

    print(f" Trailer: {tail}")

    print(f" Payload: {usable}")

    if usable == len(pt):

        print("  ✓ Payload size matches plaintext exactly.")

    else:

        print(f"  Difference = {usable-len(pt)} bytes")

# ------------------------------------------------------------

print("\n" + "="*80)
print("FIRST BLOCK RANDOMNESS")
print("="*80)

print(blocks[0].hex())

print()

print("Entropy first block:",entropy(blocks[0]))

print()

print("If this block is an IV/nonce it should appear random.")

# ------------------------------------------------------------

print("\n" + "="*80)
print("LAST BLOCK RANDOMNESS")
print("="*80)

print(blocks[-1].hex())

print()

print("Entropy last block:",entropy(blocks[-1]))

print()

print("Authentication tags also look random.")

# ------------------------------------------------------------

print("\n" + "="*80)
print("KNOWN PLAINTEXT TEST")
print("="*80)

xor = bytes(a^b for a,b in zip(pt,ct))

xor_blocks=[xor[i:i+BLOCK] for i in range(0,len(xor),BLOCK)]

dup=False

counter=Counter(xor_blocks)

for b,n in counter.items():

    if n>1:

        dup=True

        print("Repeated XOR block:",b.hex())

if not dup:

    print("✓ Every XOR block is unique.")

print()

print("If this is CTR/OFB/CFB, XOR represents the keystream.")

print()

print("Save it for comparison with other ciphertexts.")

# ------------------------------------------------------------

print("\n" + "="*80)
print("MODE SCORE")
print("="*80)

scores={

"ECB":0,

"CBC":0,

"CTR":0,

"CFB":0,

"OFB":0,

"GCM":0

}

if delta==0:

    scores["CTR"]+=2

    scores["CFB"]+=2

    scores["OFB"]+=2

if delta==16:

    scores["GCM"]+=2

    scores["CBC"]+=1

if not dup:

    scores["CTR"]+=1

    scores["CBC"]+=1

    scores["CFB"]+=1

    scores["OFB"]+=1

    scores["GCM"]+=1

scores["ECB"]-=5

for mode,score in sorted(scores.items(),key=lambda x:x[1],reverse=True):

    print(f"{mode:5} : {score}")

print()

print("NOTE: This is NOT a detector.")
print("It only reports consistency with the observed evidence.")