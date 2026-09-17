
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding


# Path to the encrypted .bin file
file_path = "ENCRYPTED_FILE_PATH"


# Open the file in binary read mode
with open(file_path, "rb") as f:
    ciphertext = f.read()


# --- Encrypted data and 16-byte IV ---
# The IV is not used because AES-ECB does not require an IV
iv = bytes([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])


# Extract the ciphertext from the file
# The first 16 bytes are skipped
ciphertext_block = bytes(ciphertext[16:48])


# Counter for the number of tested keys
count = 0


# Try all 256 possible values for the key byte
for i in range(256):

        # Create a 16-byte AES key where every byte has the value i
        key_16bytes = bytes([
            i, i, i, i,
            i, i, i, i,
            i, i, i, i,
            i, i, i, i
        ])

    # Uncomment to use a different key pattern,
    # such as alternating two different bytes.
    #
    # for j in range(256):
    #     key_16bytes = bytes([
    #         i, j, i, j,
    #         i, j, i, j,
    #         i, j, i, j,
    #         i, j, i, j
    #     ])


        # Increment the number of tested keys
        count += 1

        try:

            # Configure the AES decryptor using the current key
            # AES-ECB does not require an IV
            cipher = Cipher(algorithms.AES(key_16bytes),modes.ECB())

            # Uncomment the following line to use AES-CBC mode with the provided IV
            # cipher = Cipher(algorithms.AES(key_16bytes),modes.CBC(iv)) 
            decryptor = cipher.decryptor()


            # Decrypt the ciphertext
            decrypted_text = (decryptor.update(ciphertext_block) + decryptor.finalize())


            # Remove PKCS7 padding
            # 128 bits = 16 bytes, which is the AES block size
            unpadder = padding.PKCS7(128).unpadder()

            clean_text = (unpadder.update(decrypted_text) + unpadder.finalize())


            # Check whether all decrypted bytes are in the valid byte range (printable ASCII characters)
            if all(32 <= b <= 126 or b in (9, 10, 13) for b in clean_text):

                print(f"\n{count}")

                # Print the complete 16-byte AES key
                print(
                    f"[+] Complete key: {key_16bytes}"
                )

                # Print the decrypted plaintext
                print(f"[+] Decrypted text: "f"{clean_text.decode('utf-8', errors='ignore')}")

                # Stop once a valid candidate is found
                break


        except Exception:

            # Ignore padding or decryption errors
            # and continue testing the next key
            continue

