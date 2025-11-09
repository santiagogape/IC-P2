
// ************************************************************************************************************************
// message-protocol
// ************************************************************************************************************************

inline uint8_t mask_error(uint8_t errorID, uint8_t code) {
  return (errorID << 4) | (code & DEV_MASC);
}

inline uint8_t hexToDevId(uint8_t dev) {
  return (dev - DEV_MIN) >> 1;
}

inline uint8_t codeToDev(uint8_t code) {
  return DEV_MIN + ((code & DEV_MASC) << 1);
}

// =======================================================
// I/O de bajo nivel unificada para Serial1
// =======================================================
inline size_t sendCode(Stream &port, uint8_t code) { return port.write(&code, 1); }

inline bool receiveCode(Stream &port, uint8_t *outCode, uint32_t timeout_ms = 100) {
  const uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    if (port.available() >= 1) {
      *outCode = (uint8_t)port.read();
      return true;
    }
  }
  return false;
}

size_t sendMessage(Stream &port, const Message &msg) {
  return port.write((const uint8_t*)&msg, sizeof(Message));
}

bool receiveMessage(Stream &port, Message *outMsg, uint32_t timeout_ms = 100) {
  const uint32_t t0 = millis();
  uint8_t *p = (uint8_t*)outMsg;
  size_t got = 0;
  while ((millis() - t0) < timeout_ms) {
  //while (true){
    while (port.available() && got < sizeof(Message)) {
      p[got++] = (uint8_t)port.read();
    }
    if (got == sizeof(Message)) return true;
    /*
    else if ((millis() - t0) < timeout_ms) {
      SerialUSB.print(F("TIMEOUT: code="));
      SerialUSB.print(outMsg->code,HEX);
      SerialUSB.print(F(" param="));
      SerialUSB.print(outMsg->param);
      SerialUSB.print(F(" at "));
      SerialUSB.println(millis());
    }
    */
    
  }
  SerialUSB.print(F("TIMEOUT: code="));
  SerialUSB.print(outMsg->code,HEX);
  SerialUSB.println(F(" param="));
  SerialUSB.println(outMsg->param);
  return false;
}

bool verifyReply(uint8_t sentCode, const Message &reply) {
  if (reply.code == sentCode) return true;

  if ((reply.code & COM_MASC) >= UNDER_MIN_DELAY) {
    switch (reply.code & COM_MASC) {
      case UNDER_MIN_DELAY:
        SerialUSB.print(F("error: UNDER_MIN_DELAY. min delay: "));
        SerialUSB.println(reply.param);
        break;
      case SENSOR_DISCONECTED:
        SerialUSB.print(F("error: SENSOR_DISCONECTED "));
        SerialUSB.println(codeToDev(sentCode), HEX);
        break;
    }
  } else {
    SerialUSB.print(F("EXPECTED CODE: "));
    SerialUSB.print(sentCode);
    SerialUSB.print(F(" RECEIVED code="));
    SerialUSB.print(reply.code);
    SerialUSB.print(F(" param="));
    SerialUSB.println(reply.param);
  }
  return false;
}