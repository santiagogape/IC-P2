#include <ChRt.h>
#include "message_protocol.h"
#include "terminal.h"
#include "supervisor.h"





// ************************************************************************************************************************
// supervisor logic
// ************************************************************************************************************************


static THD_WORKING_AREA(waSensorPeriodic, 256);

static THD_FUNCTION(sensorPeriodic, arg) {
  Sensor *s = (Sensor *)arg;
  while (!chThdShouldTerminateX()) {
    if (s->cycle && s->period > 0) {
      uint8_t code = (1<<4) | hexToDevId(s->addr);
      Message msg = {code, s->unit};
      sendMessage(Serial1, msg);
      Message resp = msg;
      if (receiveMessage(Serial1, &resp, 100) && verifyReply(code, resp)) {
        SerialUSB.print(F("[AUTO] 0x"));
        SerialUSB.print(s->addr, HEX);
        SerialUSB.print(F(" → "));
        SerialUSB.print(resp.param);
        SerialUSB.print(" ");
        SerialUSB.println(UNITS[s->unit-4]);
      }
      chThdSleepMilliseconds(s->period);
    } else chThdSleepMilliseconds(100);
  }
}

void startPeriodic(Sensor *s, uint16_t period) {
  s->period = period; s->cycle = true;
  if (s->thread) { chThdTerminate(s->thread); chThdWait(s->thread); }
  s->thread = chThdCreateStatic(waSensorPeriodic, sizeof(waSensorPeriodic), NORMALPRIO+1, sensorPeriodic, s);
}

void stopPeriodic(Sensor *s) {
  if (s->thread) { chThdTerminate(s->thread); chThdWait(s->thread); s->thread=nullptr; }
  s->cycle=false; s->period=0;
}

Sensor* findSensorByAddr(uint8_t addr) {
  for (uint8_t i=0;i<SENSOR_COUNT;i++) if (sensors[i].addr==addr) return &sensors[i];
  return nullptr;
}

void setup() {
  SerialUSB.begin(9600);
  Serial1.begin(9600);
  while (!SerialUSB) {}
  sensors[0] = {false,0,0xE0,5,nullptr};
  sensors[1] = {false,0,0xF2,5,nullptr};
  SerialUSB.println(F("Supervisor iniciado."));
  chBegin([](){});
}

void loop() {
  Message cmd = CLI();
  if (cmd.code == 0) {
    delay(10);
    return;
  }

  uint8_t cmdId = cmd.code >> 4;
  uint8_t devAddr = codeToDev(cmd.code);
  Sensor *s = findSensorByAddr(devAddr);
  if (!s) {
    SerialUSB.print(F("[WARN] Sensor no reconocido (addr=0x"));
    SerialUSB.print(devAddr, HEX);
    SerialUSB.println(F(")."));
    return;
  }

  switch (cmdId) {
    // ======================================================
    // One-shot: dispara un único pulso y espera respuesta
    // ======================================================
    case 1: {
      sendMessage(Serial1, cmd);
      Message reply = cmd;
      if (receiveMessage(Serial1, &reply, 200) && verifyReply(cmd.code, reply)) {
        SerialUSB.print(F("[ONE-SHOT] 0x"));
        SerialUSB.print(s->addr, HEX);
        SerialUSB.print(F(" → "));
        SerialUSB.print(reply.param);
        SerialUSB.print(" ");
        SerialUSB.println(UNITS[s->unit - 4]);
      }
      break;
    }

    // ======================================================
    // on <period>: arranca disparos periódicos
    // ======================================================
    case 2: {
      // Primero consultamos el delay mínimo actual del sensor
      uint8_t check = (8 << 4) | hexToDevId(devAddr);
      Message delayCheck = {check, 0};
      sendMessage(Serial1, delayCheck);

      Message reply = delayCheck;
      if (receiveMessage(Serial1, &reply, 200) && verifyReply(check, reply)) {
        if (cmd.param < 2 * reply.param) {
          SerialUSB.print(F("[WARN] Periodo menor a 2×delay no permitido. (addr=0x"));
          SerialUSB.print(devAddr, HEX);
          SerialUSB.print(F(") Delay actual="));
          SerialUSB.println(reply.param);
        } else {
          startPeriodic(s, cmd.param);
        }
      }
      break;
    }

    // ======================================================
    // off: detiene disparos periódicos
    // ======================================================
    case 3:
      stopPeriodic(s);
      break;

    // ======================================================
    // unit {inc|cm|ms}: cambia unidad de medición
    // ======================================================
    case 4:
    case 5:
    case 6: {
      uint8_t oldUnit = s->unit;
      s->unit = cmdId;
      SerialUSB.print(F("[INFO] Unidad cambiada para 0x"));
      SerialUSB.print(s->addr, HEX);
      SerialUSB.print(F(" de "));
      SerialUSB.print(UNITS[oldUnit - 4]);
      SerialUSB.print(F(" a "));
      SerialUSB.println(UNITS[s->unit - 4]);
      break;
    }

    // ======================================================
    // delay <ms>: cambia retardo mínimo del SRF02
    // ======================================================
    case 7: {
      sendMessage(Serial1, cmd);
      Message reply = cmd;
      if (receiveMessage(Serial1, &reply, 200) && verifyReply(cmd.code, reply)) {
        SerialUSB.print(F("[DELAY] Sensor 0x"));
        SerialUSB.print(s->addr, HEX);
        SerialUSB.print(F(" → nuevo delay="));
        SerialUSB.println(cmd.param);
      }
      break;
    }

    // ======================================================
    // status: consulta y muestra la configuración
    // ======================================================
    case 8: {
      sendMessage(Serial1, cmd);
      Message reply = cmd;
      if (receiveMessage(Serial1, &reply, 200) && verifyReply(cmd.code, reply)) {
        SerialUSB.print(F("[STATUS] Sensor 0x"));
        SerialUSB.print(s->addr, HEX);
        SerialUSB.print(F(" | unit="));
        SerialUSB.print(UNITS[s->unit - 4]);
        SerialUSB.print(F(" | period="));
        SerialUSB.print(s->period);
        SerialUSB.print(F(" | delay="));
        SerialUSB.print(reply.param);
        SerialUSB.print(F(" | cycle="));
        SerialUSB.println(s->cycle ? "ON" : "OFF");
      }
      break;
    }

    // ======================================================
    // Comando desconocido
    // ======================================================
    default:
      SerialUSB.println(F("[WARN] Comando no reconocido."));
      break;
  }

  delay(5);
}

