
/****************************************************************************
 * vendor/allwinnertech/apps/led_rgb/led_demo.c
 * Simple LED demonstration application for OpenVela.
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
#ifdef __cplusplus
extern "C" {
#endif

#include "sunxi_hal_ledc.h"
#include <debug.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// LED颜色定义
#define LED_RED 0xFF0000
#define LED_GREEN 0x00FF00
#define LED_BLUE 0x0000FF
#define LED_YELLOW 0xFFFF00
#define LED_CYAN 0x00FFFF
#define LED_MAGENTA 0xFF00FF
#define LED_WHITE 0xFFFFFF
#define LED_OFF 0x000000
// #define INTERFACE

int hal_test(int count, char **argvs) {
  int ret;

  // 设置LED颜色序列
  unsigned int colors[] = {LED_RED,  LED_GREEN,   LED_BLUE, LED_YELLOW,
                           LED_CYAN, LED_MAGENTA, LED_WHITE};
  int num_colors = sizeof(colors) / sizeof(colors[0]);

  _info("LEDC controller initialized\n");
  _info("Running LED color sequence...\n");
  _info("Press Ctrl+C to exit\n");
  // 初始化LEDC控制器
  hal_ledc_init();

  if (count < 2) {

    while (1) {
      for (int i = 0; i < num_colors; i++) {
        // 设置LED颜色
        ret = sunxi_set_led_brightness(0, colors[i]);
        if (ret != 0) {
          _info("Error setting LED color: %d\r\n", ret);
          return -1;
        }
        sleep(1); // 等待1秒
      }
    }

    hal_ledc_deinit();
    return 0;
  }

  int color = atoi(argvs[1]);
  ret = sunxi_set_led_brightness(0, color);
  // 清理资源（通常不会执行到这里）
  hal_ledc_deinit();
  _info("LED demonstration completed\r\n");
  return 0;
}

int file_test(int count, char **argvs) {
  int ret;
  int fd = open("/dev/leds0", O_RDWR);
  if (fd < 0) {
    _info("open /dev/leds0 failed\n");
    return -1;
  }
  if (count < 2) {
    // 设置LED颜色序列
    unsigned int colors[] = {LED_RED,  LED_GREEN,   LED_BLUE, LED_YELLOW,
                             LED_CYAN, LED_MAGENTA, LED_WHITE};
    int num_colors = sizeof(colors) / sizeof(colors[0]);

    for (int i = 0; i < num_colors; i++) {
      // 设置LED颜色
      ret = write(fd, &colors[i], sizeof(int));
      if (ret != sizeof(int)) {
        _info("Error setting LED color: %d\r\n", ret);
        close(fd);
        return -1;
      }
      _info("LED color: 0x%06X\r\n", colors[i]);
      sleep(1); // 等待1秒
    }
    close(fd);

    return 0;
  }
  int color = atoi(argvs[1]);
  write(fd, &color, sizeof(int));
  close(fd);
  return 0;
}

// 简单的LED演示程序
int main(int argc, char *argv[]) {

#ifdef CONFIG_LED_RGB_WS2812

  int ret = file_test(argc, argv);
  return ret;

#else
  int ret = hal_test(argc, argv);
  return ret;
#endif
}
#ifdef __cplusplus
}
#endif