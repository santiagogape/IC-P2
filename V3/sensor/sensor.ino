
#include <ChRt.h>

#include "protocol.h"
#include "framing.h"
#include "communication.h"

#include "sensor_state.h"
#include "execution.h"
#include "periodic.h"
#include "srf02_i2c.h"


// =====================
// Framing contexts
// =====================
static FrameRxContext rx_ctx;
static FrameTxContext tx_ctx;

// =====================
// Mailbox sizes
// =====================
#define MB_EXEC_SLOTS       6
#define MB_RESP_SLOTS       6
#define MB_I2CJOB_SLOTS     6
#define MB_I2CRES_SLOTS     6

// =====================
// Memory pools objects
// =====================

// ExecCommand pool
static ExecCommand execPool[MB_EXEC_SLOTS];
MEMORYPOOL_DECL(execMemPool, sizeof(ExecCommand), PORT_NATURAL_ALIGN, nullptr);

// ExecResponse pool
static ExecResponse respPool[MB_RESP_SLOTS];
MEMORYPOOL_DECL(respMemPool, sizeof(ExecResponse), PORT_NATURAL_ALIGN, nullptr);

// I2CJob pool
static I2CJob i2cJobPool[MB_I2CJOB_SLOTS];
MEMORYPOOL_DECL(i2cJobMemPool, sizeof(I2CJob), PORT_NATURAL_ALIGN, nullptr);

// I2CResult pool (solo para respuestas a Execution)
static I2CResult i2cResPool[MB_I2CRES_SLOTS];
MEMORYPOOL_DECL(i2cResMemPool, sizeof(I2CResult), PORT_NATURAL_ALIGN, nullptr);

// =====================
// Mailboxes (store pointers)
// =====================
static msg_t execSlots[MB_EXEC_SLOTS];
MAILBOX_DECL(exec_mb, execSlots, MB_EXEC_SLOTS);

static msg_t respSlots[MB_RESP_SLOTS];
MAILBOX_DECL(resp_mb, respSlots, MB_RESP_SLOTS);

static msg_t i2cJobSlots[MB_I2CJOB_SLOTS];
MAILBOX_DECL(i2c_job_mb, i2cJobSlots, MB_I2CJOB_SLOTS);

static msg_t i2cResSlots[MB_I2CRES_SLOTS];
MAILBOX_DECL(i2c_res_mb, i2cResSlots, MB_I2CRES_SLOTS);

// =====================
// Small helpers
// =====================

// Message buffer with fixed payload space (avoid flexible array pain)
typedef struct {
  uint8_t flags;
  uint8_t cmd;
  uint8_t dir;
  uint8_t status;
  uint8_t plen;
  uint8_t payload[MAX_PAYLOAD_SIZE];
} MessageFixed;

static void exec_response_to_messagefixed(const ExecResponse *r, MessageFixed *m) // symbol # used
{
  m->flags  = MSG_FLAG_RESPONSE | (r->is_event ? MSG_FLAG_EVENT : 0);
  m->cmd    = (uint8_t)r->cmd;
  m->dir    = r->dir;
  m->status = (uint8_t)r->status;
  m->plen   = r->plen;

  for (uint8_t i = 0; i < r->plen; ++i) m->payload[i] = r->payload[i];
}

// Exported for periodic.ino (so it can enqueue events)
void sensor_post_exec_response(const ExecResponse *resp)  // symbol # used
{
  ExecResponse *p = (ExecResponse*)chPoolAlloc(&respMemPool);
  if (!p) return;
  *p = *resp;
  chMBPostTimeout(&resp_mb, (msg_t)p, TIME_INFINITE);
}

// =====================
// Threads
// =====================

// Priorities
#define COMM_PRIO   (NORMALPRIO + 2)
#define EXEC_PRIO   (NORMALPRIO + 1)
#define I2C_PRIO    (NORMALPRIO)
#define PER_PRIO    (NORMALPRIO - 1)

// Stacks
static THD_WORKING_AREA(waComm, 768);
static THD_WORKING_AREA(waExec, 768);
static THD_WORKING_AREA(waI2C,  768);
static THD_WORKING_AREA(waPer1, 512);
static THD_WORKING_AREA(waPer2, 512);

// ---------------------
// CommunicationThread
// ---------------------
static THD_FUNCTION(CommunicationThread, arg)
{
  (void)arg;

  // local scratch
  MessageFixed in_fixed;
  Message *in = (Message*)&in_fixed;

  while (true) {

    // RX: request from supervisor
    if (comm_poll_message(&rx_ctx, in)) {

      ExecCommand *cmd = (ExecCommand*)chPoolAlloc(&execMemPool);
      if (cmd) {
        if (!message_to_exec_command(in, cmd)) {
          chPoolFree(&execMemPool, cmd);
        } else {
          // post pointer to exec mailbox
          chMBPostTimeout(&exec_mb, (msg_t)cmd, TIME_INFINITE);
        }
      }
    }

    // TX: responses/events to supervisor
    ExecResponse *resp = nullptr;
    if (chMBFetchTimeout(&resp_mb, (msg_t*)&resp, TIME_INFINITE) == MSG_OK && resp) {

      MessageFixed out_fixed;
      exec_response_to_messagefixed(resp, &out_fixed);

      comm_send_message((Message*)&out_fixed, &tx_ctx);

      chPoolFree(&respMemPool, resp);
    }

    chThdSleepMilliseconds(2);
  }
}

// ---------------------
// ExecutionThread
// ---------------------
static THD_FUNCTION(ExecutionThread, arg)
{
  (void)arg;

  while (true) {

    ExecCommand *cmd = nullptr;
    if (chMBFetchTimeout(&exec_mb, (msg_t*)&cmd, TIME_INFINITE) != MSG_OK || !cmd)
      continue;

    // Prepare response object from pool
    ExecResponse resp;
    I2CJob job;
    chMtxLock(&sensors_mtx);
    bool need_i2c = execution_handle_command(cmd, &resp, &job);
    chMtxUnlock(&sensors_mtx);
    // cmd no longer needed
    chPoolFree(&execMemPool, cmd);

    // Immediate response
    if (!need_i2c) {
      sensor_post_exec_response(&resp);
      continue;
    }

    // Need I2C: send job to I2C thread
    I2CJob *pj = (I2CJob*)chPoolAlloc(&i2cJobMemPool);
    if (!pj) {
      resp.status = ERR_INTERNAL;
      resp.plen = 0;
      sensor_post_exec_response(&resp);
      continue;
    }
    *pj = job;

    // Post job and wait result
    if (chMBPostTimeout(&i2c_job_mb, (msg_t)pj, TIME_INFINITE) != MSG_OK) {
      chPoolFree(&i2cJobMemPool, pj);
      resp.status = ERR_INTERNAL;
      resp.plen = 0;
      sensor_post_exec_response(&resp);
      continue;
    }

    // Wait I2C result
    I2CResult *pres = nullptr;
    msg_t r = chMBFetchTimeout(&i2c_res_mb, (msg_t*)&pres, TIME_INFINITE);
    if (r != MSG_OK || !pres) {
      resp.status = ERR_INTERNAL;
      resp.plen = 0;
      sensor_post_exec_response(&resp);
      continue;
    }

    execution_handle_i2c_result(pres, &resp);

    if (pres->status == ERR_OK) {
      chMtxLock(&sensors_mtx);
      SensorState *st = find_state(pres->dir);
      if (st) st->last_measurement = chVTGetSystemTime();
      chMtxUnlock(&sensors_mtx);
    }

    chPoolFree(&i2cResMemPool, pres);

    sensor_post_exec_response(&resp);
  }
}

// ---------------------
// I2CThread
// - executes jobs sequentially
// - routes results: exec via i2c_res_mb
// - periodic will call srf02_execute_job() from its own thread? NO -> it posts to i2c_job_mb too
//   (we handle that by interpreting job->unit and dir only; periodic will consume result as EVENT here)
// ---------------------
static THD_FUNCTION(I2CThread, arg)
{
  (void)arg;

  while (true) {

    I2CJob *job = nullptr;
    if (chMBFetchTimeout(&i2c_job_mb, (msg_t*)&job, TIME_INFINITE) != MSG_OK || !job)
      continue;

    // Run hardware I2C (blocking ok inside this thread)
    I2CResult res = srf02_execute_job(job);

    // Free job object
    chPoolFree(&i2cJobMemPool, job);

    // Decide who consumes result:
    // - If periodic is enabled for that dir AND this measurement is due, periodic.ino can request EVENT jobs.
    // For simplicity: I2CThread always returns result to Execution via i2c_res_mb,
    // and periodic.ino will *not* post jobs; instead it triggers via execution (see note below).
    //
    // If you WANT periodic to use I2CThread too, we add a "source" field in I2CJob. (next iteration)

    I2CResult *pres = (I2CResult*)chPoolAlloc(&i2cResMemPool);
    if (!pres) continue;
    *pres = res;

    chMBPostTimeout(&i2c_res_mb, (msg_t)pres, TIME_INFINITE);
  }
}
static THD_FUNCTION(PeriodicThread, arg)
{
  uint8_t dir = (uint8_t)(uintptr_t)arg;

  while (true) {

    SensorState snapshot;

    chMtxLock(&sensors_mtx);
    SensorState *st = find_state(dir);
    if (!st) {
      chMtxUnlock(&sensors_mtx);
      chThdSleepMilliseconds(500);
      continue;
    }
    snapshot = *st;
    chMtxUnlock(&sensors_mtx);

    if (!periodic_should_run(&snapshot)) {
      chThdSleepMilliseconds(50);
      continue;
    }

    systime_t now = chVTGetSystemTime();
    uint32_t wait_ms = periodic_time_until_next(&snapshot, now);
    if (wait_ms > 0) {
      chThdSleepMilliseconds(wait_ms);
      continue;
    }

    I2CJob job = { snapshot.dir, snapshot.unit };
    I2CResult res = srf02_execute_job(&job);

    if (res.status == ERR_OK) {
      chMtxLock(&sensors_mtx);
      SensorState *st2 = find_state(dir);
      if (st2) st2->last_measurement = chVTGetSystemTime();
      chMtxUnlock(&sensors_mtx);
    }

    ExecResponse evt;
    periodic_handle_i2c_result(&res, &evt);
    sensor_post_exec_response(&evt);
  }
}


// =====================
// chSetup
// =====================
void chSetup()
{
  chMtxObjectInit(&sensors_mtx);
  // Load memory pools
  chPoolLoadArray(&execMemPool, execPool, MB_EXEC_SLOTS);
  chPoolLoadArray(&respMemPool, respPool, MB_RESP_SLOTS);
  chPoolLoadArray(&i2cJobMemPool, i2cJobPool, MB_I2CJOB_SLOTS);
  chPoolLoadArray(&i2cResMemPool, i2cResPool, MB_I2CRES_SLOTS);

  chThdCreateStatic(waComm, sizeof(waComm), COMM_PRIO, CommunicationThread, nullptr);
  chThdCreateStatic(waExec, sizeof(waExec), EXEC_PRIO, ExecutionThread, nullptr);
  chThdCreateStatic(waI2C,  sizeof(waI2C),  I2C_PRIO,  I2CThread, nullptr);

  // 2 periodic threads fixed
  chThdCreateStatic(waPer1, sizeof(waPer1), PER_PRIO, PeriodicThread, (void*)(uintptr_t)US_1);
  chThdCreateStatic(waPer2, sizeof(waPer2), PER_PRIO, PeriodicThread, (void*)(uintptr_t)US_2);
}

// =====================
// Arduino setup/loop
// =====================
void setup()
{
  Serial1.begin(9600);
  srf02_i2c_init();

  // init rx/tx contexts
  rx_ctx.index = 0;
  rx_ctx.expected_len = 0;
  rx_ctx.in_frame = false;
  tx_ctx.len = 0;

  chBegin(chSetup);
}

void loop()
{
  chThdSleepMilliseconds(1000);
}
