function p = random_prime(bits)
    if nargin < 1 || isempty(bits)
        bits = 16;
    end

    bits = max(2, min(bits, 30));

    low = 2^(bits - 1);
    high = 2^bits - 1;

    while true
        p = randi([low, high]);

        if mod(p, 2) == 0
            p = p + 1;
        end

        if p > high
            p = low + 1;
        end

        if isprime(p)
            return;
        end
    end
end