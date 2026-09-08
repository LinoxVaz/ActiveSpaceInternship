function m = rsa_decrypt(c, privateKey)
    m = powermod(double(c), privateKey.d, privateKey.n);
end