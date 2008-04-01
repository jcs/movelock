/* vim:ts=8
 * $Id: movelock.c,v 1.4 2008/04/01 01:12:01 jcs Exp $
 *
 * movelock
 * freak out when the accelerometer detects too much movement
 *
 * Copyright (c) 2005, 2008 joshua stein <jcs@jcs.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>

/* #define DEBUG 1 */

/* program to run when we detect too much movement (lock x, halt -p, etc.) */
#ifndef LOCK_PROG
#define LOCK_PROG "xautolock -locknow"
#endif

/* how many seconds to check movement */
#define MOVE_TIME 1

/* maximum amount of movement allowed per MOVE_TIME */
#define MOVE_THRESH_X 20
#define MOVE_THRESH_Y 10

/* MIB names for sensor (aps) device, X_VAR, and Y_VAR sensors */
#define APS_DEV_NUM	0
#define X_VAR_TYPE	10
#define X_VAR_NUMT	2
#define Y_VAR_TYPE	10
#define Y_VAR_NUMT	3

/* time to wait after allowing a second spawning of LOCK_PROG */
#define LOCK_WAIT 3

extern char *__progname;

int64_t	getsensor(int, int, int);
void	spawn(char *);

int
main(int argc, char *argv[])
{
	char *lockprog = LOCK_PROG;
	int lastvalues[] = { -1, -1 };
	int curvalues[] = { -1, -1 };
	int curchanges[] = { 0, 0 };
	time_t lasttime = 0, lastlock = 0;

	for (;;) {
		int newx, newy;
		int reset = 0, dolock = 0;
		time_t curtime;

		/* X_VAR: hw.sensors.aps0.raw2 */
		newx = getsensor(APS_DEV_NUM, X_VAR_TYPE, X_VAR_NUMT);

		/* Y_VAR: hw.sensors.aps0.raw3 */
		newy = getsensor(APS_DEV_NUM, Y_VAR_TYPE, Y_VAR_NUMT);

		if (!newx || !newy)
			goto wait;

		if (lastvalues[0] == -1) {
			/* initializing */
			lastvalues[0] = curvalues[0] = newx;
			lastvalues[1] = curvalues[1] = newy;

			continue;
		}

		/* tally change amounts */
		curchanges[0] += abs(newx - curvalues[0]);
		curchanges[1] += abs(newy - curvalues[1]);

		(void)time(&curtime);

		if ((curchanges[0] > MOVE_THRESH_X) ||
		    (curchanges[1] > MOVE_THRESH_Y))
			dolock++;
		else if ((curtime - lasttime) >= MOVE_TIME)
			reset++;

		if (dolock || reset) {
#ifdef DEBUG
			printf("%d [%d], %d [%d]\n", newx, curchanges[0],
				newy, curchanges[1]);
#endif
			lasttime = curtime;
			lastvalues[0] = curvalues[0];
			lastvalues[1] = curvalues[1];
			curchanges[0] = curchanges[1] = 0;

			if (dolock && ((curtime - lastlock) > LOCK_WAIT)) {
				(void)spawn(lockprog);
				(void)time(&lastlock);
			}
		}

		curvalues[0] = newx;
		curvalues[1] = newy;

wait:
		usleep(100);
	}

	exit(0);
}

int64_t
getsensor(int dev, int type, int numt)
{
	int mib[5];
	size_t slen;
	struct sensor sensor;

	slen = sizeof(sensor);

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	mib[2] = dev;
	mib[3] = type;
	mib[4] = numt;

	if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1)
		err(1, "sysctl");
	
	return (sensor.value);
}

void
spawn(char *prog)
{
	printf("%s: executing \"%s\"\n", __progname, prog);

	system(prog);
}
