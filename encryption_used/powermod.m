function result = powermod(base, exponent, modulus)
    if nargin < 3 || isempty(modulus)
        error('powermod requires base, exponent, and modulus');
    end

    base = mod(base, modulus);
    result = 1;
    exp = exponent;

    while exp > 0
        if mod(exp, 2) == 1
            result = mod(result * base, modulus);
        end

        base = mod(base * base, modulus);
        exp = floor(exp / 2);
    end
end