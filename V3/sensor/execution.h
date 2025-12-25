#ifndef SENSOR_EXECUTION_H
#define SENSOR_EXECUTION_H

#include "protocol.h"
#include "srf02_i2c.h"
#include "sensor_state.h"


/*
 * ExecCommand
 * ----------
 * Comando ya validado y listo para ejecutar.
 * Producido por CommunicationThread.
 * Consumido por ExecutionThread.
 */
typedef struct {
  CommandId cmd;
  uint8_t   dir;
  uint8_t   plen;
  uint8_t   payload[MAX_PAYLOAD_SIZE];
} ExecCommand;

/*
 * ExecResponse
 * ------------
 * Respuesta lógica producida por Execution / Periodic.
 * Todavía NO es un Message.
 */
typedef struct {
  CommandId cmd;
  uint8_t   dir;
  ErrorCode status;
  uint8_t   plen;
  uint8_t   payload[MAX_PAYLOAD_SIZE];
  bool      is_event;   // true => MSG_FLAG_EVENT
} ExecResponse;



bool execution_handle_command(const ExecCommand *cmd,
                              ExecResponse *out_resp,
                              I2CJob *out_job);

void execution_handle_i2c_result(const I2CResult *res,
                                 ExecResponse *out_resp);

void execution_get_status(uint8_t dir,
                          ExecResponse *out_resp);

void execution_list_sensors(ExecResponse *out_resp);

bool message_to_exec_command(const Message *msg, ExecCommand *out);


#endif
