function c = rsa_encrypt(m, publicKey)
    c = powermod(double(m), publicKey.e, publicKey.n);
end