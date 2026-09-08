#include <Wire.h>

#define ATECC_ADDR         0x60
#define ATECC_WORD_CMD     0x03
#define ATECC_WORD_SLEEP   0x01

#define OPCODE_NONCE       0x16
#define OPCODE_AES         0x51

#define NONCE_MODE_FIXED_TEMPKEY  0x03

#define AES_MODE_ENCRYPT   0x00
#define AES_MODE_DECRYPT   0x01

#define AES_TEMPKEY_ID     0xFFFF

#define ATECC_MAX_RESPONSE 96
#define WIRE_BUFFER_SIZE   32

//-----------------------------------------------------
// CryptoAuthentication CRC-16
// Polynomial 0x8005
//-----------------------------------------------------
void atcaCrc(
    const uint8_t *data,
    size_t length,
    uint8_t crc[2])
{
    uint16_t crcRegister = 0;
    const uint16_t polynomial = 0x8005;

    for (size_t counter = 0; counter < length; counter++)
    {
        for (uint8_t mask = 0x01; mask != 0; mask <<= 1)
        {
            uint8_t dataBit =
                (data[counter] & mask) ? 1 : 0;

            uint8_t crcBit =
                (uint8_t)((crcRegister >> 15) & 0x01);

            crcRegister <<= 1;

            if (dataBit != crcBit)
            {
                crcRegister ^= polynomial;
            }
        }
    }

    crc[0] = (uint8_t)(crcRegister & 0xFF);
    crc[1] = (uint8_t)(crcRegister >> 8);
}

//-----------------------------------------------------

void printHex(uint8_t value)
{
    if (value < 0x10)
    {
        Serial.print('0');
    }

    Serial.print(value, HEX);
}

void printBuffer(
    const uint8_t *buffer,
    size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        printHex(buffer[i]);
        Serial.print(' ');
    }

    Serial.println();
}

//-----------------------------------------------------
// Wake chip
//-----------------------------------------------------
bool wakeChip()
{
    Wire.end();

    pinMode(SDA, OUTPUT);
    digitalWrite(SDA, LOW);
    delayMicroseconds(80);

    pinMode(SDA, INPUT_PULLUP);

    delayMicroseconds(2500);

    Wire.begin();
    Wire.setClock(100000);

    uint8_t wakeResponse[4];

    uint8_t received = Wire.requestFrom(
        (uint8_t)ATECC_ADDR,
        (uint8_t)sizeof(wakeResponse)
    );

    if (received != sizeof(wakeResponse))
    {
        while (Wire.available())
        {
            Wire.read();
        }

        Serial.println(F("Wake response not received."));
        return false;
    }

    for (uint8_t i = 0; i < sizeof(wakeResponse); i++)
    {
        wakeResponse[i] = Wire.read();
    }

    Serial.print(F("Wake response: "));
    printBuffer(wakeResponse, sizeof(wakeResponse));

    if (wakeResponse[0] != 0x04)
    {
        Serial.println(F("Invalid wake response length."));
        return false;
    }

    return true;
}

//-----------------------------------------------------

void sleepChip()
{
    Wire.beginTransmission(ATECC_ADDR);

    Wire.write(ATECC_WORD_SLEEP);

    Wire.endTransmission();
}

//-----------------------------------------------------
// Status decoder
//-----------------------------------------------------
void printStatusCode(uint8_t status)
{
    Serial.print(F("Device status: 0x"));
    printHex(status);
    Serial.print(F(" - "));

    switch (status)
    {
        case 0x00:
            Serial.println(F("Success"));
            break;

        case 0x01:
            Serial.println(F("CheckMac/Verify miscompare"));
            break;

        case 0x03:
            Serial.println(F("Parse error"));
            break;

        case 0x05:
            Serial.println(F("ECC fault"));
            break;

        case 0x07:
            Serial.println(F("Self-test error"));
            break;

        case 0x08:
            Serial.println(F("Health-test error"));
            break;

        case 0x0F:
            Serial.println(F("Execution error"));
            break;

        case 0x11:
            Serial.println(F("Wake response"));
            break;

        case 0xEE:
            Serial.println(F("Watchdog warning"));
            break;

        case 0xFF:
            Serial.println(F("CRC / communication error"));
            break;

        default:
            Serial.println(F("Unknown status"));
            break;
    }
}

//-----------------------------------------------------
// Validate response CRC
//-----------------------------------------------------
bool validateResponseCrc(
    const uint8_t *response,
    size_t length)
{
    if (length < 4)
    {
        return false;
    }

    uint8_t calculated[2];

    atcaCrc(
        response,
        length - 2,
        calculated
    );

    return
        calculated[0] == response[length - 2] &&
        calculated[1] == response[length - 1];
}

//-----------------------------------------------------
// Read response
//-----------------------------------------------------
bool readResponse(
    uint8_t *response,
    size_t capacity,
    size_t &responseLength,
    uint16_t timeoutMs)
{
    responseLength = 0;

    uint8_t count = 0;
    bool gotCount = false;

    unsigned long startTime = millis();

    //-------------------------------------------------
    // Poll while device is busy
    //-------------------------------------------------

    while ((millis() - startTime) < timeoutMs)
    {
        uint8_t received =
            Wire.requestFrom(
                (uint8_t)ATECC_ADDR,
                (uint8_t)1
            );

        if (received == 1 &&
            Wire.available())
        {
            count = Wire.read();
            gotCount = true;
            break;
        }

        while (Wire.available())
        {
            Wire.read();
        }

        delay(1);
    }

    if (!gotCount)
    {
        Serial.println(F("Response timeout."));
        return false;
    }

    if (count < 4 ||
        count > capacity)
    {
        Serial.print(F("Invalid response count: "));
        Serial.println(count);
        return false;
    }

    response[0] = count;

    size_t index = 1;
    size_t remaining = count - 1;

    //-------------------------------------------------
    // Read remaining bytes in chunks
    //-------------------------------------------------

    while (remaining > 0)
    {
        uint8_t chunk =
            remaining > WIRE_BUFFER_SIZE
                ? WIRE_BUFFER_SIZE
                : (uint8_t)remaining;

        uint8_t received =
            Wire.requestFrom(
                (uint8_t)ATECC_ADDR,
                chunk
            );

        if (received != chunk)
        {
            Serial.println(F("Partial response."));
            return false;
        }

        while (Wire.available() &&
               index < capacity)
        {
            response[index++] = Wire.read();
        }

        remaining -= received;
    }

    responseLength = index;

    Serial.print(F("Response: "));
    printBuffer(response, responseLength);

    if (!validateResponseCrc(
            response,
            responseLength))
    {
        Serial.println(F("Response CRC FAILED."));
        return false;
    }

    Serial.println(F("Response CRC OK."));

    //-------------------------------------------------
    // Four-byte response = status response
    //-------------------------------------------------

    if (responseLength == 4)
    {
        printStatusCode(response[1]);

        return response[1] == 0x00;
    }

    return true;
}

//-----------------------------------------------------
// Send arbitrary command packet.
//
// packet DOES NOT contain the 0x03 word address.
// packet begins with Count.
//
// This handles packets > Arduino Wire's 32-byte buffer.
//-----------------------------------------------------
bool sendCommandPacket(
    const uint8_t *packet,
    size_t packetLength)
{
    size_t offset = 0;

    /*
     * Arduino Wire's TX buffer includes the word-address
     * byte, so each transaction can carry at most 31
     * command bytes in addition to 0x03.
     */

    while (offset < packetLength)
    {
        size_t remaining =
            packetLength - offset;

        size_t chunk =
            remaining > (WIRE_BUFFER_SIZE - 1)
                ? (WIRE_BUFFER_SIZE - 1)
                : remaining;

        Wire.beginTransmission(ATECC_ADDR);

        //-------------------------------------------------
        // CryptoAuthentication command word address
        //-------------------------------------------------

        Wire.write(ATECC_WORD_CMD);

        //-------------------------------------------------
        // Continue command FIFO
        //-------------------------------------------------

        Wire.write(
            &packet[offset],
            chunk
        );

        uint8_t result =
            Wire.endTransmission();

        if (result != 0)
        {
            Serial.print(F(
                "I2C command transmission failed: "
            ));

            Serial.println(result);

            return false;
        }

        offset += chunk;
    }

    return true;
}

//-----------------------------------------------------
// Load 32 bytes into TempKey
//
// Nonce command:
//
// Count  = 39 = 0x27
// Opcode = 0x16
// Mode   = 0x03
// Param2 = 0x0000
// Data   = 32 bytes
// CRC    = 2 bytes
//-----------------------------------------------------
bool loadTempKey(
    const uint8_t data[32])
{
    uint8_t packet[39];

    packet[0] = 0x27;
    packet[1] = OPCODE_NONCE;
    packet[2] = NONCE_MODE_FIXED_TEMPKEY;

    packet[3] = 0x00; // Param2 low
    packet[4] = 0x00; // Param2 high

    memcpy(
        &packet[5],
        data,
        32
    );

    //-------------------------------------------------
    // CRC over:
    // Count + Opcode + Mode + Param2 + Data
    //
    // 39 total bytes minus 2 CRC = 37
    //-------------------------------------------------

    atcaCrc(
        packet,
        37,
        &packet[37]
    );

    Serial.println();
    Serial.println(F("--------------------------------"));
    Serial.println(F("Loading TempKey"));
    Serial.print(F("Nonce packet: "));
    printBuffer(packet, sizeof(packet));

    if (!sendCommandPacket(
            packet,
            sizeof(packet)))
    {
        return false;
    }

    uint8_t response[4];
    size_t responseLength = 0;

    return readResponse(
        response,
        sizeof(response),
        responseLength,
        100
    );
}

//-----------------------------------------------------
// AES encrypt/decrypt using TempKey
//
// AES packet:
//
// Count  = 23 = 0x17
// Opcode = 0x51
// Mode   = encrypt/decrypt
// KeyID  = 0xFFFF = TempKey
// Data   = 16 bytes
// CRC    = 2 bytes
//-----------------------------------------------------
bool aesTempKey(
    bool decrypt,
    const uint8_t input[16],
    uint8_t output[16])
{
    uint8_t packet[23];

    packet[0] = 0x17;
    packet[1] = OPCODE_AES;

    packet[2] =
        decrypt
            ? AES_MODE_DECRYPT
            : AES_MODE_ENCRYPT;

    //-------------------------------------------------
    // KeyID 0xFFFF, little-endian
    //-------------------------------------------------

    packet[3] = 0xFF;
    packet[4] = 0xFF;

    //-------------------------------------------------
    // Input AES block
    //-------------------------------------------------

    memcpy(
        &packet[5],
        input,
        16
    );

    //-------------------------------------------------
    // CRC:
    // 23 total - 2 CRC = 21 bytes
    //-------------------------------------------------

    atcaCrc(
        packet,
        21,
        &packet[21]
    );

    Serial.println();
    Serial.println(F("--------------------------------"));

    if (decrypt)
    {
        Serial.println(F("AES DECRYPT using TempKey"));
    }
    else
    {
        Serial.println(F("AES ENCRYPT using TempKey"));
    }

    Serial.print(F("AES packet: "));
    printBuffer(packet, sizeof(packet));

    if (!sendCommandPacket(
            packet,
            sizeof(packet)))
    {
        return false;
    }

    //-------------------------------------------------
    // Successful AES response:
    //
    // Count = 19 / 0x13
    // Data  = 16 bytes
    // CRC   = 2 bytes
    //-------------------------------------------------

    uint8_t response[19];
    size_t responseLength = 0;

    if (!readResponse(
            response,
            sizeof(response),
            responseLength,
            100))
    {
        return false;
    }

    if (responseLength != 19 ||
        response[0] != 0x13)
    {
        Serial.print(F(
            "Unexpected AES response length: "
        ));

        Serial.println(responseLength);

        return false;
    }

    memcpy(
        output,
        &response[1],
        16
    );

    return true;
}

//-----------------------------------------------------

void setup()
{
    Serial.begin(9600);

    while (!Serial)
    {
    }

    Wire.begin();
    Wire.setClock(100000);

    delay(100);

    Serial.println();
    Serial.println(F(
        "========================================"
    ));

    Serial.println(F(
        "ATECC608B MANUAL AES TEMPKEY TEST"
    ));

    Serial.println(F(
        "NO EEPROM WRITES / NO LOCK COMMANDS"
    ));

    Serial.println(F(
        "========================================"
    ));

    //-------------------------------------------------
    // Wake
    //-------------------------------------------------

    if (!wakeChip())
    {
        Serial.println(F("Wake failed."));
        return;
    }

    //-------------------------------------------------
    // AES test key
    //
    // AES uses the FIRST 16 bytes of TempKey.
    //-------------------------------------------------

    uint8_t tempKey[32] =
    {
        // AES-128 key
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F,

        // Remaining TempKey bytes
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    Serial.println();
    Serial.print(F("Temporary AES key: "));

    printBuffer(
        tempKey,
        16
    );

    //-------------------------------------------------
    // Load volatile key
    //-------------------------------------------------

    if (!loadTempKey(tempKey))
    {
        Serial.println();
        Serial.println(F(
            "Could not load TempKey."
        ));

        sleepChip();
        return;
    }

    Serial.println(F(
        "TempKey loaded successfully."
    ));

    //-------------------------------------------------
    // NIST AES-128 known-answer test
    //-------------------------------------------------

    const uint8_t plaintext[16] =
    {
        0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB,
        0xCC, 0xDD, 0xEE, 0xFF
    };

    const uint8_t expectedCiphertext[16] =
    {
        0x69, 0xC4, 0xE0, 0xD8,
        0x6A, 0x7B, 0x04, 0x30,
        0xD8, 0xCD, 0xB7, 0x80,
        0x70, 0xB4, 0xC5, 0x5A
    };

    uint8_t ciphertext[16] = {0};
    uint8_t decrypted[16]  = {0};

    Serial.println();
    Serial.print(F("Plaintext : "));
    printBuffer(plaintext, 16);

    //-------------------------------------------------
    // Encrypt
    //-------------------------------------------------

    if (!aesTempKey(
            false,
            plaintext,
            ciphertext))
    {
        Serial.println();
        Serial.println(F(
            "AES ENCRYPT failed."
        ));

        Serial.println(F(
            "No EEPROM contents were changed."
        ));

        sleepChip();
        return;
    }

    Serial.print(F("Ciphertext: "));
    printBuffer(ciphertext, 16);

    Serial.print(F("Expected  : "));
    printBuffer(expectedCiphertext, 16);

    if (memcmp(
            ciphertext,
            expectedCiphertext,
            16) == 0)
    {
        Serial.println(F(
            "AES ENCRYPT KNOWN-ANSWER TEST: PASS"
        ));
    }
    else
    {
        Serial.println(F(
            "AES ENCRYPT KNOWN-ANSWER TEST: FAIL"
        ));
    }

    //-------------------------------------------------
    // Decrypt
    //-------------------------------------------------

    if (!aesTempKey(
            true,
            ciphertext,
            decrypted))
    {
        Serial.println(F(
            "AES DECRYPT failed."
        ));

        sleepChip();
        return;
    }

    Serial.print(F("Decrypted : "));
    printBuffer(decrypted, 16);

    if (memcmp(
            plaintext,
            decrypted,
            16) == 0)
    {
        Serial.println(F(
            "AES ROUND TRIP: PASS"
        ));
    }
    else
    {
        Serial.println(F(
            "AES ROUND TRIP: FAIL"
        ));
    }

    //-------------------------------------------------
    // Sleep clears volatile state
    //-------------------------------------------------

    Serial.println();
    Serial.println(F("Sleeping device..."));

    sleepChip();

    Serial.println(F(
        "TempKey discarded."
    ));

    Serial.println(F(
        "No Configuration Zone writes."
    ));

    Serial.println(F(
        "No Data Zone writes."
    ));

    Serial.println(F(
        "No Lock commands."
    ));
}

void loop()
{
}