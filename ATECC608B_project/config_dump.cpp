#include <Wire.h>

#define DEVICE_ADDR 0x47

#define WORD_ADDR_IDLE     0x02
#define WORD_ADDR_COMMAND  0x03

#define OPCODE_READ   0x02
#define OPCODE_WRITE  0x12

uint8_t configZone[128];


// ============================================================
// CRC
// ============================================================

void calculateCRC(
    uint8_t length,
    const uint8_t *data,
    uint8_t *crcOut)
{
    uint16_t crcRegister = 0;
    const uint16_t polynom = 0x8005;

    for (uint8_t counter = 0; counter < length; counter++)
    {
        for (uint8_t shiftReg = 0x01;
             shiftReg != 0;
             shiftReg <<= 1)
        {
            uint8_t dataBit =
                (data[counter] & shiftReg) ? 1 : 0;

            uint8_t crcBit =
                (uint8_t)(crcRegister >> 15);

            crcRegister <<= 1;

            if (dataBit != crcBit)
                crcRegister ^= polynom;
        }
    }

    crcOut[0] = (uint8_t)(crcRegister & 0xFF);
    crcOut[1] = (uint8_t)(crcRegister >> 8);
}


// ============================================================
// HEX
// ============================================================

void printHex(uint8_t b)
{
    if (b < 0x10)
        Serial.print('0');

    Serial.print(b, HEX);
}


// ============================================================
// WAKE
// ============================================================

bool wakeChip()
{
    Wire.beginTransmission(0x00);
    Wire.write(0x00);
    Wire.endTransmission();

    delayMicroseconds(1500);

    Wire.requestFrom(DEVICE_ADDR, (uint8_t)4);

    if (Wire.available() < 4)
        return false;

    uint8_t response[4];

    for (uint8_t i = 0; i < 4; i++)
        response[i] = Wire.read();

    Serial.print(F("Wake: "));

    for (uint8_t i = 0; i < 4; i++)
    {
        printHex(response[i]);
        Serial.print(' ');
    }

    Serial.println();

    return (
        response[0] == 0x04 &&
        response[1] == 0x11
    );
}


// ============================================================
// RESPONSE CRC
// ============================================================

bool checkCRC(
    uint8_t *response,
    uint8_t length)
{
    uint8_t crc[2];

    calculateCRC(
        length - 2,
        response,
        crc
    );

    return (
        response[length - 2] == crc[0] &&
        response[length - 1] == crc[1]
    );
}


// ============================================================
// READ CONFIG WORD
// ============================================================

bool readConfigWord(
    uint8_t wordAddr,
    uint8_t *data4)
{
    uint8_t packet[5];

    packet[0] = 0x07;
    packet[1] = OPCODE_READ;
    packet[2] = 0x00;
    packet[3] = wordAddr;
    packet[4] = 0x00;

    uint8_t crc[2];

    calculateCRC(
        sizeof(packet),
        packet,
        crc
    );

    Wire.beginTransmission(DEVICE_ADDR);

    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    if (Wire.endTransmission() != 0)
        return false;

    delay(5);

    Wire.requestFrom(
        DEVICE_ADDR,
        (uint8_t)7
    );

    if (Wire.available() < 7)
        return false;

    uint8_t response[7];

    for (uint8_t i = 0; i < 7; i++)
        response[i] = Wire.read();

    if (response[0] != 0x07)
        return false;

    if (!checkCRC(response, 7))
        return false;

    for (uint8_t i = 0; i < 4; i++)
        data4[i] = response[i + 1];

    return true;
}


// ============================================================
// FULL CONFIG READ
// ============================================================

bool readConfig()
{
    for (uint8_t word = 0; word < 32; word++)
    {
        if (!readConfigWord(
                word,
                &configZone[word * 4]))
        {
            Serial.print(F("Read failed word 0x"));
            Serial.println(word, HEX);

            return false;
        }
    }

    return true;
}


// ============================================================
// WRITE CONFIG WORD
// ============================================================

bool writeConfigWord(
    uint8_t wordAddr,
    uint8_t d0,
    uint8_t d1,
    uint8_t d2,
    uint8_t d3)
{
    uint8_t packet[9];

    packet[0] = 0x0B;
    packet[1] = OPCODE_WRITE;

    // Configuration zone
    // 4-byte write
    packet[2] = 0x00;

    packet[3] = wordAddr;
    packet[4] = 0x00;

    packet[5] = d0;
    packet[6] = d1;
    packet[7] = d2;
    packet[8] = d3;

    uint8_t crc[2];

    calculateCRC(
        sizeof(packet),
        packet,
        crc
    );


    Serial.print(F("Write word 0x"));
    Serial.print(wordAddr, HEX);
    Serial.print(F(": "));

    printHex(d0); Serial.print(' ');
    printHex(d1); Serial.print(' ');
    printHex(d2); Serial.print(' ');
    printHex(d3); Serial.println();


    Wire.beginTransmission(DEVICE_ADDR);

    Wire.write(WORD_ADDR_COMMAND);
    Wire.write(packet, sizeof(packet));
    Wire.write(crc, 2);

    uint8_t result = Wire.endTransmission();

    if (result != 0)
    {
        Serial.print(F("I2C error "));
        Serial.println(result);

        return false;
    }


    delay(30);


    Wire.requestFrom(
        DEVICE_ADDR,
        (uint8_t)4
    );

    if (Wire.available() < 4)
    {
        Serial.println(F("No write response"));
        return false;
    }


    uint8_t response[4];

    for (uint8_t i = 0; i < 4; i++)
        response[i] = Wire.read();


    Serial.print(F("Response: "));

    for (uint8_t i = 0; i < 4; i++)
    {
        printHex(response[i]);
        Serial.print(' ');
    }

    Serial.println();


    if (!checkCRC(response, 4))
    {
        Serial.println(F("Bad response CRC"));
        return false;
    }


    if (response[1] != 0x00)
    {
        Serial.print(F("ATECC status: 0x"));
        printHex(response[1]);
        Serial.println();

        return false;
    }


    return true;
}


// ============================================================
// VERIFY WORD
// ============================================================

bool verifyWord(
    uint8_t wordAddr,
    uint8_t a,
    uint8_t b,
    uint8_t c,
    uint8_t d)
{
    uint8_t readback[4];

    if (!readConfigWord(
            wordAddr,
            readback))
    {
        return false;
    }


    Serial.print(F("Verify word 0x"));
    Serial.print(wordAddr, HEX);
    Serial.print(F(": "));

    for (uint8_t i = 0; i < 4; i++)
    {
        printHex(readback[i]);
        Serial.print(' ');
    }

    Serial.println();


    return (
        readback[0] == a &&
        readback[1] == b &&
        readback[2] == c &&
        readback[3] == d
    );
}


// ============================================================
// DUMP
// ============================================================

void dumpConfig()
{
    Serial.println();
    Serial.println(F("Configuration Zone:"));

    for (uint8_t row = 0; row < 8; row++)
    {
        uint8_t base = row * 16;

        printHex(base);
        Serial.print(F(": "));

        for (uint8_t j = 0; j < 16; j++)
        {
            printHex(configZone[base + j]);
            Serial.print(' ');
        }

        Serial.println();
    }
}


// ============================================================
// FINAL VERIFICATION
// ============================================================

bool finalVerify()
{
    uint16_t slotConfig8 =
        (uint16_t)configZone[36] |
        ((uint16_t)configZone[37] << 8);

    uint16_t keyConfig8 =
        (uint16_t)configZone[112] |
        ((uint16_t)configZone[113] << 8);

    uint16_t slotLocked =
        (uint16_t)configZone[88] |
        ((uint16_t)configZone[89] << 8);


    Serial.println();
    Serial.println(F("FINAL CHECK"));

    Serial.print(F("LockConfig : "));
    printHex(configZone[87]);
    Serial.println();

    Serial.print(F("LockValue  : "));
    printHex(configZone[86]);
    Serial.println();

    Serial.print(F("AES Enable : "));
    Serial.println(
        (configZone[13] & 1)
        ? F("YES")
        : F("NO")
    );


    Serial.print(F("SlotCfg 8  : 0x"));
    Serial.println(slotConfig8, HEX);

    Serial.print(F("KeyCfg 8   : 0x"));
    Serial.println(keyConfig8, HEX);

    Serial.print(F("SlotLocked : 0x"));
    Serial.println(slotLocked, HEX);


    bool ok =
        configZone[87] == 0x55 &&
        configZone[86] == 0x55 &&
        (configZone[13] & 0x01) &&
        slotConfig8 == 0x8F8F &&
        keyConfig8 == 0x0038 &&
        slotLocked == 0xFFFF;


    if (ok)
    {
        Serial.println();
        Serial.println(F("*** CONFIG OK ***"));
        Serial.println(F("NO LOCK COMMAND EXECUTED"));
    }
    else
    {
        Serial.println();
        Serial.println(F("*** CONFIG ERROR ***"));
        Serial.println(F("DO NOT LOCK CHIP"));
    }


    return ok;
}


// ============================================================
// IDLE
// ============================================================

void idleChip()
{
    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(WORD_ADDR_IDLE);
    Wire.endTransmission();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(9600);

    Wire.begin();
    Wire.setClock(100000);

    delay(1000);


    Serial.println();
    Serial.println(F("ATECC608B Slot 8 AES setup"));


    // --------------------------------------------------------
    // WAKE
    // --------------------------------------------------------

    if (!wakeChip())
    {
        Serial.println(F("Wake failed"));
        return;
    }


    // --------------------------------------------------------
    // INITIAL CONFIG READ
    // --------------------------------------------------------

    if (!readConfig())
    {
        Serial.println(F("Config read failed"));
        return;
    }


    dumpConfig();


    // --------------------------------------------------------
    // CRITICAL SAFETY CHECK
    // --------------------------------------------------------

    if (configZone[87] != 0x55)
    {
        Serial.println(F("STOP: CONFIG locked"));
        return;
    }


    if (configZone[86] != 0x55)
    {
        Serial.println(F("STOP: DATA locked"));
        return;
    }


    if (!(configZone[13] & 0x01))
    {
        Serial.println(F("STOP: AES disabled"));
        return;
    }


    Serial.println(F("Zones unlocked - proceeding"));


    // ========================================================
    // STEP 1
    // SlotConfig[8]
    //
    // Word 9 = bytes 36..39
    //
    // Change 36/37 only.
    // Preserve 38/39.
    // ========================================================

    Serial.println();
    Serial.println(F("1. SlotConfig[8] -> 8F8F"));


    if (!writeConfigWord(
            9,
            0x8F,
            0x8F,
            configZone[38],
            configZone[39]))
    {
        Serial.println(F("SlotConfig write FAILED"));
        return;
    }


    if (!verifyWord(
            9,
            0x8F,
            0x8F,
            configZone[38],
            configZone[39]))
    {
        Serial.println(F("SlotConfig verify FAILED"));
        return;
    }


    // Update local copy

    configZone[36] = 0x8F;
    configZone[37] = 0x8F;


    // ========================================================
    // STEP 2
    // KeyConfig[8]
    //
    // Word 28 = bytes 112..115
    //
    // 38 00 = AES
    // Preserve bytes 114/115
    // ========================================================

    Serial.println();
    Serial.println(F("2. KeyConfig[8] -> 0038 AES"));


    if (!writeConfigWord(
            28,
            0x38,
            0x00,
            configZone[114],
            configZone[115]))
    {
        Serial.println(F("KeyConfig write FAILED"));
        return;
    }


    if (!verifyWord(
            28,
            0x38,
            0x00,
            configZone[114],
            configZone[115]))
    {
        Serial.println(F("KeyConfig verify FAILED"));
        return;
    }


    configZone[112] = 0x38;
    configZone[113] = 0x00;


    // ========================================================
    // STEP 3
    // SlotLocked
    //
    // Word 22 = bytes 88..91
    //
    // FF FF
    //
    // Preserve ChipOptions 90/91
    // ========================================================

    Serial.println();
    Serial.println(F("3. SlotLocked -> FFFF"));


    if (!writeConfigWord(
            22,
            0xFF,
            0xFF,
            configZone[90],
            configZone[91]))
    {
        Serial.println(F("SlotLocked write FAILED"));
        return;
    }


    if (!verifyWord(
            22,
            0xFF,
            0xFF,
            configZone[90],
            configZone[91]))
    {
        Serial.println(F("SlotLocked verify FAILED"));
        return;
    }


    // ========================================================
    // STEP 4
    // Read everything again
    // ========================================================

    Serial.println();
    Serial.println(F("Reading final configuration..."));


    if (!readConfig())
    {
        Serial.println(F("Final read FAILED"));
        return;
    }


    dumpConfig();


    // ========================================================
    // STEP 5
    // Final verification
    // ========================================================

    finalVerify();


    idleChip();
}


void loop()
{
}