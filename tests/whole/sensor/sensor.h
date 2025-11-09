// ============================================================
/*   Mailboxes por sensor
   - Empaquetamos Message en uint32_t:
     bits: [7:0]=code, [23:8]=param
*/
// ============================================================
static inline uint32_t packMsg(const Message& m);
static inline Message unpackMsg(uint32_t w);

// ============================================================
//   Arranque con ChibiOS
// ============================================================
static void chSetup(void);
