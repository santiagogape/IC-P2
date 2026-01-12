#ifndef SUPERVISOR_PRINTER_H
#define SUPERVISOR_PRINTER_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

// Longitud canónica del payload de status
#define STATUS_PAYLOAD_LEN 7

// Imprime un Message recibido (response/event/help)
void printer_print_message(const Message *msg);
void printer_print_help(void);

// Decodifica payload de CMD_GET_STATUS => SensorStatus (DTO)
// (SensorStatus lo añadiste tú en protocol.h)
bool decode_status_payload(const Message *msg, SensorStatus *out);

#endif
