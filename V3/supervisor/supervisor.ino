

#include "protocol.h"
#include "framing.h"
#include "communication.h"
#include "cli.h"
#include "command.h"
#include "printer.h"

// =====================
// Communication contexts
// =====================
static FrameRxContext rx_ctx;
static FrameTxContext tx_ctx;

// =====================
// Request timeout control
// =====================
static bool      waiting_response = false;
static uint32_t  response_deadline_ms = 0;
static CommandId pending_cmd;
static uint8_t   pending_dir;

#define RESPONSE_TIMEOUT_MS 5000
#define SUPERVISOR_LOOP_DELAY_MS 5

// =====================
// Arduino setup
// =====================
void setup()
{
  SerialUSB.begin(115200);
  while (!SerialUSB) {}

  Serial1.begin(9600);

  cli_init();

  // Inicializar contextos de framing
  rx_ctx.index = 0;
  rx_ctx.expected_len = 0;
  rx_ctx.in_frame = false;

  tx_ctx.len = 0;

  SerialUSB.println("Supervisor ready. Type 'help'.");
}

// =====================
// Main supervisor loop
// =====================
void loop()
{
  ConsoleCommand cmd;
  Message msg;

  // ---------------------
  // CLI → request
  // ---------------------
  if (cli(&cmd)) {

    // help es local (no va al sensor)
    if (cmd.cmd == CMD_HELP) {
      printer_print_help();
    }
    else if (waiting_response) {
      printer_print_busy();
    }
    else {
      

      // Construir mensaje de protocolo
      if (exec_build_message_from_console(&cmd, &msg)) {

        // Enviar request al sensor
        comm_send_message(&msg, &tx_ctx);

        // Armar timeout
        waiting_response = true;
        pending_cmd = (CommandId)msg.cmd;
        pending_dir = msg.dir;
        response_deadline_ms = millis() + RESPONSE_TIMEOUT_MS;
      }
      else {
        // Error local de construcción (no debería pasar)
        SerialUSB.println("Local error building command");
      }
    }
  } else printer_print_cli_error(cli_last_error());

  // ---------------------
  // Incoming responses / events
  // ---------------------
  while (comm_poll_message(&rx_ctx, &msg)) {

    // Imprimir siempre
    printer_print_message(&msg);

    // Los eventos periódicos NO cancelan timeout
    if (msg.flags & MSG_FLAG_EVENT)
      continue;

    // Cancelar timeout si coincide la response esperada
    if (waiting_response &&
        msg.cmd == pending_cmd &&
        msg.dir == pending_dir) {

      waiting_response = false;
    }
  } 
  

  // ---------------------
  // Timeout expirado
  // ---------------------
  if (waiting_response && millis() > response_deadline_ms) {

    Message timeout_msg;
    timeout_msg.flags  = MSG_FLAG_RESPONSE;
    timeout_msg.cmd    = pending_cmd;
    timeout_msg.dir    = pending_dir;
    timeout_msg.status = ERR_TIMEOUT;
    timeout_msg.plen   = 0;

    printer_print_message(&timeout_msg);

    waiting_response = false;
  }

  // ---------------------
  // Cooperative yield
  // ---------------------
  delay(SUPERVISOR_LOOP_DELAY_MS);
}
