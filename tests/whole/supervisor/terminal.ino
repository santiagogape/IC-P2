

// ************************************************************************************************************************
// terminal
// ************************************************************************************************************************

bool isHex(const char *s) { return (strlen(s) == 4 && s[0] == '0' && s[1] == 'x'); }
bool isNumber(const char *s) { for (int i = 0; s[i]; i++) if (!isdigit(s[i])) return false; return true; }

uint8_t parseHex(char *hex) { return (uint8_t)strtol(hex, NULL, 16); }

uint16_t parseInt(const char *num, bool *error) {
  for (const char *p = num; *p; p++) if (!isdigit(*p)) { if (error) *error = true; return 0; }
  char *endptr; unsigned long val = strtoul(num, &endptr, 10);
  if (*endptr != '\0' || val > 65535UL) { if (error) *error = true; return 0; }
  return (uint16_t)val;
}

inline uint8_t codeFromCommandIdAndDevId(uint8_t id, uint8_t dev_id) {
  return (id<<4)|dev_id;
}

// ============================================================
// CLI parser
// ============================================================
inline void argument_error(){ SerialUSB.println(F("malos argumentos. Use 'help'.")); }

Message processCommand(uint8_t argc, char *argv[]) {
  Message msg = {0,0};
  if (argc == 0) return msg;

  String cmd = argv[0];
  if (cmd == HELP) {
    SerialUSB.println(F("Comandos disponibles:"));
    SerialUSB.println(F("us <HEX:0x--> one-shot"));
    SerialUSB.println(F("us <HEX:0x--> on <INT: max=65535>"));
    SerialUSB.println(F("us <HEX:0x--> off"));
    SerialUSB.println(F("us <HEX:0x--> unit <LITERAL:[inc,cm,ms]>"));
    SerialUSB.println(F("us <HEX:0x--> delay <INT: max=65535>"));
    SerialUSB.println(F("us <HEX:0x--> status"));
    return msg;
  }
  if (cmd != "us") {
    SerialUSB.print(F("Comando desconocido: ")); SerialUSB.println(cmd); return msg;
  }
  if (argc == 1) {
    SerialUSB.println(F("Sensores: 0xE0, 0xF2"));
    msg.code = (9<<4); return msg;
  }

  if (argc < 3) { argument_error(); return msg; }

  if (!isHex(argv[1])) { SerialUSB.println(F("Error: dirección no válida.")); return msg; }

  uint8_t devHex = parseHex(argv[1]);
  uint8_t devId = hexToDevId(devHex);
  uint16_t param = 0;
  uint8_t code = 0;

  Argument command_type = {"",0};
  for (uint8_t i = 1; i < COM_NUM; i++)
    if (strcmp(argv[2], commands[i].name) == 0) command_type = commands[i];

  bool err = false;
  switch (command_type.id) {
    case 1: case 3: case 8:
      break;
    case 2:
      if (argc == 4) param = parseInt(argv[3], &err);
      break;
    case 4:
      if (argc == 4) {
        if (strcmp(argv[3], "cm") == 0) code = codeFromCommandIdAndDevId(5, devId);
        else if (strcmp(argv[3], "ms") == 0) code = codeFromCommandIdAndDevId(6, devId);
        else if (strcmp(argv[3], "inc") != 0) err = true;
      }
      break;
    case 7:
      if (argc == 4) param = parseInt(argv[3], &err);
      break;
    default: err = true; break;
  }
  if (err) return msg;
  msg.code = code ? code : codeFromCommandIdAndDevId(command_type.id, devId);
  msg.param = param;
  return msg;
}

Message CLI() {
  if (SerialUSB.available() <= 0) return empty_message;
  String message = SerialUSB.readStringUntil('\n');
  message.trim(); if (message.length() == 0) return empty_message;

  char buf[32]; message.toCharArray(buf, sizeof(buf));
  char *argv[MAX_ARGS]; uint8_t argc = 0;
  char *token = strtok(buf, " ");
  while (token && argc < MAX_ARGS) argv[argc++] = token, token = strtok(NULL, " ");
  return processCommand(argc, argv);
}