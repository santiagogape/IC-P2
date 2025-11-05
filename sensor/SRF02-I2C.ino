/* 
 * srf02_example.ino
 * Example showing how to use the Devantech SRF02 ultrasonic sensor
 * in I2C mode. More info about this LCD display in
 *   http://www.robot-electronics.co.uk/htm/srf02techI2C.htm
 *
 * author: Antonio C. Domínguez Brito <adominguez@iusiani.ulpgc.es>
 */
 
/* 
 * Arduino MKR WAN 1310 I2C pins
 *   Pin 11 -> SDA (I2C data line)
 *   Pin 12 -> SCL (I2C clock line)
 * sensor -> dir 226, hex F2
 * minimo 66ms entre disparo del sensor
 */
 
#include <Wire.h> // Arduino's I2C library


#define SRF02_I2C_ADDRESS byte((0xF2)>>1) 
// se desplaza a la derecha para que funcione, ya que luego la libreria es la que desplaza para el bit de r/w
#define SRF02_I2C_INIT_DELAY 100 // in milliseconds
#define SRF02_RANGING_DELAY 70 // milliseconds

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define SOFTWARE_REVISION byte(0x00)
#define RANGE_HIGH_BYTE byte(2)
#define RANGE_LOW_BYTE byte(3)
#define AUTOTUNE_MINIMUM_HIGH_BYTE byte(4)
#define AUTOTUNE_MINIMUM_LOW_BYTE byte(5)

// SRF02's command codes
#define REAL_RANGING_MODE_INCHES    byte(80)
#define REAL_RANGING_MODE_CMS       byte(81)
#define REAL_RANGING_MODE_USECS     byte(82)
#define FAKE_RANGING_MODE_INCHES    byte(86)
#define FAKE_RANGING_MODE_CMS       byte(87)
#define FAKE_RANGING_MODE_USECS     byte(88)
#define TRANSMIT_8CYCLE_40KHZ_BURST byte(92)
#define FORCE_AUTOTUNE_RESTART      byte(96)
#define ADDRESS_CHANGE_1ST_SEQUENCE byte(160)
#define ADDRESS_CHANGE_3RD_SEQUENCE byte(165)
#define ADDRESS_CHANGE_2ND_SEQUENCE byte(170)

typedef struct {
  bool cycle = false;
  unsigned long last_time = 0;
  uint16_t period = 0;
  uint8_t addr = DEV_MIN>>1;
  uint8_t unit = REAL_RANGING_MODE_INCHES; // 0x50 inc, 0x51 cm, 0x52 ms
} Sensor;

volatile Sensor sensor_1;
volatile Sensor sensor_2;
sensor_2.addr = 0xF2>>1;

inline void write_command(byte address,byte command)
{ 
  Wire.beginTransmission(address);
  Wire.write(COMMAND_REGISTER); 
  Wire.write(command); 
  Wire.endTransmission();
}

byte read_register(byte address,byte the_register)
{
  Wire.beginTransmission(address);
  Wire.write(the_register);
  Wire.endTransmission();
  
  // getting sure the SRF02 is not busy
  Wire.requestFrom(address,byte(1));
  while(!Wire.available()) { /* do nothing */ }
  return Wire.read();
} 

volatile Sensor* select(uint8_t addr) {
  if (addr == sensor_1.addr)
    return (Sensor*)&sensor_1;
  else if (addr == sensor_2.addr)
    return (Sensor*)&sensor_2;
  else
    return NULL;  // no encontrado
}

uint16_t readDistance(uint8_t address) {
  uint8_t high = read_register(address, RANGE_HIGH_BYTE);
  uint8_t low  = read_register(address, RANGE_LOW_BYTE);
  return (high << 8) | low;
}

/**
uint8_t unit in {REAL_RANGING_MODE_INCHES, REAL_RANGING_MODE_CMS, REAL_RANGING_MODE_USECS}
*/
void oneShot(uint8_t address) {
  Sensor* sensor = select(address);
  if (sensor == NULL){ return;} //ERROR
  write_command(address, sensor.unit);
  delay(70); // tiempo de medición típico
}

void changeUnit(uint8_t address, uint8_t unit) {
  Sensor* sensor = select(address);
  if (sensor == NULL){ return;} //ERROR
  sensor.unit = unitIdToCommandMetric(unit);
}

void onPeriodic(uint8_t address, uint16_t period_ms) {
  Sensor* sensor = select(address);
  if (sensor == NULL){return;} //ERROR
  if (period_ms < 70){return;} //ERROR
  sensor.cycle = true;
  sensor.period = period_ms;
  sensor.last_time = millis();
  // launch thread or somthing like that....
}

void offPeriodic(uint8_t address){
  Sensor* sensor = select(address);
  if (sensor == NULL){} //ERROR
  sensor.cycle = false;
  sensor.period = 0;
  sensor.last_time = 0;
  // delete thread or somthing like that....
}

void delayShot(uint8_t address, uint16_t delay_ms) {
  Sensor* sensor = select(address);
  if (sensor == NULL){return;} //ERROR
  delay(delay_ms);
  oneShot(address);
}
 
// the setup routine runs once when you press reset:
void setup() 
{
  SerialUSB.begin(9600);
  while (!SerialUSB){;}
  
  SerialUSB.println("initializing Wire interface ...");
  Wire.begin();
  delay(SRF02_I2C_INIT_DELAY);  
  SerialUSB.println("initializing Wire interface... ...");
  byte software_revision=read_register(SRF02_I2C_ADDRESS,SOFTWARE_REVISION);
  SerialUSB.print("SFR02 ultrasonic range finder in address 0x");
  SerialUSB.print(SRF02_I2C_ADDRESS,HEX); SerialUSB.print("(0x");
  SerialUSB.print(software_revision,HEX); SerialUSB.println(")");
}

// the loop routine runs over and over again forever:
void loop() 
{
  SerialUSB.print("ranging ...");
  write_command(SRF02_I2C_ADDRESS,REAL_RANGING_MODE_CMS);
  delay(SRF02_RANGING_DELAY);
  
  byte high_byte_range=read_register(SRF02_I2C_ADDRESS,RANGE_HIGH_BYTE);
  byte low_byte_range=read_register(SRF02_I2C_ADDRESS,RANGE_LOW_BYTE);
  byte high_min=read_register(SRF02_I2C_ADDRESS,AUTOTUNE_MINIMUM_HIGH_BYTE);
  byte low_min=read_register(SRF02_I2C_ADDRESS,AUTOTUNE_MINIMUM_LOW_BYTE);
  
  SerialUSB.print(int((high_byte_range<<8) | low_byte_range)); SerialUSB.print(" cms. (min=");
  SerialUSB.print(int((high_min<<8) | low_min)); SerialUSB.println(" cms.)");
  
  delay(1000);
}
