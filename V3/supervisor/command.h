#ifndef SUPERVISOR_COMMAND_H
#define SUPERVISOR_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"
#include "cli.h"

// Construye un Message (request) listo para enviar por Serial1
bool exec_build_message_from_console(const ConsoleCommand *cmd, Message *out_msg);

// Helpers genéricos (útiles también en otros módulos)
static inline uint8_t exec_encode_u16(uint16_t v, uint8_t *out_payload)
{
  out_payload[0] = (uint8_t)((v >> 8) & 0xFF);
  out_payload[1] = (uint8_t)(v & 0xFF);
  return 2;
}

static inline uint16_t exec_decode_u16(const uint8_t *payload)
{
  return ((uint16_t)payload[0] << 8) | (uint16_t)payload[1];
}

#endif
