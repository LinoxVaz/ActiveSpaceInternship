#include <Wire.h>

#define DEVICE_ADDR 0x60 // Default I2C address for the ATECC608B chip

#define WORD_ADDR_IDLE     0x02 // Word address used to send the chip into Idle mode
#define WORD_ADDR_COMMAND  0x03 // Word address used to send commands to the chip

#define OPCODE_READ   0x02 // Opcode for the Read command
#define OPCODE_WRITE  0x12 // Opcode for the Write command
#define OPCODE_LOCK   0x17 // Opcode for the Lock command
#define OPCODE_AES    0x51 // Opcode for the AES command

#define AES_SLOT      8 // Slot 8 is used for storing the AES key. It must be configured as a 32-byte key slot in the ATECC608B configuration.

uint8_t configZone[128]; // Buffer to hold the full contents of the configuration zone (32 words x 4 bytes)

const uint8_t aesKey[16] = { // AES key that will be provisioned into Slot 8
  0x9E, 0xE7, 0x4F, 0x11,
  0x30, 0x6B, 0x86, 0xFE,
  0x5F, 0x4B, 0x8E, 0x91,
  0xF0, 0xEA, 0x01, 0x98
};

const uint8_t testPlaintext[16] = { // Sample plaintext used to validate the AES encrypt/decrypt round trip
  'A','T','E','C','C','6','0','8',
  'B','-','A','E','S','-','0','1'
};

void calculateCRC(uint8_t length, const uint8_t *data, uint8_t *crcOut) { // Calculate the ATECC608B CRC-16 checksum for a command or response

    uint16_t crcRegister = 0;
    const uint16_t polynom = 0x8005;

    for (uint8_t counter = 0; counter < length; counter++) {

        for (uint8_t shiftReg = 0x01; shiftReg != 0; shiftReg <<= 1) {

            uint8_t dataBit = (data[counter] & shiftReg) ? 1 : 0;
            uint8_t crcBit = (uint8_t)(crcRegister >> 15);

            crcRegister <<= 1;

            if (dataBit != crcBit) crcRegister ^= polynom;

        }

    }

    crcOut[0] = (uint8_t)(crcRegister & 0xFF); // Low byte of the CRC
    crcOut[1] = (uint8_t)(crcRegister >> 8); // High byte of the CRC

}

void printHex(uint8_t b) { // Print a single byte as a two-digit hexadecimal value

    if (b < 0x10) Serial.print('0');

    Serial.print(b, HEX);

}

void printBuffer(const uint8_t *data, uint8_t len) { // Print a buffer of bytes as space-separated hexadecimal values

    for (uint8_t i = 0; i < len; i++) {

        printHex(data[i]);

        if (i != len - 1) Serial.print(' ');

    }

    Serial.println();

}

bool checkCRC(uint8_t *response, uint8_t length) { // Verify that the CRC appended to a response matches the calculated value

    uint8_t crc[2];

    calculateCRC(length - 2, response, crc);

    return (response[length - 2] == crc[0] && response[length - 1] == crc[1]);

}

bool wakeChip() { // Wake up the ATECC608B chip by pulling SDA low for the required wake period

    Wire.beginTransmission(0x00);
    Wire.write(0x00);
    Wire.endTransmission();

    delayMicroseconds(1500);

    Wire.requestFrom(DEVICE_ADDR, (uint8_t)4);

    if (Wire.available() < 4) return false;

    uint8_t response[4];

    for (uint8_t i = 0; i < 4; i++) response[i] = Wire.read();

    Serial.print(F("Wake: "));
    printBuffer(response, 4);

    return (response[0] == 0x04 && response[1] == 0x11);

}

bool readConfigWord(uint8_t wordAddr, uint8_t *data4) { // Read a single 4-byte word from the configuration zone

    uint8_t packet[5];

    packet[0] = 0x07;
    packet[1] = OPCODE_READ;
    packet[2] = 0x00;
    packet[3] = wordAddr;
    packet[4] = 0x00;

    uint8_t crc[2];

    calculateCRC(sizeof(packet), packet, crc);

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    if (Wire.endTransmission() != 0) return false;

    delay(5);

    Wire.requestFrom(DEVICE_ADDR, (uint8_t)7);

    if (Wire.available() < 7) return false;

    uint8_t response[7];

    for (uint8_t i = 0; i < 7; i++) response[i] = Wire.read();

    if (response[0] != 0x07) return false;
    if (!checkCRC(response, 7)) return false;

    for (uint8_t i = 0; i < 4; i++) data4[i] = response[i + 1];

    return true;

}

bool readConfig() { // Read the entire 128-byte configuration zone, one word at a time

    for (uint8_t word = 0; word < 32; word++) {

        if (!readConfigWord(word, &configZone[word * 4])) return false;

    }

    return true;

}

bool verifyExpectedConfig() { // Check that the chip's configuration zone matches the values expected before locking anything

    uint16_t slotCfg = (uint16_t)configZone[36] | ((uint16_t)configZone[37] << 8); // SlotConfig for Slot 8
    uint16_t keyCfg = (uint16_t)configZone[112] | ((uint16_t)configZone[113] << 8); // KeyConfig for Slot 8
    uint16_t slotLocked = (uint16_t)configZone[88] | ((uint16_t)configZone[89] << 8); // SlotLocked bitmap

    Serial.println(F("Current configuration:"));

    Serial.print(F("LockConfig  = 0x"));
    printHex(configZone[87]);
    Serial.println();

    Serial.print(F("LockValue   = 0x"));
    printHex(configZone[86]);
    Serial.println();

    Serial.print(F("SlotCfg8    = 0x"));
    Serial.println(slotCfg, HEX);

    Serial.print(F("KeyCfg8     = 0x"));
    Serial.println(keyCfg, HEX);

    Serial.print(F("SlotLocked  = 0x"));
    Serial.println(slotLocked, HEX);

    return (

        configZone[87] == 0x55 &&
        configZone[86] == 0x55 &&
        slotCfg == 0x0F8F &&
        keyCfg == 0x0038 &&
        slotLocked == 0xFFFF

    );

}

bool readStatusResponse() { // Read and validate the chip's status response after sending a command

    Wire.requestFrom(DEVICE_ADDR, (uint8_t)4);

    if (Wire.available() < 4) return false;

    uint8_t response[4];

    for (uint8_t i = 0; i < 4; i++) response[i] = Wire.read();

    Serial.print(F("Response: "));
    printBuffer(response, 4);

    if (!checkCRC(response, 4)) return false;

    return (response[0] == 0x04 && response[1] == 0x00);

}

bool lockConfigZone() { // Irreversibly lock the configuration zone so it can no longer be modified

    uint8_t packet[5];

    packet[0] = 0x07;
    packet[1] = OPCODE_LOCK;
    packet[2] = 0x80; // Config zone + ignore summary CRC
    packet[3] = 0x00;
    packet[4] = 0x00;

    uint8_t crc[2];

    calculateCRC(sizeof(packet), packet, crc);

    Serial.println(F("LOCKING CONFIG ZONE..."));

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    if (Wire.endTransmission() != 0) return false;

    delay(40);

    return readStatusResponse();

}

bool lockDataZone() { // Irreversibly lock the data zone so slot contents can no longer be modified

    uint8_t packet[5];

    packet[0] = 0x07;
    packet[1] = OPCODE_LOCK;
    packet[2] = 0x81; // Data / OTP + ignore summary CRC
    packet[3] = 0x00;
    packet[4] = 0x00;

    uint8_t crc[2];

    calculateCRC(sizeof(packet), packet, crc);

    Serial.println(F("LOCKING DATA ZONE..."));

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    uint8_t wireResult = Wire.endTransmission();

    Serial.print(F("Wire.endTransmission() = "));
    Serial.println(wireResult);

    if (wireResult != 0) {

        Serial.println(F("I2C transmission FAILED"));
        return false;

    }

    delay(40);

    return readStatusResponse();

}

bool writeAESKey() { // Write the AES key into Slot 8 using a 32-byte Data-zone write. Requires Wire BUFFER_LENGTH >= 41.

    uint8_t packet[37];

    packet[0] = 0x27;
    packet[1] = OPCODE_WRITE;
    packet[2] = 0x82; // Data zone + 32-byte write
    packet[3] = 0x40; // Slot 8, block 0
    packet[4] = 0x00;

    for (uint8_t i = 0; i < 16; i++) packet[5 + i] = aesKey[i]; // First 16 bytes hold the AES key
    for (uint8_t i = 16; i < 32; i++) packet[5 + i] = 0x00; // Remaining bytes of the slot block are padded with zeros

    uint8_t crc[2];

    calculateCRC(sizeof(packet), packet, crc);

    Serial.println(F("Writing AES key into Slot 8..."));

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_COMMAND);

    size_t n1 = Wire.write(packet, sizeof(packet));
    size_t n2 = Wire.write(crc, 2);

    Serial.print(F("Wire queued command bytes: "));
    Serial.println(n1);

    Serial.print(F("Wire queued CRC bytes: "));
    Serial.println(n2);

    if (n1 != sizeof(packet) || n2 != 2) {

        Serial.println(F("ERROR: Wire buffer too small!"));
        return false;

    }

    if (Wire.endTransmission() != 0) return false;

    delay(40);

    return readStatusResponse();

}

bool aesBlock(bool decrypt, const uint8_t input[16], uint8_t output[16]) { // Run a single AES-128 block through the chip, either encrypting or decrypting depending on the flag

    uint8_t packet[21];

    packet[0] = 0x17;
    packet[1] = OPCODE_AES;
    packet[2] = decrypt ? 0x01 : 0x00; // Mode: 0x00 = encrypt, 0x01 = decrypt
    packet[3] = AES_SLOT; // KeyID = Slot 8
    packet[4] = 0x00;

    for (uint8_t i = 0; i < 16; i++) packet[5 + i] = input[i];

    uint8_t crc[2];

    calculateCRC(sizeof(packet), packet, crc);

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    if (Wire.endTransmission() != 0) return false;

    delay(15);

    Wire.requestFrom(DEVICE_ADDR, (uint8_t)19); // Response = 1 count byte + 16 data bytes + 2 CRC bytes

    if (Wire.available() < 19) return false;

    uint8_t response[19];

    for (uint8_t i = 0; i < 19; i++) response[i] = Wire.read();

    if (response[0] != 19) {

        if (response[0] == 4) {

            Serial.print(F("AES error status = 0x"));
            printHex(response[1]);
            Serial.println();

        }

        return false;

    }

    if (!checkCRC(response, 19)) return false;

    for (uint8_t i = 0; i < 16; i++) output[i] = response[i + 1];

    return true;

}

void idleChip() { // Send the chip into Idle mode to end the session

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_IDLE);
    Wire.endTransmission();

}

void setup() {

    Serial.begin(9600);
    Wire.begin();
    Wire.setClock(100000);

    delay(1500);

    Serial.println();
    Serial.println(F("=============================="));
    Serial.println(F("ATECC608B AES provisioning"));
    Serial.println(F("=============================="));

    if (!wakeChip()) {

        Serial.println(F("STOP: wake failed"));
        return;

    }

    if (!readConfig()) {

        Serial.println(F("STOP: config read failed"));
        return;

    }

    if (!verifyExpectedConfig()) {

        Serial.println(F("STOP: configuration not as expected"));
        Serial.println(F("Nothing was locked."));
        return;

    }

    Serial.println(F("Configuration verified."));

    // Irreversible step 1: lock the configuration zone

    if (!lockConfigZone()) {

        Serial.println(F("STOP: Config lock failed"));
        return;

    }

    if (!readConfig()) {

        Serial.println(F("STOP: cannot verify Config lock"));
        return;

    }

    if (configZone[87] != 0x00) {

        Serial.println(F("STOP: LockConfig did not become 00"));
        return;

    }

    if (configZone[86] != 0x55) {

        Serial.println(F("STOP: Data unexpectedly locked"));
        return;

    }

    Serial.println(F(">>> CONFIG ZONE LOCKED successfully <<<"));
    Serial.println(F("Data zone still unlocked."));

    if (!writeAESKey()) {

        Serial.println(F("STOP: AES key write failed"));
        Serial.println(F("Data zone remains unlocked."));
        return;

    }

    Serial.println(F("AES key write accepted."));

    // Irreversible step 2: lock the data zone

    if (!lockDataZone()) {

        Serial.println(F("STOP: Data lock failed"));
        return;

    }

    if (!readConfig()) {

        Serial.println(F("STOP: cannot verify Data lock"));
        return;

    }

    if (configZone[86] != 0x00) {

        Serial.println(F("STOP: LockValue did not become 00"));
        return;

    }

    Serial.println(F(">>> DATA ZONE LOCKED successfully <<<"));

    uint8_t ciphertext[16];
    uint8_t recovered[16];

    Serial.println();
    Serial.print(F("Plaintext : "));
    printBuffer(testPlaintext, 16);

    if (!aesBlock(false, testPlaintext, ciphertext)) {

        Serial.println(F("AES encrypt FAILED"));
        return;

    }

    Serial.print(F("Ciphertext: "));
    printBuffer(ciphertext, 16);

    if (!aesBlock(true, ciphertext, recovered)) {

        Serial.println(F("AES decrypt FAILED"));
        return;

    }

    Serial.print(F("Recovered : "));
    printBuffer(recovered, 16);

    bool match = true;

    for (uint8_t i = 0; i < 16; i++) {

        if (recovered[i] != testPlaintext[i]) {

            match = false;
            break;

        }

    }

    Serial.println();

    if (match) {

        Serial.println(F("================================"));
        Serial.println(F(">>> AES TEST SUCCESS <<<"));
        Serial.println(F("Encrypt + decrypt verified."));
        Serial.println(F("================================"));

    } 
    
    else {

        Serial.println(F("================================"));
        Serial.println(F("AES TEST FAILED"));
        Serial.println(F("Recovered data does not match."));
        Serial.println(F("================================"));

    }

    idleChip();

}

void loop() {
}
