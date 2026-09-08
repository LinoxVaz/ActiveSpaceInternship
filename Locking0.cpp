#include <Wire.h>

#define DEVICE_ADDR 0x60

#define WORD_ADDR_IDLE     0x02
#define WORD_ADDR_COMMAND  0x03

#define OPCODE_READ   0x02
#define OPCODE_WRITE  0x12
#define OPCODE_LOCK   0x17
#define OPCODE_AES    0x51

#define AES_SLOT      8

uint8_t configZone[128];

const uint8_t aesKey[16] = {
  0x9E, 0xE7, 0x4F, 0x11,
  0x30, 0x6B, 0x86, 0xFE,
  0x5F, 0x4B, 0x8E, 0x91,
  0xF0, 0xEA, 0x01, 0x98
};

const uint8_t testPlaintext[16] = {
  'A','T','E','C','C','6','0','8',
  'B','-','A','E','S','-','0','1'
};


// ============================================================
// CRC
// ============================================================

void calculateCRC(uint8_t length, const uint8_t *data, uint8_t *crcOut)
{
  uint16_t crcRegister = 0;
  const uint16_t polynom = 0x8005;

  for (uint8_t counter = 0; counter < length; counter++)
  {
    for (uint8_t shiftReg = 0x01; shiftReg != 0; shiftReg <<= 1)
    {
      uint8_t dataBit = (data[counter] & shiftReg) ? 1 : 0;
      uint8_t crcBit = (uint8_t)(crcRegister >> 15);

      crcRegister <<= 1;

      if (dataBit != crcBit)
        crcRegister ^= polynom;
    }
  }

  crcOut[0] = (uint8_t)(crcRegister & 0xFF);
  crcOut[1] = (uint8_t)(crcRegister >> 8);
}


// ============================================================
// PRINT HELPERS
// ============================================================

void printHex(uint8_t b)
{
  if (b < 0x10)
    Serial.print('0');

  Serial.print(b, HEX);
}


void printBuffer(const uint8_t *data, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    printHex(data[i]);

    if (i != len - 1)
      Serial.print(' ');
  }

  Serial.println();
}


// ============================================================
// RESPONSE CRC
// ============================================================

bool checkCRC(uint8_t *response, uint8_t length)
{
  uint8_t crc[2];

  calculateCRC(length - 2, response, crc);

  return (
    response[length - 2] == crc[0] &&
    response[length - 1] == crc[1]
  );
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
  printBuffer(response, 4);

  return (
    response[0] == 0x04 &&
    response[1] == 0x11
  );
}


// ============================================================
// READ CONFIG WORD
// ============================================================

bool readConfigWord(uint8_t wordAddr, uint8_t *data4)
{
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

  if (Wire.endTransmission() != 0)
    return false;

  delay(5);

  Wire.requestFrom(DEVICE_ADDR, (uint8_t)7);

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
// READ ALL CONFIG
// ============================================================

bool readConfig()
{
  for (uint8_t word = 0; word < 32; word++)
  {
    if (!readConfigWord(word, &configZone[word * 4]))
      return false;
  }

  return true;
}


// ============================================================
// CHECK CONFIG
// ============================================================

bool verifyExpectedConfig()
{
  uint16_t slotCfg =
    (uint16_t)configZone[36] |
    ((uint16_t)configZone[37] << 8);

  uint16_t keyCfg =
    (uint16_t)configZone[112] |
    ((uint16_t)configZone[113] << 8);

  uint16_t slotLocked =
    (uint16_t)configZone[88] |
    ((uint16_t)configZone[89] << 8);


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


// ============================================================
// STATUS RESPONSE
// ============================================================

bool readStatusResponse()
{
  Wire.requestFrom(DEVICE_ADDR, (uint8_t)4);

  if (Wire.available() < 4)
    return false;

  uint8_t response[4];

  for (uint8_t i = 0; i < 4; i++)
    response[i] = Wire.read();

  Serial.print(F("Response: "));
  printBuffer(response, 4);

  if (!checkCRC(response, 4))
    return false;

  return (
    response[0] == 0x04 &&
    response[1] == 0x00
  );
}


// ============================================================
// LOCK CONFIG
// ============================================================

bool lockConfigZone()
{
  uint8_t packet[5];

  packet[0] = 0x07;
  packet[1] = OPCODE_LOCK;

  // Config zone + ignore summary CRC
  packet[2] = 0x80;

  packet[3] = 0x00;
  packet[4] = 0x00;

  uint8_t crc[2];

  calculateCRC(sizeof(packet), packet, crc);

  Serial.println(F("LOCKING CONFIG ZONE..."));

  Wire.beginTransmission(DEVICE_ADDR);
  Wire.write(WORD_ADDR_COMMAND);
  Wire.write(packet, sizeof(packet));
  Wire.write(crc, 2);

  if (Wire.endTransmission() != 0)
    return false;

  delay(40);

  return readStatusResponse();
}


// ============================================================
// LOCK DATA
// ============================================================

bool lockDataZone()
{
  uint8_t packet[5];

  packet[0] = 0x07;
  packet[1] = OPCODE_LOCK;

  // Data / OTP + ignore summary CRC
  packet[2] = 0x81;

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

    if (wireResult != 0)
    {
        Serial.println(F("I2C transmission FAILED"));
        return false;
    }

  delay(40);

  return readStatusResponse();
}


// ============================================================
// WRITE AES KEY SLOT 8
//
// Uses 32-byte Data-zone write.
// Requires Wire BUFFER_LENGTH >= 41.
// ============================================================

bool writeAESKey()
{
  uint8_t packet[37];

  packet[0] = 0x27;
  packet[1] = OPCODE_WRITE;

  // Data zone + 32-byte write
  packet[2] = 0x82;

  // Slot 8, block 0
  packet[3] = 0x40;
  packet[4] = 0x00;


  // First 16 bytes = AES key

  for (uint8_t i = 0; i < 16; i++)
    packet[5 + i] = aesKey[i];


  // Remaining bytes of slot block = zero

  for (uint8_t i = 16; i < 32; i++)
    packet[5 + i] = 0x00;


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


  if (n1 != sizeof(packet) || n2 != 2)
  {
    Serial.println(F("ERROR: Wire buffer too small!"));
    return false;
  }


  if (Wire.endTransmission() != 0)
    return false;


  delay(40);


  return readStatusResponse();
}


// ============================================================
// AES COMMAND
//
// decrypt=false = encrypt
// decrypt=true  = decrypt
// ============================================================

bool aesBlock(
  bool decrypt,
  const uint8_t input[16],
  uint8_t output[16])
{
  uint8_t packet[21];

  packet[0] = 0x17;
  packet[1] = OPCODE_AES;

  packet[2] =
    decrypt ? 0x01 : 0x00;

  // KeyID = Slot 8
  packet[3] = AES_SLOT;
  packet[4] = 0x00;


  for (uint8_t i = 0; i < 16; i++)
    packet[5 + i] = input[i];


  uint8_t crc[2];

  calculateCRC(sizeof(packet), packet, crc);


  Wire.beginTransmission(DEVICE_ADDR);

  Wire.write(WORD_ADDR_COMMAND);
  Wire.write(packet, sizeof(packet));
  Wire.write(crc, 2);


  if (Wire.endTransmission() != 0)
    return false;


  delay(15);


  // response:
  // count + 16 data + 2 CRC = 19

  Wire.requestFrom(
    DEVICE_ADDR,
    (uint8_t)19
  );


  if (Wire.available() < 19)
    return false;


  uint8_t response[19];

  for (uint8_t i = 0; i < 19; i++)
    response[i] = Wire.read();


  if (response[0] != 19)
  {
    if (response[0] == 4)
    {
      Serial.print(F("AES error status = 0x"));
      printHex(response[1]);
      Serial.println();
    }

    return false;
  }


  if (!checkCRC(response, 19))
    return false;


  for (uint8_t i = 0; i < 16; i++)
    output[i] = response[i + 1];


  return true;
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

  delay(1500);


  Serial.println();
  Serial.println(F("=============================="));
  Serial.println(F("ATECC608B AES provisioning"));
  Serial.println(F("=============================="));


  // ----------------------------------------------------------
  // WAKE
  // ----------------------------------------------------------

  if (!wakeChip())
  {
    Serial.println(F("STOP: wake failed"));
    return;
  }


  // ----------------------------------------------------------
  // VERIFY CURRENT CONFIGURATION
  // ----------------------------------------------------------

  if (!readConfig())
  {
    Serial.println(F("STOP: config read failed"));
    return;
  }


  if (!verifyExpectedConfig())
  {
    Serial.println(F("STOP: configuration not as expected"));
    Serial.println(F("Nothing was locked."));
    return;
  }


  Serial.println(F("Configuration verified."));


  // ==========================================================
  // IRREVERSIBLE STEP 1
  // LOCK CONFIG
  // ==========================================================

  if (!lockConfigZone())
  {
    Serial.println(F("STOP: Config lock failed"));
    return;
  }


  // Read configuration again

  if (!readConfig())
  {
    Serial.println(F("STOP: cannot verify Config lock"));
    return;
  }


  if (configZone[87] != 0x00)
  {
    Serial.println(F("STOP: LockConfig did not become 00"));
    return;
  }


  if (configZone[86] != 0x55)
  {
    Serial.println(F("STOP: Data unexpectedly locked"));
    return;
  }


  Serial.println(F("CONFIG ZONE LOCKED successfully."));
  Serial.println(F("Data zone still unlocked."));


  // ==========================================================
  // WRITE AES KEY
  // ==========================================================

  if (!writeAESKey())
  {
    Serial.println(F("STOP: AES key write failed"));
    Serial.println(F("Data zone remains unlocked."));
    return;
  }


  Serial.println(F("AES key write accepted."));


  // ==========================================================
  // IRREVERSIBLE STEP 2
  // LOCK DATA
  // ==========================================================

  if (!lockDataZone())
  {
    Serial.println(F("STOP: Data lock failed"));
    return;
  }


  // Verify LockValue

  if (!readConfig())
  {
    Serial.println(F("STOP: cannot verify Data lock"));
    return;
  }


  if (configZone[86] != 0x00)
  {
    Serial.println(F("STOP: LockValue did not become 00"));
    return;
  }


  Serial.println(F("DATA ZONE LOCKED successfully."));


  // ==========================================================
  // AES TEST
  // ==========================================================

  uint8_t ciphertext[16];
  uint8_t recovered[16];


  Serial.println();
  Serial.print(F("Plaintext : "));
  printBuffer(testPlaintext, 16);


  if (!aesBlock(
        false,
        testPlaintext,
        ciphertext))
  {
    Serial.println(F("AES encrypt FAILED"));
    return;
  }


  Serial.print(F("Ciphertext: "));
  printBuffer(ciphertext, 16);


  if (!aesBlock(
        true,
        ciphertext,
        recovered))
  {
    Serial.println(F("AES decrypt FAILED"));
    return;
  }


  Serial.print(F("Recovered : "));
  printBuffer(recovered, 16);


  bool match = true;

  for (uint8_t i = 0; i < 16; i++)
  {
    if (recovered[i] != testPlaintext[i])
    {
      match = false;
      break;
    }
  }


  Serial.println();


  if (match)
  {
    Serial.println(F("================================"));
    Serial.println(F("AES TEST SUCCESS"));
    Serial.println(F("Encrypt + decrypt verified."));
    Serial.println(F("================================"));
  }
  else
  {
    Serial.println(F("================================"));
    Serial.println(F("AES TEST FAILED"));
    Serial.println(F("Recovered data does not match."));
    Serial.println(F("================================"));
  }


  idleChip();
}


void loop()
{
}