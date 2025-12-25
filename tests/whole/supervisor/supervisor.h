typedef struct {
  bool cycle;
  uint16_t period;
  uint8_t addr;
  uint8_t unit;
  thread_t *thread;
} Sensor;

#define SENSOR_COUNT 2
Sensor sensors[SENSOR_COUNT];

void startPeriodic(Sensor *s, uint16_t period);
void stopPeriodic(Sensor *s);
Sensor* findSensorByAddr(uint8_t addr);

/*
  test:
  help
  us 0xE2 status //[STATUS] Sensor 0xE0 | unit=cm | period=0 | delay=70 | cycle=OFF
  us 0xE2 one-shot
  us 0xE2 on 1000
  us 0xE2 delay 200
  us 0xE2 status
  us 0xE2 off
  us 0xE2 unit ms
  us 0xE2 one-shot
*/