function [publicKey, privateKey] = rsa_keygen(bits)
    if nargin < 1 || isempty(bits)
        bits = 16;
    end

    effectiveBits = max(8, min(bits, 30));

    e = 65537;

    p = random_prime(floor(effectiveBits / 2));
    q = random_prime(ceil(effectiveBits / 2));

    n = p * q;
    phi = (p - 1) * (q - 1);

    d = modInverse(e, phi);

    publicKey.n = n;
    publicKey.e = e;

    privateKey.n = n;
    privateKey.d = d;
end