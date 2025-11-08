#include <ChRt.h>


// ************************************************************************************************************************
// message-protocol
// ************************************************************************************************************************

#define COM_MASC 0xF0
#define DEV_MASC 0x0F
#define DEV_MIN 0xE0

// ==== Mensaje binario común (Supervisor <-> Sensor) ====
// Compacto: 1 byte code (cmd|dev), 2 bytes param (p.ej. distancia)
typedef struct __attribute__((packed)) {
  uint8_t  code;
  uint16_t param;
} Message;



inline uint8_t hexToDevId(uint8_t dev){
  return (dev - DEV_MIN)>>1;
}

inline uint8_t codeToDev(uint8_t code){
  return DEV_MIN + ((code & DEV_MASC) << 1);
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
// terminal
// ************************************************************************************************************************

// ============================================================
// CLI: Lee, interpreta y ejecuta comandos desde SerialUSB
// ============================================================

#define HELP "help"
#define MAX_ARGS 4
#define COM_NUM 7


// Estructuras auxiliares
typedef struct {
  char name[9];
  uint8_t id;
} Argument;

// Tabla de comandos
const Argument commands[COM_NUM] = {
  {"us", 9}, {"one-shot",1}, {"on",2}, {"off",3},
  {"unit",4}, {"delay",7}, {"status",8}
};

// Tabla de unidades (usadas en SRF02)
const char UNITS[3][4] = {"inc","cm","ms"}; // inches, centimeters, milliseconds

// ============================================================
// Validadores básicos
// ============================================================
bool isHex(const char *s) {
  return (strlen(s) == 4 && s[0] == '0' && s[1] == 'x');
}

bool isNumber(const char *s) {
  for (int i = 0; s[i]; i++) if (!isdigit(s[i])) return false;
  return true;
}

uint8_t parseHex(char *hex) {
  return (uint8_t)strtol(hex, NULL, 16);
}

uint16_t parseInt(const char *num, bool *error) {

  // Validar que todos sean dígitos
  for (const char *p = num; *p; p++) {
    if (!isdigit(*p)) {
      if (error) *error = true;
      return 0;
    }
  }

  // Convertir usando strtoul (para detectar overflow)
  char *endptr;
  unsigned long val = strtoul(num, &endptr, 10);

  // Comprobaciones de error
  if (*endptr != '\0' || val > 65535UL) {
    if (error) *error = true;
    return 0;
  }

  // Conversión segura
  return (uint16_t)val;
}


inline uint8_t codeFromCommandIdAndDevId(uint8_t id, uint8_t dev_id){
  return (id<<4)|dev_id;
}

Message CLI() {
  if (SerialUSB.available() <= 0) return empty_message;

  String message = SerialUSB.readStringUntil('\n');
  message.trim();
  if (message.length() == 0) return empty_message;

  SerialUSB.print(F("leido: "));
  SerialUSB.println(message);

  char buf[32];
  message.toCharArray(buf, sizeof(buf));
  char *argv[MAX_ARGS];
  uint8_t argc = 0;
  char *token = strtok(buf, " ");
  while (token && argc < MAX_ARGS) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  return processCommand(argc, argv);
}

inline void argument_error(){SerialUSB.println(F("malos argumentos. Use 'help'."));}
Message processCommand(uint8_t argc, char *argv[]) {
  Message msg = {0, 0}; // valor por defecto (help, vacío, error)

  if (argc == 0) return msg;

  String cmd = argv[0];

  // ============================= HELP =============================
  if (cmd == HELP) {
    SerialUSB.println(F("Comandos disponibles:"));
    SerialUSB.println(F("us <HEX:0x--> one-shot"));
    SerialUSB.println(F("us <HEX:0x--> on <INT: max=65535>"));
    SerialUSB.println(F("us <HEX:0x--> off"));
    SerialUSB.println(F("us <HEX:0x--> unit <inc|cm|ms>"));
    SerialUSB.println(F("us <HEX:0x--> delay <INT: max=65535>"));
    SerialUSB.println(F("us <HEX:0x--> status"));
    return msg;
  }

  // ============================= US ===============================
  if (cmd != "us") {
    SerialUSB.print(F("Comando desconocido: "));
    SerialUSB.println(cmd);
    return msg;
  }

  if (argc == 1) {
    SerialUSB.println(F("Sensores disponibles:"));
    SerialUSB.println(F(" - 0xE0"));
    SerialUSB.println(F(" - 0xF2"));
    msg.code = (9 << 4); // id=9 (us)
    return msg;
  }

  if (argc < 3) {
    argument_error();
    return msg;
  }

  // --- Validar dirección ---
  if (!isHex(argv[1])) {
    SerialUSB.println(F("Error: dirección debe ser hexadecimal (ej: 0xE0)"));
    return msg;
  }

  uint8_t devHex = parseHex(argv[1]);
  uint8_t devId = hexToDeviceId(devHex);
  uint16_t param = 0;
  uint8_t code = 0;

  // --- Buscar tipo de comando ---
  Argument command_type = {"", 0};
  for (uint8_t i = 1; i < COM_NUM; i++) {
    if (strcmp(argv[2], commands[i].name) == 0) {
      command_type = commands[i];
      break;
    }
  }

  // ===================== Resolver comando =====================
  code = codeFromCommandIdAndDevId(command_type.id, devId);
  bool error = false;
  switch (command_type.id) {
    case 1: // one-shot
    case 3: // off
    case 8: // status
      if (argc != 3) {argument_error(); error = true;}
      break;

    case 2: // on <period>
      if (argc == 4 && isNumber(argv[3])) {
        param = parseInt(argv[3]);
        if (error){ SerialUSB.println(F("no uso un numero entero positivo menor a 65535."));}
      } else {argument_error(); error = true;}
      break;

    case 4: // unit <inc|cm|ms>
      if (argc == 4) {
        if (strcmp(argv[3], "cm") == 0) code = codeFromCommandIdAndDevId(5, devId);
        else if (strcmp(argv[3], "ms") == 0) code = codeFromCommandIdAndDevId(6, devId);
        else if(strcmp(argv[3], "inc") != 0) {
          SerialUSB.println(F("Unidad inválida (use inc|cm|ms)"));
          error = true;
        }
      } else {argument_error(); error = true;}
      break;

    case 7: // delay <ms>
      if (argc == 4 && isNumber(argv[3])) {
        param = parseInt(argv[3]);
        if (error){ SerialUSB.println(F("no uso un numero entero positivo menor a 65535."));}
      }  else {argument_error(); error = true;}
      break;

    default:
      SerialUSB.println(F("Comando US inválido o incompleto."));
      argument_error(); error = true;
      break;
  }
  if (error){return msg;}
  msg.code = code;
  msg.param = param;
  return msg;
}





// ************************************************************************************************************************
// supervisor
// ************************************************************************************************************************
/*
  test:
  help
  su 0xE0 one-shot
  su 0xF2 on 1000
  su estatus
  # espera unos segundos...
  su 0xE0 delay 200
  su 0xF2 off
*/


// ============================================================
// Sensor struct
// ============================================================

typedef struct {
  bool cycle = false;          // modo periódico activo
  uint16_t period = 0;         // periodo ms
  uint8_t addr = 0xE0;      // dirección I2C base
  uint8_t unit = 4;            // 4=inc,5=cm,6=ms
  thread_t *thread = nullptr;  // referencia al hilo
} Sensor;

// ============================================================
// Configuración de sensores
// ============================================================
#define SENSOR_COUNT 2
Sensor sensors[SENSOR_COUNT] = {
  {false, 0, 0xE0, 5, nullptr}, //alberto
  {false, 0, 0xF2, 5, nullptr}  //santiago
};

// ============================================================
// Función de hilo periódico por sensor
// ============================================================

static THD_FUNCTION(sensorPeriodic, arg) {
  Sensor *s = (Sensor *)arg;

  while (!chThdShouldTerminateX()) {
    if (s->cycle && s->period > 0) {
      // Generar code a partir de unit y addr
      uint8_t code = (s->unit << 4) | hexToDevId(s->addr);
      sendCode(Serial1, code);

      Message resp;
      if (receiveMessage(Serial1, &resp, 80)) {
        if (verifyReply(code, resp)) {
          SerialUSB.print(F("[AUTO] Sensor "));
          SerialUSB.print(s->addr, HEX);
          SerialUSB.print(F(" → "));
          SerialUSB.print(resp.param);
          if (s->unit == 4) SerialUSB.println(F(" in"));
          else if (s->unit == 5) SerialUSB.println(F(" cm"));
          else SerialUSB.println(F(" us"));
        }
      }
      chThdSleepMilliseconds(s->period);
    } else {
      chThdSleepMilliseconds(100);
    }
  }
}

// ============================================================
// Control del hilo periódico por sensor
// ============================================================

void startPeriodic(Sensor *s, uint16_t period) {
  s->period = period;
  s->cycle = true;

  // Si ya tenía hilo, lo reinicia
  if (s->thread != nullptr) {
    chThdTerminate(s->thread);
    chThdWait(s->thread);
    s->thread = nullptr;
  }

  s->thread = chThdCreateFromHeap(NULL, THD_WORKING_AREA_SIZE(256),
                                  NORMALPRIO + 1, sensorPeriodic, s);

  SerialUSB.print(F("[INFO] Periodo activado para 0x"));
  SerialUSB.print(s->addr, HEX);
  SerialUSB.print(F(" cada "));
  SerialUSB.print(period);
  SerialUSB.println(F(" ms."));
}

void stopPeriodic(Sensor *s) {
  if (s->thread != nullptr) {
    chThdTerminate(s->thread);
    chThdWait(s->thread);
    s->thread = nullptr;
  }
  s->cycle = false;
  s->period = 0;

  SerialUSB.print(F("[INFO] Periodo detenido para 0x"));
  SerialUSB.println(s->addr, HEX);
}

// ============================================================
// Buscar sensor por dirección
// ============================================================

Sensor* findSensorByAddr(uint8_t addr) {
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (sensors[i].addr == addr) return &sensors[i];
  }
  return nullptr;
}

// ============================================================
// setup()
// ============================================================

void setup() {
  SerialUSB.begin(9600);
  Serial1.begin(9600);
  while (!SerialUSB) {;}

  SerialUSB.println(F("Supervisor iniciado."));
  SerialUSB.println(F("Listo para comandos CLI."));
  chBegin([](){}); // Inicializar ChibiOS
}

// ============================================================
// loop()
// ============================================================

void loop() {
  Message cmd = CLI();

  if (cmd.code == 0) {
    delay(10);
    return;
  }

  uint8_t cmdId = cmd.code >> 4;
  uint8_t devAddr = codeToDev(cmd.code);
  Sensor *s = findSensorByAddr(devAddr);

  if (sensor == nullptr) {
    SerialUSB.print(F("[WARN] Sensor no reconocido (addr=0x"));
    SerialUSB.print(devAddr, HEX);
    SerialUSB.println(F(")."));
    return;
  }

  switch (cmdId) {
    case 1: // one-shot
      sendMessage(Serial1, cmd);
      break;

    case 2: // on <period>
      startPeriodic(s, cmd.param);
      break;

    case 3: // off
      stopPeriodic(s);
      break;

    case 4: case 5: case 6: // cambiar unidad
      s->unit = cmdId;
      SerialUSB.print(F("[INFO] Unidad cambiada para 0x"));
      SerialUSB.print(s->addr, HEX);
      SerialUSB.print(F(" → "));
      if (cmdId == 4) SerialUSB.println(F("inches"));
      else if (cmdId == 5) SerialUSB.println(F("cm"));
      else SerialUSB.println(F("microsegundos"));
      break;

    case 7: // delay
      sendMessage(Serial1, cmd);
      break;

    case 8: // status
      SerialUSB.print(F("[STATUS] Sensor 0x"));
      SerialUSB.print(s->addr, HEX);
      SerialUSB.print(F(" | unit="));
      SerialUSB.print(UNITS[s->unit - 4]);
      SerialUSB.print(F(" | period="));
      SerialUSB.print(s->period);
      SerialUSB.print(F(" | cycle="));
      SerialUSB.println(s->cycle ? "ON" : "OFF");
      break;

    default:
      SerialUSB.println("not a command");
      //sendMessage(Serial1, cmd);
      break;
  }

  delay(5);
}