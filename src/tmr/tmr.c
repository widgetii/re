/**
 * @file tmr.c  Timer implementation
 *
 * Copyright (C) 2010 Creytiv.com
 */
#if defined(FREEBSD) || defined(OPENBSD) || defined(DARWIN)
#define _DEFAULT_SOURCE 1
#else
#define _POSIX_C_SOURCE 199309L
#endif

#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifndef WIN32
#include <time.h>
#endif
#include <re_types.h>
#include <re_list.h>
#include <re_fmt.h>
#include <re_mem.h>
#include <re_tmr.h>
#include <re_net.h>
#include <re_main.h>


#define DEBUG_MODULE "tmr"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


/** Timer values */
enum {
	MAX_BLOCKING = 500   /**< Maximum time spent in handler [ms] */
};

extern struct list *tmrl_get(void);


static bool inspos_handler(struct le *le, void *arg)
{
	struct tmr *tmr = le->data;
	const uint64_t now = *(uint64_t *)arg;

	return tmr->jfs <= now;
}


static bool inspos_handler_0(struct le *le, void *arg)
{
	struct tmr *tmr = le->data;
	const uint64_t now = *(uint64_t *)arg;

	return tmr->jfs > now;
}


#if TMR_DEBUG
static void call_handler(tmr_h *th, void *arg)
{
	const uint64_t tick = tmr_jiffies();
	uint32_t diff;

	/* Call handler */
	th(arg);

	diff = (uint32_t)(tmr_jiffies() - tick);

	if (diff > MAX_BLOCKING) {
		DEBUG_WARNING("long async blocking: %u>%u ms (h=%p arg=%p)\n",
			      diff, MAX_BLOCKING, th, arg);
	}
}
#endif


/**
 * Poll all timers in the current thread
 *
 * @param tmrl Timer list
 */
void tmr_poll(struct list *tmrl)
{
	const uint64_t jfs = tmr_jiffies();

	for (;;) {
		struct tmr *tmr;
		tmr_h *th;
		void *th_arg;

		tmr = list_ledata(tmrl->head);

		if (!tmr || (tmr->jfs > jfs)) {
			break;
		}

		th = tmr->th;
		th_arg = tmr->arg;

		tmr->th = NULL;

		list_unlink(&tmr->le);

		if (!th)
			continue;

#if TMR_DEBUG
		call_handler(th, th_arg);
#else
		th(th_arg);
#endif
	}
}


/**
 * Get the timer jiffies in microseconds
 *
 * @return Jiffies in [us]
 */
uint64_t tmr_jiffies_usec(void)
{
	uint64_t jfs;

#if defined(WIN32)
	LARGE_INTEGER li;
	static LARGE_INTEGER freq;

	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);

	QueryPerformanceCounter(&li);
	li.QuadPart *= 1000000;
	li.QuadPart /= freq.QuadPart;

	jfs = li.QuadPart;
#else
	struct timespec now;
	clockid_t clock_id;

	/* Use CLOCK_MONOTONIC_RAW, if available,
	   which is not subject to adjustment by NTP */
#ifdef CLOCK_MONOTONIC_RAW
	clock_id = CLOCK_MONOTONIC_RAW;
#else
	clock_id = CLOCK_MONOTONIC;
#endif

	if (0 != clock_gettime(clock_id, &now)) {
		DEBUG_WARNING("jiffies: clock_gettime() failed (%m)\n", errno);
		return 0;
	}

	jfs  = (long)now.tv_sec * (uint64_t)1000000;
	jfs += now.tv_nsec/1000;
#endif

	return jfs;
}


/**
 * Get the timer jiffies in milliseconds
 *
 * @return Jiffies in [ms]
 */
uint64_t tmr_jiffies(void)
{
	return tmr_jiffies_usec() / 1000;
}


/**
 * Get number of milliseconds until the next timer expires
 *
 * @param tmrl Timer-list
 *
 * @return Number of [ms], or 0 if no active timers
 */
uint64_t tmr_next_timeout(struct list *tmrl)
{
	const uint64_t jif = tmr_jiffies();
	const struct tmr *tmr;

	tmr = list_ledata(tmrl->head);
	if (!tmr)
		return 0;

	if (tmr->jfs <= jif)
		return 1;
	else
		return tmr->jfs - jif;
}


int tmr_status(struct re_printf *pf, void *unused)
{
	struct list *tmrl = tmrl_get();
	struct le *le;
	uint32_t n;
	int err;

	(void)unused;

	n = list_count(tmrl);
	if (!n)
		return 0;

	err = re_hprintf(pf, "Timers (%u):\n", n);

	for (le = tmrl->head; le; le = le->next) {
		const struct tmr *tmr = le->data;
		err |= re_hprintf(pf, "  %p: th=%p expire=%llums file=%s:%d\n",
				  tmr, tmr->th,
				  (unsigned long long)tmr_get_expire(tmr),
				  tmr->file, tmr->line);
	}

	if (n > 100)
		err |= re_hprintf(pf, "    (Dumped Timers: %u)\n", n);

	return err;
}


/**
 * Print timer debug info to stderr
 */
void tmr_debug(void)
{
	if (!list_isempty(tmrl_get()))
		(void)re_fprintf(stderr, "%H", tmr_status, NULL);
}


/**
 * Initialise a timer object
 *
 * @param tmr Timer to initialise
 */
void tmr_init(struct tmr *tmr)
{
	if (!tmr)
		return;

	memset(tmr, 0, sizeof(*tmr));
}


void tmr_start_dbg(struct tmr *tmr, uint64_t delay, tmr_h *th, void *arg,
		   const char *file, int line)
{
	struct list *tmrl = tmrl_get();
	struct le *le;

	if (!tmr)
		return;

#ifndef RELEASE
	if (re_thread_check())
		return;
#endif

	if (tmr->th) {
		list_unlink(&tmr->le);
	}

	tmr->th	  = th;
	tmr->arg  = arg;
	tmr->file = file;
	tmr->line = line;

	if (!th)
		return;

	tmr->jfs = delay + tmr_jiffies();

	if (delay == 0) {
		le = list_apply(tmrl, true, inspos_handler_0, &tmr->jfs);
		if (le) {
			list_insert_before(tmrl, le, &tmr->le, tmr);
		}
		else {
			list_append(tmrl, &tmr->le, tmr);
		}
	}
	else {
		le = list_apply(tmrl, false, inspos_handler, &tmr->jfs);
		if (le) {
			list_insert_after(tmrl, le, &tmr->le, tmr);
		}
		else {
			list_prepend(tmrl, &tmr->le, tmr);
		}
	}
}


/**
 * Cancel an active timer
 *
 * @param tmr Timer to cancel
 */
void tmr_cancel(struct tmr *tmr)
{
	tmr_start(tmr, 0, NULL, NULL);
}


/**
 * Get the time left until timer expires
 *
 * @param tmr Timer object
 *
 * @return Time in [ms] until expiration
 */
uint64_t tmr_get_expire(const struct tmr *tmr)
{
	uint64_t jfs;

	if (!tmr || !tmr->th)
		return 0;

	jfs = tmr_jiffies();

	return (tmr->jfs > jfs) ? (tmr->jfs - jfs) : 0;
}
