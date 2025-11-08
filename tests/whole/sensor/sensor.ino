#include <Wire.h>
#include <ChRt.h>

// ************************************************************************************************************************
// message-protocol
// ************************************************************************************************************************

// ==== Mensaje binario común (Supervisor <-> Sensor) ====
// Compacto: 1 byte code (cmd|dev), 2 bytes param (p.ej. distancia)
typedef struct __attribute__((packed)) {
  uint8_t  code;
  uint16_t param;
} Message;

#define COM_MASC 0xF0
#define DEV_MASC 0x0F
#define DEV_MIN 0xE0

inline uint8_t hexToDevId(uint8_t dev){
  return (dev - DEV_MIN)>>1;
}

inline uint8_t codeToDev(uint8_t code){
  return DEV_MIN + ((code & DEV_MASC)<<1);
}


// =======================================================
// I/O de bajo nivel unificada para Serial1
//  - Supervisor: sendCode() -> receiveMessage()
//  - Sensor:     receiveCode() -> sendMessage()
// =======================================================

/**
 * Envía SOLO el código (1 byte). Útil para "one-shot" controlado por Supervisor.
 * Devuelve bytes enviados (1 si OK).
 */
inline size_t sendCode(Stream &port, uint8_t code) {
  return port.write(&code, 1);
}

/**
 * Recibe SOLO el código (1 byte) con timeout en ms.
 * Devuelve true si se leyó 1 byte; guarda en outCode.
 */
inline bool receiveCode(Stream &port, uint8_t *outCode, uint32_t timeout_ms = 100) {
  const uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    if (port.available() >= 1) {
      *outCode = (uint8_t)port.read();
      return true;
    }
  }
  return false; // timeout
}

/**
 * Envía un Message completo (3 bytes: code + param16).
 * Devuelve bytes enviados (3 si OK).
 */
inline size_t sendMessage(Stream &port, const Message &msg) {
  return port.write((const uint8_t*)&msg, sizeof(Message));
}

/**
 * Recibe un Message completo con timeout en ms.
 * Devuelve true si logró leer exactamente sizeof(Message) bytes.
 */
inline bool receiveMessage(Stream &port, Message *outMsg, uint32_t timeout_ms = 50) {
  const uint32_t t0 = millis();
  uint8_t *p = (uint8_t*)outMsg;
  size_t   got = 0;
  while ((millis() - t0) < timeout_ms) {
    while (port.available() && got < sizeof(Message)) {
      p[got++] = (uint8_t)port.read();
    }
    if (got == sizeof(Message)) return true; // recibido completo
  }
  return false; // timeout
}

/**
 * Verifica que la respuesta del Sensor corresponde al código enviado.
 * Úsalo SIEMPRE que el Supervisor haga sendCode() y luego receiveMessage().
 */
inline bool verifyReply(uint8_t sentCode, const Message &reply) {
  return reply.code == sentCode;
}


// ************************************************************************************************************************
// srf02-i2c-communication
// ************************************************************************************************************************

// =============================================================
// Constantes de SRF02
// =============================================================
#define SRF02_I2C_INIT_DELAY      100   // ms
#define SRF02_RANGING_DELAY       70    // ms (mínimo entre disparos)

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define SOFTWARE_REVISION byte(0x00)
#define RANGE_HIGH_BYTE byte(2)
#define RANGE_LOW_BYTE byte(3)
#define AUTOTUNE_MINIMUM_HIGH_BYTE byte(4)
#define AUTOTUNE_MINIMUM_LOW_BYTE byte(5)

// === Modos de disparo ===
#define REAL_RANGING_MODE_INCHES   byte(80)
#define REAL_RANGING_MODE_CMS      byte(81)
#define REAL_RANGING_MODE_USECS    byte(82)

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

  Wire.requestFrom(address, byte(1));
  while (!Wire.available()) { /* espera activa corta */ }
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
uint16_t srf02_oneShot(uint8_t address, uint8_t unit) {
  uint8_t mode;
  switch (unit) {
    case 4: mode = REAL_RANGING_MODE_INCHES; break; // 
    case 5: mode = REAL_RANGING_MODE_CMS;    break;
    case 6: mode = REAL_RANGING_MODE_USECS;  break;
    default: mode = REAL_RANGING_MODE_CMS;   break;
  }

  srf02_writeCommand(address, mode);
  delay(SRF02_RANGING_DELAY);

  uint8_t high = srf02_readRegister(address, RANGE_HIGH_BYTE);
  uint8_t low  = srf02_readRegister(address, RANGE_LOW_BYTE);

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



// ************************************************************************************************************************
// sensor
// ************************************************************************************************************************
// ============================================================
// Estructura de configuración local del sensor
// ============================================================

typedef struct {
  uint8_t addr;       // Dirección lógica del sensor (0xE0, 0xF2…)
  uint8_t i2c_addr;   // Dirección de 7 bits para Wire
  uint16_t delay_ms;  // Retardo mínimo entre disparos
} SensorConfig;

// ============================================================
// Configuración de sensores conectados
// ============================================================

#define N_SENSORS 2
SensorConfig sensors[N_SENSORS] = {
  {0xE0, byte(0xE0 >> 1), 70},
  {0xF2, byte(0xF2 >> 1), 70}
};


// ============================================================
/*   Mailboxes por sensor
   - Empaquetamos Message en uint32_t:
     bits: [7:0]=code, [23:8]=param
*/
// ============================================================
static inline uint32_t packMsg(const Message& m) {
  return ( (uint32_t)m.code ) | ( ((uint32_t)m.param) << 8 );
}
static inline Message unpackMsg(uint32_t w) {
  Message m;
  m.code  = (uint8_t)(w & 0xFF);
  m.param = (uint16_t)((w >> 8) & 0xFFFF);
  return m;
}

/* Mailboxes y storage */
#define MBOX_DEPTH 8
static mailbox_t mbox[N_SENSORS];
static msg_t      mbox_buf[N_SENSORS][MBOX_DEPTH];

// ============================================================
//   Hilo receptor (único lector de Serial1)
// ============================================================
static THD_WORKING_AREA(waRx, 384);
static THD_FUNCTION(rxThread, arg) {
  (void)arg;
  Message in;
  while (!chThdShouldTerminateX()) {
    if (receiveMessage(Serial1, &in, 50)) {
      uint8_t dev = codeToDev(in.code);

      /* Seleccionar a qué mailbox va */
      int target = -1;
      for (int i = 0; i < N_SENSORS; i++) {
        if (sensors[i].addr == dev) { target = i; break; }
      }
      if (target < 0) {
        // Mensaje para un dispositivo desconocido -> descartar silenciosamente
        continue;
      }

      // Encolar (si lleno, descartamos para no bloquear RX)
      msg_t packed = (msg_t) packMsg(in);
      (void) chMBPostTimeout(&mbox[target], packed, TIME_IMMEDIATE);
    }
    chThdSleepMilliseconds(5);
  }
}

// ============================================================
//   Hilo por sensor (consume su mailbox)
// ============================================================
static THD_WORKING_AREA(waSensor[N_SENSORS], 384);
static THD_FUNCTION(sensorThread, arg) {
  SensorConfig* self = (SensorConfig*)arg;
  int idx = -1;
  for (int i = 0; i < N_SENSORS; i++) if (&sensors[i] == self) idx = i;

  // Bucle: espera mensajes dirigidos a este sensor
  while (!chThdShouldTerminateX()) {
    msg_t packed;
    if (chMBFetchTimeout(&mbox[idx], &packed, TIME_MS2I(100)) == MSG_OK) {
      Message cmd = unpackMsg((uint32_t)packed);
      uint8_t cmdId = cmd.code >> 4;

      if (cmdId == 7) {
        // Ajuste de delay local (no responde)
        self->delay_ms = cmd.param;
      } else {
        // Disparo de medida (unidad desde cmdId si 4..6, si no: cm)
        uint8_t unitForShot = (cmdId >= 4 && cmdId <= 6) ? cmdId : 5;
        cmd.param = srf02_oneShot(self->i2c_addr, unitForShot);
        sendMessage(Serial1, cmd);
        chThdSleepMilliseconds(self->delay_ms);
      }
    }
  }
}

// ============================================================
//   Arranque con ChibiOS
// ============================================================
static void chSetup(void) {
  // Inicializar mailboxes
  for (int i = 0; i < N_SENSORS; i++) {
    chMBObjectInit(&mbox[i], mbox_buf[i], MBOX_DEPTH);
  }

  // Crear hilos de sensores
  for (int i = 0; i < N_SENSORS; i++) {
    chThdCreateStatic(waSensor[i], sizeof(waSensor[i]),
                      NORMALPRIO + 1, sensorThread, &sensors[i]);
  }

  // Crear hilo receptor (único que lee Serial1)
  chThdCreateStatic(waRx, sizeof(waRx),
                    NORMALPRIO + 2, rxThread, nullptr);
}

// ============================================================
//   setup()/loop() (entrega control a ChibiOS)
// ============================================================
void setup() {
  Serial1.begin(9600);
  Wire.begin();
  chBegin(chSetup);
}

void loop() {
  // no se usa (ChibiOS controla la ejecución)
}