/*
  - help: se muestra información acerca de los comandos aceptados y la operativa correcta del sistema. 
  - us <srf02> {one-shot | on <period_ms> | off}: 
    se comanda un único disparo del sensor de ultrasonidos (one-shot), 
    o bien se establece que se dispare con un periodo específico de manera continuada (on <period_ms>),
    o que se cese de disparar el sensor de manera periódica si lo estuviera (off). 
    En el comando debe identificarse qué sensor SRF02 quiere dispararse (<srf02>). 
  - us <srf02> unit {inc | cm | ms}: este comando permite modificar la unidad de medida devuelta
    por un sensor SRF02 específico (<srf02>). 
  - us <srf02> delay <ms>: este comando establece el tiempo de espera o retardo mínimo que debe 
    haber entre dos disparos consecutivos del sensor (<srf02>). 
  - us <srf02> status: este comando debe proporcionar información de configuración del sensor, 
    en concreto, su dirección I2C, retardo mínimo entre disparos, su configuración de unidades de medida, 
    y su estado de disparo periódico, en el caso de que éste esté activado o no. 
  - us: este comando debe proporcionar la relación de sensores de ultrasonidos disponibles en el dispositivo sensor. 

*/
/*

byte = 0000 

comando: 10 -> 0-9 -> 4 bits
device: E0,E2,E4,E6,E8,EA,EC,EE,F0,F2,F4,F6,F8,FA,FC,FE -> 0-F  -> 4 bits
param -> (0:inc|1:cm|2:ms) -> 2 bits | (0-65535) 16 bits

message: byte(comando,device):byte(param)
mascara:

examples:
0:help
1:us 0xFF one-shot
2:us 0xFF on 65535
3:us 0xFF off
4:us 0xFF unit inc
5:us 0xFF unit cm
6:us 0xFF unit ms
7:us 0xFF delay 1000
8:us 0xFF status
9:us
*/

#define COM_MASC 0xF0
#define DEV_MASC 0x0F
#define DEV_MIN 0xE0
#define UNIT_COM 4
#define MAX_ARGS 4

uint8_t DeviceIdToHex(uint8_t offset){
  return DEV_MIN + (offset<<1);
}

uint8_t hexToDeviceId(uint8_t hex){
  return (hex - DEV_MIN)>>1;
}


/*
  char name[8];
  uint8_t id; 1-9
*/
typedef struct {
  char name[9];
  uint8_t id;
} Argument;

#define HELP "help"
#define COM_NUM 7
const Argument commands[COM_NUM] = {
  {"us", 9}, {"one-shot",1}, {"on",2}, {"off",3}, {"unit",4}, {"delay",7}, {"status",8}
};
const Argument empty_arg = {"",0};

char UNITS[3][4] = {"inc","cm","ms"}; //inches, centimeters, miliseconds

uint16_t commadOffset(uint8_t id){
  return id<<4;
}

uint16_t unitToCommandId(const char *unit){
  for (uint8_t i = 0; i<3;i++){
    if (strcmp(unit, UNITS[i]) == 0) return UNIT_COM + i;
  }
  return 0;
}

void setup() {
  // put your setup code here, to run once:
  SerialUSB.begin(9600);
  while (!SerialUSB){;}
  SerialUSB.println("ready");
}

//blocking
void CLI(){
  if (SerialUSB.available() > 0 ){
    String message = SerialUSB.readStringUntil('\n');
    message.trim();
    SerialUSB.print("leido: "); SerialUSB.println(message);

    parseCommand(message);
  }
}

void parseCommand(String message) {
  char buf[18]; // el comando mas largo es CYCLE,65535. 18 basta para esto
  message.toCharArray(buf, sizeof(buf));
  char *token = strtok(buf, " ");
  uint8_t argc = 0;
  char *argv[8];

  while (token != NULL && argc < MAX_ARGS) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  // Llamamos al procesador con los tokens
  processCommand(argc, argv);
}


void processCommand(uint8_t argc, char *argv[]) {
  if (argc == 0) return;

  String cmd = argv[0];
  Argument command_type = empty_arg;
  uint8_t device;
  uint16_t num = 0;
  if (cmd == HELP) {
    SerialUSB.println(F("Comandos disponibles:"));
    SerialUSB.println(F("us <HEX:0x--> one-shot"));
    SerialUSB.println(F("us <HEX:0x--> on <INT:max 65535>"));
    SerialUSB.println(F("us <HEX:0x--> off"));
    SerialUSB.println(F("us <HEX:0x--> unit <LITERAL:inc|cm|ms>"));
    SerialUSB.println(F("us <HEX:0x--> delay <INT:max 65535>"));
    SerialUSB.println(F("us <HEX:0x--> status"));
    return;
  }
  else if (cmd == "us") {
    
    if (argc == 1) {
      SerialUSB.println(F("us: Comando sin implementar."));
      command_type = commands[0];
    } else if (argc == 2) {
      SerialUSB.println(F("Comando 'us' no necesita HEX."));
      return;
    } else if (argc > 4) {
      SerialUSB.println(F("Mal comando, muchos argumentos."));
      return;
    }

    // ---- Argumento 1: dirección hexadecimal ----
    if (!isHex(argv[1])) {
      SerialUSB.println(F("Error: direccion debe ser hexadecimal (ej: 0xFF)"));
      return;
    }
    SerialUSB.println(argv[1]);
    SerialUSB.println(parseHex(argv[1]), HEX);
    device = hexToDeviceId(parseHex(argv[1]));
    SerialUSB.println(device);
    

    // ---- Comandos secundarios ----
    for (uint8_t i =1; i<COM_NUM; i++){
      if (strcmp(argv[2], commands[i].name) == 0) {command_type = commands[i];}
    }


    if (command_type.id == 1) {
      SerialUSB.print(F("One-shot")); SerialUSB.println(device, HEX);
    }
    else if (command_type.id == 2 && argc == 4 && isNumber(argv[3])) {
      SerialUSB.print(F("cycle ")); SerialUSB.print(device, HEX);
      SerialUSB.print(F(" on ")); SerialUSB.println(argv[3]);
      num = parseInt(argv[3]);
    }
    else if (command_type.id == 3) {
      SerialUSB.print(F("Apagar ")); SerialUSB.println(device, HEX);
    }
    else if (command_type.id == 4 && argc == 4) {
      if (unitToCommandId(argv[3]) != 0) {
        SerialUSB.print(F("Unit ")); SerialUSB.println(argv[3]);
      } else {
        SerialUSB.println(F("Unidad invalida (use inc|cm|ms)"));
      }
    }
    else if (command_type.id == 7 && argc == 4 && isNumber(argv[3])) {
      SerialUSB.print(F("Delay = ")); SerialUSB.println(argv[3]);
      num = parseInt(argv[3]);
    }
    else if (command_type.id == 8) {
      SerialUSB.print(F("Status ")); SerialUSB.println(device, HEX);
    }
    else {
      SerialUSB.println(F("Comando US invalido o incompleto."));
    }
  }
  else {
    SerialUSB.print(F("Comando desconocido: "));
    SerialUSB.println(cmd);
    return;
  }

  if (command_type.id >0){
    SerialUSB.println(command_type.id<<4, BIN);
    SerialUSB.println(device, HEX);
    SerialUSB.print(commadOffset(command_type.id), HEX);SerialUSB.print(" ");SerialUSB.println(device, HEX);
    uint8_t code = commadOffset(command_type.id) + device;
    SerialUSB.print(F("code for command+device: "));SerialUSB.print(code);SerialUSB.print(" HEX: ");SerialUSB.println(code, HEX);
    SerialUSB.print(F("from id: "));SerialUSB.print(code>>4);
    SerialUSB.print(F(" and dev: "));SerialUSB.println(DeviceIdToHex( code & DEV_MASC) , HEX);
    SerialUSB.println(code & DEV_MASC, BIN);
    uint16_t param = 0;
    if (command_type.id == 2 || command_type.id == 7){
      param = num;
      SerialUSB.println(param);SerialUSB.println(param,HEX);
    }

  }

}

bool isHex(const char *s) {
  return (strlen(s) == 4 && s[0] == '0' && s[1] == 'x');
}

bool isNumber(const char *s) {
  for (int i = 0; s[i]; i++) {
    if (!isdigit(s[i])) return false;
  }
  return true;
}

uint8_t parseHex(char *hex){
  return (uint8_t) strtol(hex, NULL, 16);
}

uint16_t parseInt(char *num){
  return (uint16_t) strtol(num, NULL, 10);
}

void loop() {
  // put your main code here, to run repeatedly:
  //readLine();
  CLI();
}
