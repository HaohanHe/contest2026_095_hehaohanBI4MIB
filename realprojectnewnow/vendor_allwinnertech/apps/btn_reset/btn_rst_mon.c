/****************************************************************************
 * vendor/allwinnertech/apps/btn_reset/btn_rst_mon.c
 *
 * Application to monitor the RST button status and reset the system if it is
 * pressed for a certain time.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <nuttx/config.h>
#include <arch/chip/mi_hw_version.h>
#include <nuttx/input/buttons.h>


#define BTN_DEVPATH "/dev/input/event1"
#define HW_VERSION_DVT2 2
#define HW_VERSION_DVT3 3

#define BTN_RST_VALUE_DVT2 0x01
#define BTN_RST_VALUE_DVT3 0x08

//#define ESCAPE_TIME_FROM_POWERON_MS (3 * 1000)

int main(int argc, char *argv[])
{
	int fd = -1, ret = -1;
	btn_buttonset_t val, valid_rst_val = BTN_RST_VALUE_DVT2;
#if 0 // No need to wait
	struct pollfd pfd;
	int timeout = CONFIG_BTN_RST_MON_TIME_MS - ESCAPE_TIME_FROM_POWERON_MS;
	struct timeval start, now;
	time_t escape = 0;
#endif

	if (hw_version_get() > 2)
		valid_rst_val = BTN_RST_VALUE_DVT3;

	fd = open(BTN_DEVPATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("Failed to open %s: %d\n", BTN_DEVPATH, errno);
		goto skip_reset;
	}

	ret = read(fd, &val, sizeof(val));
	if (ret < 0) {
		printf("Failed to read %s: %d\n", BTN_DEVPATH, errno);
		goto skip_reset;
	}

	if (val != valid_rst_val) {
		printf("Rst Button is released, exit directly\n");
		goto skip_reset;
	}

#if 0 // No need to wait, detect RST BUTTON pressed, reset system directly
	pfd.fd = fd;
	pfd.events = POLLIN | POLLERR;
	pfd.revents = 0;
	gettimeofday(&start, NULL);

	while (ret = poll(&pfd, 1, timeout)) {
		if (ret < 0) {
			if (EINTR == errno || EAGAIN == errno) {
				/* calac escape time
				 * if running out of monitor time, reset system
				 */
				gettimeofday(&now, NULL);
				escape = now.tv_sec - start.tv_sec;
				timeout -= escape;	
				if (timeout > 0) {
					pfd.fd = fd;
					pfd.events = POLLIN | POLLERR;
					pfd.revents = 0;
					gettimeofday(&start, NULL);
					continue;	
				} else {
					// it's impossible to be here actually
					printf("That's impossible to be here !!!");
					break;
				}
			} else {
				// skip reset
				// TBD: add retry logic
				goto skip_reset;
			}
		} else if (ret > 0) {
			// Rst button is released in monitor time, don't reset system, exit directly
			printf("Release Rst Button, exit\n");
			goto skip_reset;
		}
	}
#endif
	// Rst Button still is pressed, reset system
	ret = 0;

skip_reset:
	if (fd >= 0)
		close(fd);

	return ret;
}

