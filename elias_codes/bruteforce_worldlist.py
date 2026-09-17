from Crypto.Cipher import AES

# Load your known data
with open('PLAIN_TEXT_PATH', 'rb') as f:
    plaintext = f.read()

with open('CIPHER_TEXT_PATH', 'rb') as f:
    ciphertext = f.read()

with open('passwords1.txt') as f:
    passlist = f.read().splitlines()

# Grab the first 16 bytes (one AES block) to test quickly
target_block = ciphertext[:32]
known_plain_block = plaintext[:32]

# Define a wordlist or a logical range of keys to test
passlist = [key for key in passlist if len(key) <= 16]

iv = b'\x00' * 16

# print(passlist)
for password in passlist:
    # Keys must be 16, 24, or 32 bytes. Pad your guess with zeros or encode it:
    key = password.encode('utf-8').ljust(16, b'\x00') 

    # Uncomment to encrypt with AES_ECB mode
    # cipher = AES.new(key, AES.MODE_ECB)

    # Uncomment to encrypt with AES_CBC mode
    cipher = AES.new(key, AES.MODE_CBC, iv)
    
    test_ciphertext = cipher.encrypt(known_plain_block)
    
    # print(test_ciphertext.decode('latin-1', errors='ignore'))
    # If match = key found
    if test_ciphertext == target_block:
        print(f"Success! The key is: {password}")
        break
