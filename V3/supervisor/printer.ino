#include "printer.h"
#include <Arduino.h>


const Message HELP_MESSAGE = {
  .flags  = 0,
  .cmd    = CMD_HELP,
  .dir    = 0,
  .status = 0,
  .plen   = 0,
};

static const char* error_to_string(ErrorCode err)
{
  switch (err) {
    case ERR_OK:           return "OK";
    case ERR_BAD_CMD:      return "BAD_CMD";
    case ERR_BAD_DIR:      return "BAD_DIR";
    case ERR_BAD_PARAM:    return "BAD_PARAM";
    case ERR_I2C_NACK:     return "I2C_NACK";
    case ERR_I2C_TIMEOUT:  return "I2C_TIMEOUT";
    case ERR_SENSOR_BUSY:  return "SENSOR_BUSY";
    case ERR_INTERNAL:     return "INTERNAL";
    case ERR_TIMEOUT:      return "TIMEOUT";
    default:               return "UNKNOWN";
  }
}

static const char* unit_to_string(Unit u)
{
  switch (u) {
    case UNIT_INCHES: return "inches";
    case UNIT_CM:     return "centimeter";
    case UNIT_USEC:   return "micro seconds";
    default:          return "?";
  }
}

static void print_prefix(const Message *msg)
{
  if (msg->flags & MSG_FLAG_EVENT) SerialUSB.print("[PERIODIC] ");
  else                            SerialUSB.print("[RESPONSE] ");

  SerialUSB.print("us ");
  if (msg->dir) SerialUSB.print(msg->dir, HEX);
  else          SerialUSB.print("--");
  SerialUSB.print(": ");
}

void printer_print_help(void)
{
  SerialUSB.println();
  SerialUSB.println("Commands:");
  SerialUSB.println("  help");
  SerialUSB.println("  us");
  SerialUSB.println("  us <0xSRF02> one-shot");
  SerialUSB.println("  us <0xSRF02> on <period_ms>");
  SerialUSB.println("  us <0xSRF02> off");
  SerialUSB.println("  us <0xSRF02> unit {inc|cm|ms}");
  SerialUSB.println("  us <0xSRF02> delay <ms>");
  SerialUSB.println("  us <0xSRF02> status");
}

bool decode_status_payload(const Message *msg, SensorStatus *out)
{
  if (msg->cmd != CMD_GET_STATUS) return false;
  if (msg->plen != STATUS_PAYLOAD_LEN) return false;

  out->dir = msg->payload[0];
  out->unit = (Unit)msg->payload[1];

  out->min_delay_ms =
    ((uint16_t)msg->payload[2] << 8) | (uint16_t)msg->payload[3];

  out->periodic_enabled = msg->payload[4] ? true : false;

  out->period_ms =
    ((uint16_t)msg->payload[5] << 8) | (uint16_t)msg->payload[6];

  return true;
}

void printer_print_message(const Message *msg)
{
  if (msg->cmd == CMD_HELP) {
    printer_print_help();
    return;
  }

  print_prefix(msg);

  // errores primero
  if (msg->status != ERR_OK) {
    SerialUSB.print("ERROR ");
    SerialUSB.println(error_to_string((ErrorCode)msg->status));
    return;
  }

  switch ((CommandId)msg->cmd) {

    case CMD_RANGE_ONCE:
      if (msg->plen == 2) {
        uint16_t v = ((uint16_t)msg->payload[0] << 8) | msg->payload[1];
        SerialUSB.print("range = ");
        SerialUSB.println(v);
      } else {
        SerialUSB.println("range OK");
      }
      break;

    case CMD_STREAM_ON:
      SerialUSB.println("stream ON");
      break;

    case CMD_STREAM_OFF:
      SerialUSB.println("stream OFF");
      break;

    case CMD_SET_UNIT:
      if (msg->plen == 1) {
        SerialUSB.print("unit = ");
        SerialUSB.println(unit_to_string((Unit)msg->payload[0]));
      } else {
        SerialUSB.println("unit set");
      }
      break;

    case CMD_SET_DELAY:
      if (msg->plen == 2) {
        uint16_t d = ((uint16_t)msg->payload[0] << 8) | msg->payload[1];
        SerialUSB.print("delay = ");
        SerialUSB.print(d);
        SerialUSB.println(" ms");
      } else {
        SerialUSB.println("delay set");
      }
      break;

    case CMD_GET_STATUS: {
      SensorStatus st;
      if (!decode_status_payload(msg, &st)) {
        SerialUSB.println("invalid status payload");
        return;
      }

      SerialUSB.print("dir=0x"); SerialUSB.print(st.dir, HEX);
      SerialUSB.print(" unit="); SerialUSB.print(unit_to_string(st.unit));
      SerialUSB.print(" min_delay="); SerialUSB.print(st.min_delay_ms); SerialUSB.print("ms");
      SerialUSB.print(" periodic="); SerialUSB.print(st.periodic_enabled ? "ON" : "OFF");
      SerialUSB.print(" period="); SerialUSB.print(st.period_ms); SerialUSB.println("ms");
      break;
    }

    case CMD_LIST_SENSORS:
      SerialUSB.print("sensors:");
      for (uint8_t i = 0; i < msg->plen; ++i) {
        SerialUSB.print(" 0x");
        SerialUSB.print(msg->payload[i], HEX);
      }
      SerialUSB.println();
      break;

    default:
      SerialUSB.println("OK");
      break;
  }
}
