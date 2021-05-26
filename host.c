#include <stdatomic.h>
#include <pthread.h>
#include <able/able.h>
#include "host.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "trap.h"

#define DS0 ABLE_CORE_DSV(&host->c, 1)
#define DS1 ABLE_CORE_DSV(&host->c, 2)

#define P(C, Y) \
	fprintf(stderr, "%02"PRIX8"(%d) %"PRId64" %08"PRIX64" %"PRId64"/%"PRIX64" %"PRId64"/%"PRIX64" (%"PRIu8"); %"PRId64"/%"PRIX64" (%"PRIu8")\n", \
		(C)->i, \
		(Y), \
		(C)->ts, \
		(C)->p, \
		(C)->dp >= 2 ? ABLE_CORE_DSV(C, 2) : 0, (C)->dp >= 2 ? ABLE_CORE_DSV(C, 2) : 0, \
		(C)->dp >= 1 ? ABLE_CORE_DSV(C, 1) : 0, (C)->dp >= 1 ? ABLE_CORE_DSV(C, 1) : 0, \
		(C)->dp, \
		(C)->cp >= 1 ? ABLE_CORE_CSV(C, 1) : 0, (C)->cp >= 1 ? ABLE_CORE_CSV(C, 1) : 0, \
		(C)->cp);

#define DSI ABLE_CORE_DSI

#define DSR(C) \
	(C)->dp = 0;

#define CSR(C) \
	(C)->cp = 0;

#define T(C, N) \
	DSI(C); \
	DS0 = host->c.p - 1; \
	DSI(C); \
	DS0 = (N); \
	host->c.p = 0;

int
host_init(able_host_t *host) {
	T(&host->c, 1);
	return 0;
}

int
host_exec(able_host_t *host) {
	for (;;) {
		if (host == trap_data.u) {
			int q;
			q = atomic_exchange(&trap_data.q, 0);
			if (q == 1) {
				DSR(&host->c);
				CSR(&host->c);
				T(&host->c, 0);
			}
		}
		int y;
		y = able_host_exec(host);
		switch (y) {
			case -1: // end of timeslice
				able_host_node_wait_shim(host->n, NULL, NULL);
				return y;
			case -2: // bad memory access
				T(&host->c, 2);
				break;
			case -3: // divide by zero
				T(&host->c, 3);
				break;
			case -4: // illegal instruction
				switch (host->c.i) {
					case 0x85: // reset
						DSR(&host->c);
						CSR(&host->c);
						T(&host->c, 0);
						break;
					case 0x86: { // depth
						uint8_t dp;
						dp = host->c.dp;
						uint8_t cp;
						cp = host->c.cp;
						DSI(&host->c);
						DS0 = cp;
						DSI(&host->c);
						DS0 = dp;
						break;
					}
					case 0xfe: // debug
						P(&host->c, y);
						break;
					default:
#ifdef DEBUG
						P(&host->c, y);
#endif
						T(&host->c, 4);
						break;
				}
				break;
			case -5: // start of timeslice
				return y;
			case -6: // stack underflow
				DSR(&host->c);
				CSR(&host->c);
				T(&host->c, 6);
				break;
			case -7: // stack overflow
				DSR(&host->c);
				CSR(&host->c);
				T(&host->c, 7);
				break;
			case -8: // illegal register
				T(&host->c, 8);
				break;
		}
	}

	// should not happen
	return 1;
}
