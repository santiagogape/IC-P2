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
  enviar un pulso
  enviar pulso periódico con T={ms}
  apagar pulso periódico (solo si hay uno activo)
  enviar un pulso despues de delay=ms
  cambiar metrica del resultado: inc,cm,ms
  obtener: direccion de dispositivo, retardo mínimo entre disparos, su configuración de unidades de medida

*/

/*
  char name[8];
  uint8_t id; 1-9
*/
typedef struct {
  char name[9];
  uint8_t id;
} Argument;

/*
  uint8_t code: 1 byte = command code + device id
  uint16_t param: 0-65535
*/
typedef struct {
  uint8_t code;
  uint16_t param;
} Message;

typedef struct { uint8_t id; uint8_t address; uint16_t param; } Command;

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
#define UNIT_MIN_HEX 0x50
#define MAX_ARGS 4
#define HELP "help"
#define COM_NUM 7

const Message empty_message = {0,0};
const Argument commands[COM_NUM] = {
  {"us", 9}, {"one-shot",1}, {"on",2}, {"off",3}, {"unit",4}, {"delay",7}, {"status",8}
};
const Argument empty_arg = {"",0};

const char UNITS[3][4] = {"inc","cm","ms"}; //inches, centimeters, miliseconds


inline uint8_t DeviceIdToHex(uint8_t offset){
  return DEV_MIN + (offset<<1);
}

inline uint8_t hexToDeviceId(uint8_t hex){
  return (hex - DEV_MIN)>>1;
}

inline uint8_t commadIdOffset(uint8_t id){
  return id<<4;
}

inline uint8_t codeToCommandId(uint8_t code){
  return code>>4;
}

inline uint8_t codeToHexAddr(uint8_t code){
  return DeviceIdToHex(code & DEV_MASC);
}

bool equalMessages(Message self, Message other){
  return self.code == other.code && self.param == other.param;
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

Command translateCommand(uint8_t code, uint16_t param){
  return Command{
    .id = codeToCommandId(code),
    .address = codeToHexAddr(code),
    .param = param
  };
}


uint8_t unitIdToCommandMetric(uint8_t id){
  return UNIT_MIN_HEX + (id-UNIT_COM);
}