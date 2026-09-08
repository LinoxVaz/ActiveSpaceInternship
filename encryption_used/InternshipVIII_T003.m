clc;
clear;
close all;

fprintf('===============================\n');
fprintf('Hybrid RSA + AES Demonstration\n');
fprintf('===============================\n\n');

%% PARAMETERS
input_file = 'InternshipVIII_T003.txt';
encrypted_file = 'InternshipVIII_T003.bin';
decrypted_file = 'InternshipVIII_T003.dat';

%% READ FILE (AS BYTES)

fid = fopen(input_file,'r');
plaintext = fread(fid,'*uint8');
fclose(fid);

fprintf('File size: %.2f KB\n\n', length(plaintext)/1024);

%% IMPORT JAVA CRYPTO
import java.security.*
import javax.crypto.*
import javax.crypto.spec.*

%% RSA KEY GENERATION
fprintf('Generating RSA-1024 keys...\n');

tic;
keyGen = KeyPairGenerator.getInstance('RSA');
keyGen.initialize(1024);

pair = keyGen.generateKeyPair();

publicKey = pair.getPublic();
privateKey = pair.getPrivate();

t_keygen = toc;

fprintf('RSA key generation: %.4f s\n\n', t_keygen);

%% AES KEY GENERATION
fprintf('Generating AES key...\n');

keyGenAES = KeyGenerator.getInstance('AES');
keyGenAES.init(192);

aesKey = keyGenAES.generateKey();

fprintf('AES key generated.\n\n');

%% AES ENCRYPTION
fprintf('Encrypting file using AES...\n');

tic;

aesCipher = Cipher.getInstance('AES');
aesCipher.init(Cipher.ENCRYPT_MODE, aesKey);

encryptedData = aesCipher.doFinal(int8(plaintext));

t_aes_enc = toc;

fprintf('AES encryption: %.6f s\n\n', t_aes_enc);

%% SAVE ENCRYPTED FILE
fid = fopen(encrypted_file,'wb');
fwrite(fid, typecast(encryptedData,'uint8'));
fclose(fid);

fprintf('Encrypted file saved.\n\n');

%% RSA ENCRYPT AES KEY
fprintf('Encrypting AES key using RSA...\n');

tic;

rsaCipher = Cipher.getInstance('RSA');
rsaCipher.init(Cipher.ENCRYPT_MODE, publicKey);

encryptedAESKey = rsaCipher.doFinal(aesKey.getEncoded());

t_rsa_enc = toc;

fprintf('RSA encryption: %.6f s\n\n', t_rsa_enc);

%% RSA DECRYPT AES KEY
fprintf('Decrypting AES key using RSA...\n');

tic;

rsaCipher.init(Cipher.DECRYPT_MODE, privateKey);

decryptedAESKey = rsaCipher.doFinal(encryptedAESKey);

t_rsa_dec = toc;

fprintf('RSA decryption: %.6f s\n\n', t_rsa_dec);

%% REBUILD AES KEY
aesKeyRecovered = SecretKeySpec(decryptedAESKey, 'AES');

%% AES DECRYPTION
fprintf('Decrypting file using AES...\n');

tic;

aesCipher.init(Cipher.DECRYPT_MODE, aesKeyRecovered);

decryptedData = aesCipher.doFinal(encryptedData);

t_aes_dec = toc;

fprintf('AES decryption: %.6f s\n\n', t_aes_dec);

%% SAVE DECRYPTED FILE
fid = fopen(decrypted_file,'w');
fwrite(fid, char(decryptedData), 'char');
fclose(fid);

fprintf('Decrypted file saved.\n\n');

%% VERIFY INTEGRITY
decrypted_uint8 = typecast(decryptedData,'uint8');

if isequal(plaintext, decrypted_uint8)
    fprintf('SUCCESS: File recovered correctly.\n');
else
    fprintf('ERROR: Files differ.\n');
end

%% SUMMARY
fprintf('\n===============================\n');
fprintf('PERFORMANCE SUMMARY\n');
fprintf('===============================\n');

fprintf('RSA KeyGen : %.4f s\n', t_keygen);
fprintf('AES Encrypt: %.6f s\n', t_aes_enc);
fprintf('RSA Encrypt: %.6f s\n', t_rsa_enc);
fprintf('RSA Decrypt: %.6f s\n', t_rsa_dec);
fprintf('AES Decrypt: %.6f s\n', t_aes_dec);

original_info = dir(input_file);
decrypted_info = dir(decrypted_file);

fprintf('Original size : %d bytes\n', original_info.bytes);
fprintf('Decrypted size: %d bytes\n', decrypted_info.bytes);

%% DISPLAY CONTENT
fprintf('\n--- ORIGINAL TEXT ---\n');
disp(char(plaintext'));

fprintf('\n--- DECRYPTED TEXT ---\n');
disp(char(decrypted_uint8'));