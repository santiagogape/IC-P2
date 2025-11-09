#include <Wire.h>
#include <ChRt.h>
#include "message_protocol.h"
#include "rcf02_i2c.h"
#include "sensor.h"
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
MUTEX_DECL(i2c_lock);

// ============================================================
//   Hilo receptor (único lector de Serial1)
// ============================================================
static THD_WORKING_AREA(waRx, 384);
static THD_FUNCTION(rxThread, arg) {
  (void)arg;
  Message in;
  while (!chThdShouldTerminateX()) {
    if (receiveMessage(Serial1, &in, 100)) {
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
      SerialUSB.print("RECEIVED: ");SerialUSB.print(cmdId);SerialUSB.print(":");SerialUSB.print(cmd.code & DEV_MASC, HEX);SerialUSB.print(":");SerialUSB.println(cmd.param);
      switch (cmdId){
        case 1:
          // Disparo de medida (unidad desde cmdId si 4..6, si no: cm)
          chMtxLock(&i2c_lock);
          SerialUSB.print("executing shot on ");
          SerialUSB.print(self->addr,HEX);
          SerialUSB.print(":");
          SerialUSB.println(self->i2c_addr,HEX);
          srf02_oneShot(self->i2c_addr, cmd.param);
          SerialUSB.println("waiting");
          chThdSleepMilliseconds(100);
          cmd.param = srf02_read_result(self->i2c_addr);
          if (cmd.param == 0){
            mask_error(WIRE_ERROR,cmd.code);
          }
          SerialUSB.print("sending reply: code=");
          SerialUSB.print(cmd.code, HEX);
          SerialUSB.print(" param=");
          SerialUSB.println(cmd.param);
          chMtxUnlock(&i2c_lock);
          sendMessage(Serial1, cmd);
          SerialUSB.println("reply sent");

          chThdSleepMilliseconds(self->delay_ms);
          break;
        case 7: //delay
          if (cmd.param < SRF02_RANGING_DELAY){
            cmd.code = mask_error(UNDER_MIN_DELAY,cmd.code); 
            cmd.param = SRF02_RANGING_DELAY;
          } else {
            uint16_t old = self->delay_ms;
            self->delay_ms = cmd.param;
            cmd.param = old;
          }
          sendMessage(Serial1,cmd);
          break;
        case 8: //status (reads delay)
          cmd.param = self->delay_ms;
          sendMessage(Serial1, cmd);
          break;
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
  SerialUSB.begin(9600);
  Serial1.begin(9600);
  Wire.begin();
  chBegin(chSetup);
}

void loop() {
  // no se usa (ChibiOS controla la ejecución)
}