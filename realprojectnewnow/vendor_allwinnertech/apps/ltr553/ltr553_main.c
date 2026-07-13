/****************************************************************************
 * vendor/allwinnertech/apps/ltr553/ltr553_main.c
 * Simple LTR553 demonstration application for OpenVela.
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
#include <sensor/light.h>
#include <sensor/prox.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LIGHT_TIMEOUT 2000
#define READ_TIMES 100

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * ltr553_main
 ****************************************************************************/

int main(int argc, FAR char *argv[]) {
  FAR const struct orb_metadata *light_meta, *prox_meta;
  struct sensor_light light_data;
  struct sensor_prox prox_data;
  struct pollfd fds[2];
  int ret = OK;
  int light_fd, prox_fd;
  int i;

  light_meta = ORB_ID(sensor_light);
  light_fd = orb_subscribe_multi(light_meta, 0);
  if (light_fd < 0) {
    printf("sensor light subscribe error! return:%d\n", light_fd);
    return light_fd;
  }

  prox_meta = ORB_ID(sensor_prox);
  prox_fd = orb_subscribe_multi(prox_meta, 0);
  if (prox_fd < 0) {
    printf("sensor prox subscribe error! return:%d\n", prox_fd);
    orb_unsubscribe(light_fd);
    return prox_fd;
  }

  fds[0].fd = light_fd;
  fds[0].events = POLLIN;
  fds[1].fd = prox_fd;
  fds[1].events = POLLIN;

  for (i = 0; i < READ_TIMES; i++) {
    if (poll(fds, 2, LIGHT_TIMEOUT) > 0) {
      if (fds[0].revents & POLLIN) {
        ret = orb_copy(light_meta, light_fd, &light_data);
#ifdef CONFIG_DEBUG_UORB
        if (ret == OK && light_meta->o_format != NULL) {
          orb_info(light_meta->o_format, light_meta->o_name, &light_data);
        }
#endif
      }

      if (fds[1].revents & POLLIN) {
        ret = orb_copy(prox_meta, prox_fd, &prox_data);
#ifdef CONFIG_DEBUG_UORB
        if (ret == OK && prox_meta->o_format != NULL) {
          orb_info(prox_meta->o_format, prox_meta->o_name, &prox_data);
        }
#endif
      }
    } else if (ret == 0) {
      printf("Poll timeout %d/%d\n", i + 1, READ_TIMES);
      continue;
    } else if (errno != EINTR) {
      printf("Poll error: %d\n", errno);
      break;
    }
  }

  orb_unsubscribe(light_fd);
  orb_unsubscribe(prox_fd);
  printf("sensor ltr553 read examples exit.\n");

  return ret;
}