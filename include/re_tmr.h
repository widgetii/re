/**
 * @file re_tmr.h  Interface to timer implementation
 *
 * Copyright (C) 2010 Creytiv.com
 */


/**
 * Defines the timeout handler
 *
 * @param arg Handler argument
 */
typedef void (tmr_h)(void *arg);

/** Defines a timer */
struct tmr {
	struct le le;       /**< Linked list element */
	tmr_h *th;          /**< Timeout handler     */
	void *arg;          /**< Handler argument    */
	uint64_t jfs;       /**< Jiffies for timeout */
	const char *file;
	int line;
};


void     tmr_poll(struct list *tmrl);
uint64_t tmr_jiffies_usec(void);
uint64_t tmr_jiffies(void);
uint64_t tmr_next_timeout(struct list *tmrl);
void     tmr_debug(void);
int      tmr_status(struct re_printf *pf, void *unused);

void     tmr_init(struct tmr *tmr);
void     tmr_start_dbg(struct tmr *tmr, uint64_t delay, tmr_h *th, void *arg,
		   const char *file, int line);

/**
 * @def tmr_start(tmr, delay, th, arg)
 *
 * Start a timer
 *
 * @param tmr   Timer to start
 * @param delay Timer delay in [ms]
 * @param th    Timeout handler
 * @param arg   Handler argument
 */
#define tmr_start(tmr, delay, th, arg)                                        \
	tmr_start_dbg(tmr, delay, th, arg, __FILE__, __LINE__)

void     tmr_cancel(struct tmr *tmr);
uint64_t tmr_get_expire(const struct tmr *tmr);


/**
 * Check if the timer is running
 *
 * @param tmr Timer to check
 *
 * @return true if running, false if not running
 */
static inline bool tmr_isrunning(const struct tmr *tmr)
{
	return tmr ? NULL != tmr->th : false;
}
