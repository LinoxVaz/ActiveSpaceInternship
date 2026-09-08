function x = modInverse(a,m)

[g,x,~] = gcd(a,m);

if g ~= 1
    error('No modular inverse');
end

x = mod(x,m);

end