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
  us 0xE0 status
  us 0xE0 one-shot
  us 0xE0 on 1000
  us 0xE0 delay 200
  us 0xE0 status
  us 0xE0 off
  us 0xE0 unit cm
  us 0xE0 one-shot
*/