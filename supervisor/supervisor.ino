/* ----------------------------------------------------------------------
 *  Ejemplo sending_example.ino 
 *    Este ejemplo muestra como utilizar el puerto serie uart (Serial1) 
 *    para comunicarse con otro dispositivo.
 *    
 *  Asignatura (GII-IC)
 * ---------------------------------------------------------------------- 
 */

/*-----------------------------------------------
*Bloque de declaracion de las variables globales
*------------------------------------------------
*/
constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t serial1_bauds=9600;

constexpr const uint32_t pseudo_period_ms=1000;

uint8_t counter=0;
uint8_t led_state=LOW;

#define COM_MASC 0xF0
#define DEV_MASC 0x0F
#define DEV_MIN 0xE0
#define UNIT_COM 4
#define MAX_ARGS 4

#define HELP "help"
#define COM_NUM 7

typedef struct {
  char name[9];
  uint8_t id;
} Argument;

typedef struct {
  uint8_t code;
  uint16_t param;
} Message;

const Argument commands[COM_NUM] = {
  {"us", 9}, {"one-shot",1}, {"on",2}, {"off",3}, {"unit",4}, {"delay",7}, {"status",8}
};
const Argument empty_arg = {"",0};

char UNITS[3][4] = {"inc","cm","ms"}; //inches, centimeters, miliseconds

const Message empty_message = {0,0};

/*-----------------------------------------------------------------------------------------------
*Bloque de funciones que nos ayudaran a comprobar los datos y a cambiar los valores de los mismos
*------------------------------------------------------------------------------------------------
*/
uint8_t DeviceIdToHex(uint8_t offset){
  return DEV_MIN + (offset<<1);
}

uint8_t hexToDeviceId(uint8_t hex){
  return (hex - DEV_MIN)>>1;
}

bool equalMessages(Message self, Message other){
  return self.code == other.code && self.param == other.param;
}

uint16_t commadOffset(uint8_t id){
  return id<<4;
}

uint16_t unitToCommandId(const char *unit){
  for (uint8_t i = 0; i<3;i++){
    if (strcmp(unit, UNITS[i]) == 0) {
      SerialUSB.print("leido: "); SerialUSB.print(unit);
      SerialUSB.print(" : "); SerialUSB.println(UNIT_COM + i);
       return UNIT_COM + i;}
  }
  return 0;
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

/*--------
*Setup
*---------
*/
void setup()
{
  // Configuración del LED incluido en placa
  // Inicialmente apagado
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
  
  // Inicialización del puerto para el serial monitor 
  Serial.begin(serial_monitor_bauds);
  while (!Serial);

  // Inicialización del puerto de comunicaciones con el otro dispositivo MKR 
  Serial1.begin(serial1_bauds);
}

/*-------------------------------------------------
*Bloque de funciones para el manejo de los comandos
*--------------------------------------------------
*/
Message CLI(){
  if (SerialUSB.available() > 0 ){
    String message = SerialUSB.readStringUntil('\n');
    message.trim();
    SerialUSB.print("leido: "); SerialUSB.println(message);

    return parseCommand(message);
  }

  return empty_message;
}

Message parseCommand(String message) {
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
  return processCommand(argc, argv);
}

Message processCommand(uint8_t argc, char *argv[]) {
  if (argc == 0){
    Serial.println("No se puede procesar un mensaje vacio.");
    return empty_message;
  } 

  String cmd = argv[0];
  Argument command_type = empty_arg;
  uint8_t device;
  uint16_t num = 0;
  if (cmd == HELP) {
    SerialUSB.println(F("Comandos disponibles:"));
    SerialUSB.println(F("us <HEX:0x--> one-shot"));
    SerialUSB.println(F("us <HEX:0x--> on <INT:0-65535>"));
    SerialUSB.println(F("us <HEX:0x--> off"));
    SerialUSB.println(F("us <HEX:0x--> unit <LITERAL:inc|cm|ms>"));
    SerialUSB.println(F("us <HEX:0x--> delay <INT:0-65535>"));
    SerialUSB.println(F("us <HEX:0x--> status"));
    return empty_message;
  }
  else if (cmd == "us") {
    
    if (argc == 1) {
      SerialUSB.println(F("us: Comando sin implementar."));
      command_type = commands[0];
    } else if (argc == 2) {
      SerialUSB.println(F("Comando 'us' no necesita HEX."));
      return empty_message;
    } else if (argc > 4) {
      SerialUSB.println(F("Mal comando, muchos argumentos."));
      return empty_message;
    }

    // ---- Argumento 1: dirección hexadecimal ----
    if (!isHex(argv[1])) {
      SerialUSB.println(F("Error: direccion debe ser hexadecimal (ej: 0xFF)"));
      return empty_message;
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
        command_type.id = unitToCommandId(argv[3]);
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
    return empty_message;
  }

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

  return (Message){code,param};

}


void loop()
{
  Message command = CLI();

  if (!equalMessages(command, empty_message)){
    SerialUSB.print("readed Message: code[");
    SerialUSB.print(command.code);
    SerialUSB.print("] param[");SerialUSB.print(command.param);
    SerialUSB.println("]");
  }
  /*
  Serial.println("******************* sending example *******************"); 

  
  Message command = CLI();
  if (!equalMessages(command, empty_message)){
    SerialUSB.print("readed Message: code[");SerialUSB.print(command.code);SerialUSB.print("] param[");SerialUSB.print(command.param);SerialUSB.println("]");
    Serial.print("--> sending: "); Serial.println(static_cast<int>(counter)); 
    Serial1.write(counter++);
  } else {
    Serial.print("--> sending: "); Serial.println(static_cast<int>(counter)); 
    Serial1.write(counter++);
  }

  uint32_t last_ms=millis();
  while(millis()-last_ms<pseudo_period_ms) 
  { 
    if(Serial1.available()>0) 
    {
      uint8_t data=Serial1.read();
      Serial.print("<-- received: "); Serial.println(static_cast<int>(data)); 
      break;
    }
  }

  if(millis()-last_ms<pseudo_period_ms) delay(pseudo_period_ms-(millis()-last_ms));
  else Serial.println("<-- received: TIMEOUT!!"); 

  Serial.println("*******************************************************"); 

  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;*/
}
