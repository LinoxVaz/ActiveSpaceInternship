function encrypt_file_1000_times_T1T2(inputFile, outputFile)
    if nargin < 1 || isempty(inputFile)
        inputFile = 'frase.txt';
    end
    if nargin < 2 || isempty(outputFile)
        outputFile = 'encrypted_1000_times.txt';
    end

    scriptDir = fileparts(mfilename('fullpath'));
    if ~isempty(scriptDir)
        addpath(scriptDir);
    end

    if ~exist(inputFile, 'file')
        candidate = fullfile(scriptDir, inputFile);
        if exist(candidate, 'file')
            inputFile = candidate;
        end
    end

    fid = fopen(inputFile, 'r');
    if fid == -1
        error('Could not open input file: %s', inputFile);
    end
    plaintext = fread(fid, '*uint8');
    fclose(fid);

    import java.security.*
    import javax.crypto.*
    import javax.crypto.spec.*

    iterations = 1000;

    if ~exist(outputFile, 'file') && ~isempty(scriptDir)
        outputFile = fullfile(scriptDir, outputFile);
    end

    fidOut = fopen(outputFile, 'wt');
    if fidOut == -1
        error('Could not open output file: %s', outputFile);
    end

    fprintf(fidOut, 'INPUT_FILE=%s\n', inputFile);
    fprintf(fidOut, 'ITERATIONS=%d\n', iterations);
    fprintf(fidOut, 'METHOD=Hybrid_RSA_AES_T1_T2\n');
    fprintf(fidOut, 'RESULTS_BEGIN\n');

    for i = 1:iterations
        % RSA key generation: same as T1/T2 style, RSA-512
        keyGen = KeyPairGenerator.getInstance('RSA');
        keyGen.initialize(512);
        pair = keyGen.generateKeyPair();
        publicKey = pair.getPublic();
        privateKey = pair.getPrivate();

        % AES key generation: same as T1/T2 style, AES-128
        keyGenAES = KeyGenerator.getInstance('AES');
        keyGenAES.init(128);
        aesKey = keyGenAES.generateKey();

        % AES encryption of the plaintext bytes
        aesCipher = Cipher.getInstance('AES');
        aesCipher.init(Cipher.ENCRYPT_MODE, aesKey);
        encryptedData = aesCipher.doFinal(int8(plaintext));

        % RSA encryption of the AES key
        rsaCipher = Cipher.getInstance('RSA');
        rsaCipher.init(Cipher.ENCRYPT_MODE, publicKey);
        encryptedAESKey = rsaCipher.doFinal(aesKey.getEncoded());

        encryptedDataBytes = javaByteArrayToUint8(encryptedData);
        encryptedKeyBytes = javaByteArrayToUint8(encryptedAESKey);

        fprintf(fidOut, 'RUN_%d\n', i);
        fprintf(fidOut, 'AES_KEY_HEX=%s\n', bytesToHex(encryptedKeyBytes));
        fprintf(fidOut, 'CIPHERTEXT_HEX=%s\n', bytesToHex(encryptedDataBytes));
    end

    fprintf(fidOut, 'RESULTS_END\n');
    fclose(fidOut);

    fprintf('Input file : %s\n', inputFile);
    fprintf('Output file: %s\n', outputFile);
    fprintf('Completed %d encryptions.\n', iterations);
end

function bytes = javaByteArrayToUint8(javaBytes)
    if isempty(javaBytes)
        bytes = uint8([]);
        return;
    end

    bytes = zeros(1, numel(javaBytes), 'uint8');
    for k = 1:numel(javaBytes)
        bytes(k) = uint8(javaBytes(k));
    end
end

function hexStr = bytesToHex(bytes)
    if isempty(bytes)
        hexStr = '';
        return;
    end
    hexStr = sprintf('%02x', bytes(:));
end