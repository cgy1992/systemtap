#ifndef _IO_C_ /* -*- linux-c -*- */
#define _IO_C_

#include "relay-app.h"
#include "print.c"

/** @file io.c
 * @brief I/O functions
 */
/** @addtogroup io I/O
 * I/O functions
 * @{
 */

/** private buffer for _stp_log() */
#define STP_LOG_BUF_LEN 2047
static char _stp_lbuf[NR_CPUS][STP_LOG_BUF_LEN + 1];

/** Logs Data.
 * This function prints to the system log if stpd has not connected
 * yet.  Otherwise it sends the message immediately to stpd.
 * @param fmt A variable number of args.
 * @note Lines are limited in length by printk buffer. If there is
 * no newline in the format string, then other syslog output could
 * get appended to the SystemTap line.
 * @todo Evaluate if this function is necessary.
 */

void _stp_log (const char *fmt, ...)
{
	int num;
	char *buf = &_stp_lbuf[smp_processor_id()][0];
	va_list args;
	va_start(args, fmt);
	num = vscnprintf (buf, STP_LOG_BUF_LEN, fmt, args);
	va_end(args);
	buf[num] = '\0';

	if (app.logging)
		send_reply (STP_REALTIME_DATA, buf, num + 1, stpd_pid);
	else
		printk("STP: %s", buf);
}

static void stpd_app_started(void)
{
	printk ("stpd has started.\n");
}

static void stpd_app_stopped(void)
{
	printk ("stpd has stopped.\n");
}

static void probe_exit(void);

#include <linux/delay.h>
static int stpd_command (int type, void *data)
{
	if (type == STP_EXIT) {
		printk ("STP_EXIT received\n");
		probe_exit();
#ifndef STP_NETLINK_ONLY
		relay_flush(app.chan);
		ssleep(2); /* FIXME: time for data to be flushed */
#endif
		send_reply (STP_EXIT, __this_module.name, strlen(__this_module.name) + 1, stpd_pid);
		return 1;
	}
	return 0;
}

/*
 * relay-app callbacks
 */
static struct relay_app_callbacks stp_callbacks =
{
	.app_started = stpd_app_started,
	.app_stopped = stpd_app_stopped,
	.user_command = stpd_command
};

/** Opens netlink and relayfs connections to stpd.
 * This must be called before any I/O is done, probably 
 * at the start of module initialization.
 */
int _stp_netlink_open(void)
{
	if (init_relay_app("stpd", "cpu", &stp_callbacks)) {
		printk ("STP: couldn't init relay app\n");
		return -1;
	}
	return 0;
}

/** Closes netlink and relayfs connections to stpd.
 * This must be called after all I/O is done, probably 
 * at the end of module cleanup.
 * @returns 0 on success.  -1 if there is a problem establishing
 * a connection.
 */
	
void _stp_netlink_close (void)
{
	send_reply (STP_DONE, NULL, 0, stpd_pid);
	close_relay_app();
}

/** @} */
#endif /* _IO_C_ */
