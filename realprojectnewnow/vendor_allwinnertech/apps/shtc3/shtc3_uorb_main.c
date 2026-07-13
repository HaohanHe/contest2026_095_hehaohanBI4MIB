/****************************************************************************
 * vendor/allwinnertech/apps/shtc3/shtc3_uorb_main.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <poll.h>
#include <sensor/humi.h>
#include <sensor/temp.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEMP_TIMEOUT 1000
#define READ_TIMES 100

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * shtc3_main
 ****************************************************************************/

int main(int argc, FAR char *argv[]) {
  FAR const struct orb_metadata *temp_meta, *humi_meta;
  struct sensor_temp temp_data;
  struct sensor_humi humi_data;
  struct pollfd fds[2];
  int ret = OK;
  int temp_fd, humi_fd;
  int i;

  temp_meta = ORB_ID(sensor_temp);
  temp_fd = orb_subscribe_multi(temp_meta, 0);
  if (temp_fd < 0) {
    printf("sensor temp subscribe error! return:%d\n", temp_fd);
    return temp_fd;
  }

  humi_meta = ORB_ID(sensor_humi);
  humi_fd = orb_subscribe_multi(humi_meta, 0);
  if (humi_fd < 0) {
    printf("sensor humi subscribe error! return:%d\n", humi_fd);
    orb_unsubscribe(temp_fd);
    return humi_fd;
  }

  fds[0].fd = temp_fd;
  fds[0].events = POLLIN;
  fds[1].fd = humi_fd;
  fds[1].events = POLLIN;

  for (i = 0; i < READ_TIMES; i++) {
    if (poll(fds, 2, TEMP_TIMEOUT) > 0) {
      if (fds[0].revents & POLLIN) {
        ret = orb_copy(temp_meta, temp_fd, &temp_data);
#ifdef CONFIG_DEBUG_UORB
        if (ret == OK && temp_meta->o_format != NULL) {
          orb_info(temp_meta->o_format, temp_meta->o_name, &temp_data);
        }
#endif
      }

      if (fds[1].revents & POLLIN) {
        ret = orb_copy(humi_meta, humi_fd, &humi_data);
#ifdef CONFIG_DEBUG_UORB
        if (ret == OK && humi_meta->o_format != NULL) {
          orb_info(humi_meta->o_format, humi_meta->o_name, &humi_data);
        }
#endif
      }
    } else if (errno != EINTR) {
      printf("Waited for %d milliseconds without a message. "
             "Giving up. err:%d",
             TEMP_TIMEOUT, errno);
      break;
    }
  }

  orb_unsubscribe(temp_fd);
  orb_unsubscribe(humi_fd);
  printf("sensor shtc3 read examples exit.\n");

  return ret;
}