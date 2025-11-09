// =============================================================
// Funciones de bajo nivel
// =============================================================

/**
 * Envía un comando al registro 0x00 del SRF02.
 * address = dirección I2C (7 bits)
 * command = código de comando SRF02 (p. ej. REAL_RANGING_MODE_CMS)
 */
inline void srf02_writeCommand(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(COMMAND_REGISTER);
  Wire.write(command);
  Wire.endTransmission();
}

/**
 * Lee un registro del SRF02 (1 byte)
 */
inline uint8_t srf02_readRegister(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  SerialUSB.println("expecting");
  Wire.requestFrom(address, byte(1));
  SerialUSB.println("readed");
  uint32_t t0 = millis();
  while (!Wire.available()) {
    if (millis() - t0 > 120) {  // timeout de 120 ms
      SerialUSB.print(F("[ERROR] I2C read timeout addr=0x"));
      SerialUSB.print(address, HEX);
      SerialUSB.print(F(" reg=0x"));
      SerialUSB.println(reg, HEX);
      return 0x00;
    }
  }
  SerialUSB.println("done");
  return Wire.read();
}

/**
 * Realiza un disparo de medición (one-shot) con la unidad especificada.
 * Retorna el valor de distancia como uint16_t (high<<8 | low)
 * 
 * units:
 *   4 → pulgadas (inches)
 *   5 → centímetros (cm)
 *   6 → microsegundos (µs)
 * en terminal.ino  /supervisor se indica que 
 * las unidades se identifican por 4,5,6
 */
void srf02_oneShot(uint8_t address, uint8_t unit) {
  uint8_t mode;
  switch (unit) {
    case 4: mode = REAL_RANGING_MODE_INCHES; break; // 
    case 5: mode = REAL_RANGING_MODE_CMS;    break;
    case 6: mode = REAL_RANGING_MODE_USECS;  break;
    default: mode = REAL_RANGING_MODE_CMS;   break;
  }

  srf02_writeCommand(address, mode);
}

uint16_t srf02_read_result(uint8_t address) {
  SerialUSB.println("high");
  uint8_t high = srf02_readRegister(address, RANGE_HIGH_BYTE);
  SerialUSB.println("low");
  uint8_t low  = srf02_readRegister(address, RANGE_LOW_BYTE);
  SerialUSB.print("one-shot: ");SerialUSB.print(address,HEX);SerialUSB.print(":");SerialUSB.println((uint16_t)((high<<8)|low));
  return (uint16_t)((high << 8) | low);
}


/**
 * Lee información de estado del SRF02.
 * Devuelve software_revision y valor autotune mínimo (uint16_t)
 */
void srf02_status(uint8_t address, uint8_t *rev, uint16_t *autoMin) {
  if (rev)
    *rev = srf02_readRegister(address, SOFTWARE_REVISION);

  if (autoMin) {
    uint8_t high = srf02_readRegister(address, AUTOTUNE_MINIMUM_HIGH_BYTE);
    uint8_t low  = srf02_readRegister(address, AUTOTUNE_MINIMUM_LOW_BYTE);
    *autoMin = (uint16_t)((high << 8) | low);
  }
}
