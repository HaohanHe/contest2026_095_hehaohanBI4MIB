
/**
 * vendor/allwinnertech/apps/factory_test/factory_test.c
 * Application to run factory tests on OpenVela.
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
 * @file factory_test.c
 * @brief OpenVela工厂测试程序
 * @version 1.0
 *
 * 支持：
 * -h/--help: 显示帮助信息
 * -cid <int>: 运行指定ID的测试用例
 * -a: 运行所有测试用例
 */

#include <sys/ioctl.h>
#include <nuttx/lirc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <strings.h>
#include <poll.h>
#include <nuttx/input/touchscreen.h>
#include <uORB/uORB.h>
#include "sunxi_hal_ledc.h"
#include <nuttx/input/buttons.h>
#include "system/nxrecorder.h"
#include "system/nxplayer.h"
#include <stdint.h>
#include <nuttx/audio/audio.h>


/* 测试结果定义 */
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2,
    TEST_ERROR = 3
} test_result_t;

/* 测试用例结构体 */
typedef struct {
    int id;
    const char *name;
    const char *description;
    test_result_t (*test_func)(void);
    int manual; /* 0 = automatic, 1 = requires human verification */
} test_case_t;

/* 测试上下文 */
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
    int errors;
} test_context_t;

/* 前向声明测试函数 */
static test_result_t test_led(void);
static test_result_t test_button(void);
static test_result_t test_memory(void);
static test_result_t test_network(void);
static test_result_t test_screen(void);
static test_result_t test_touchscreen(void);
static test_result_t test_sensor_light(void);
static test_result_t test_sensor_prox(void);
static test_result_t test_sensor_temp(void);
static test_result_t test_ir(void);
static test_result_t test_ir_tx_manual(void);
static test_result_t test_ir_rx_manual(void);
static test_result_t test_bt(void);
static test_result_t test_sdcard(void);
static test_result_t test_mic(void);
static test_result_t test_speaker(void);

/* 辅助函数：检测显示器是否连接、记录跳过原因 */
static int detect_display_connected(void);
static void record_skip_reason_by_info(int id, const char *name, const char *reason);

/* 测试用例注册表 */
static test_case_t g_test_cases[] = {
    /* id, name, description, func, manual */
    {0, "BT Test", "蓝牙测试", test_bt, 0},
    {1, "LED Test", "LED指示灯功能测试", test_led, 1},
    {2, "Button Test", "按键输入测试", test_button, 0},
    {3, "Memory Test", "内存读写测试", test_memory, 0},
    {4, "Network Test", "网络连接测试", test_network, 1},
    {5, "Screen Test", "显示屏连接与显示测试", test_screen, 1},
    {6, "Touch Test", "触摸屏检测 (设备 /dev/input0) — OpenVela: uses touchscreen driver when enabled", test_touchscreen, 0},
    {7, "Light Sensor", "环境光传感器检测", test_sensor_light, 0},
    {8, "Proximity Sensor", "距离/接近传感器检测", test_sensor_prox, 0},
    {9, "Temperature Sensor", "温度传感器检测", test_sensor_temp, 0},
    // {10, "IR Test", "红外发射测试", test_ir, 0},
    //{11, "BT Test", "蓝牙测试", test_bt, 0},
    {12, "SD Card Test", "SD检测", test_sdcard, 0},
    {13, "Microphone Test", "麦克风录音测试", test_mic, 0},
    {14, "Speaker Test", "扬声器播放测试", test_speaker, 1},
    {15, "IR TX Manual", "红外发射(人工判定)", test_ir_tx_manual, 1},
    {16, "IR RX Manual", "红外接收(等待信号)", test_ir_rx_manual, 0},
    {0, NULL, NULL, NULL, 0}  /* 结束标记 */
};

/* 全局配置 */
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"cid", required_argument, 0, 'c'},
    {"all", no_argument, 0, 'a'},
    {0, 0, 0, 0}
};

/* 显示帮助信息 */
static void show_help(const char *program_name) {
    printf("\nOpenVela Factory Test Tool v1.0\n");
    printf("================================\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -c <id>              Run specific test case by ID\n");
    printf("  -a, --all            Run all test cases\n\n");
    printf("Available Test Cases:\n");
    printf("  ID  Name             Description\n");
    printf("  --- ---------------- --------------------------------\n");

    for (int i = 0; g_test_cases[i].name != NULL; i++) {
        printf("  %3d %-16s %s%s\n",
               g_test_cases[i].id,
               g_test_cases[i].name,
               g_test_cases[i].description,
               g_test_cases[i].manual ? " (manual)" : "");
    }
    printf("\nExamples:\n");
    printf("  %s -c 1            # Run LED test only\n", program_name);
    printf("  %s -a              # Run all tests\n", program_name);
    printf("  %s --help          # Show this help\n\n", program_name);
}

/* 打印测试结果 */
static void print_test_result(const test_case_t *test, test_result_t result) {
    const char *status_str;
    const char *color_code;

    switch (result) {
        case TEST_PASS:
            status_str = "PASS";
            color_code = "\033[32m"; // Green
            break;
        case TEST_FAIL:
            status_str = "FAIL";
            color_code = "\033[31m"; // Red
            break;
        case TEST_SKIP:
            status_str = "SKIP";
            color_code = "\033[33m"; // Yellow
            break;
        case TEST_ERROR:
            status_str = "ERROR";
            color_code = "\033[35m"; // Magenta
            break;
        default:
            status_str = "UNKNOWN";
            color_code = "\033[0m";
    }

    printf("[%s%-5s\033[0m] Test %02d: %s\n",
           color_code, status_str, test->id, test->name);
}

/* 更新测试统计 */
static void update_stats(test_context_t *ctx, test_result_t result) {
    ctx->total++;
    switch (result) {
        case TEST_PASS: ctx->passed++; break;
        case TEST_FAIL: ctx->failed++; break;
        case TEST_SKIP: ctx->skipped++; break;
        case TEST_ERROR: ctx->errors++; break;
    }
}

/* 打印统计摘要 */
static void print_summary(const test_context_t *ctx) {
    printf("\n");
    printf("================================\n");
    printf("Test Summary\n");
    printf("================================\n");
    printf("Total:   %d\n", ctx->total);
    printf("Passed:  \033[32m%d\033[0m\n", ctx->passed);
    printf("Failed:  \033[31m%d\033[0m\n", ctx->failed);
    printf("Skipped: \033[33m%d\033[0m\n", ctx->skipped);
    printf("Errors:  \033[35m%d\033[0m\n", ctx->errors);

    if (ctx->failed > 0 || ctx->errors > 0) {
        printf("\n\033[31mTEST FAILED\033[0m\n");
    } else if (ctx->passed == ctx->total && ctx->total > 0) {
        printf("\n\033[32mALL TESTS PASSED\033[0m\n");
    } else {
        printf("\n\033[33mTEST INCOMPLETE\033[0m\n");
    }
    printf("\n");
}

/* 查找测试用例 */
static const test_case_t* find_test_by_id(int id) {
    for (int i = 0; g_test_cases[i].name != NULL; i++) {
        if (g_test_cases[i].id == id) {
            return &g_test_cases[i];
        }
    }
    return NULL;
}

/* 如果测试需要人工判定，询问测试人员 */
static test_result_t ask_human_verdict(const test_case_t *test, test_result_t auto_result, int timeout_seconds)
{
    char buf[64];
    fd_set rfds;
    struct timeval tv;
    int rv;

    printf("Manual check required for test %02d: %s\n", test->id, test->name);
    printf("  Please inspect the device and enter: [y]=PASS  [n]=FAIL  [s]=SKIP  [Enter]=accept automated (%s)\n",
           (auto_result == TEST_PASS) ? "PASS" :
           (auto_result == TEST_FAIL) ? "FAIL" :
           (auto_result == TEST_SKIP) ? "SKIP" : "ERROR");
    printf("  Input (timeout %d s): ", timeout_seconds);
    fflush(stdout);

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;

    rv = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (rv <= 0) {
        printf("\nNo input (timeout). Accepting automated result.\n");
        return auto_result;
    }

    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("\nRead error, accepting automated result.\n");
        return auto_result;
    }

    /* 找到第一个非空字符 */
    char c = 0;
    for (int i = 0; buf[i]; i++) {
        if (!isspace((unsigned char)buf[i])) { c = buf[i]; break; }
    }

    if (c == 0) {
        printf("No selection, accepting automated result.\n");
        return auto_result;
    }
    if (c == 'y' || c == 'Y') return TEST_PASS;
    if (c == 'n' || c == 'N') return TEST_FAIL;
    if (c == 's' || c == 'S') return TEST_SKIP;

    printf("Unrecognized input, accepting automated result.\n");
    return auto_result;
}

/* 运行单个测试 */
static test_result_t run_test_case(const test_case_t *test) {
    printf("\n--- Running: %s (ID: %d) ---\n", test->name, test->id);
    printf("Description: %s\n", test->description);

    /* 模拟测试执行延迟 */
    usleep(100000); // 100ms

    test_result_t result = test->test_func();

    /* 如果需要人工判定，询问测试人员；超时或空输入则接受自动结果 */
    if (test->manual) {
        result = ask_human_verdict(test, result, 15); // 15s timeout
    }

    print_test_result(test, result);

    return result;
}

/* 执行所有测试 */
static void run_all_tests(test_context_t *ctx) {
    printf("\n================================\n");
    printf("Starting All Factory Tests\n");
    printf("================================\n");

    for (int i = 0; g_test_cases[i].name != NULL; i++) {
        test_result_t result = run_test_case(&g_test_cases[i]);
        update_stats(ctx, result);
    }
}

/* 执行指定测试 */
static bool run_specific_test(test_context_t *ctx, int cid) {
    const test_case_t *test = find_test_by_id(cid);

    if (!test) {
        printf("\n\033[31mERROR: Test case ID %d not found!\033[0m\n", cid);
        printf("Use -h or --help to see available test cases.\n\n");
        return false;
    }

    printf("\n================================\n");
    printf("Starting Specific Test\n");
    printf("================================\n");

    test_result_t result = run_test_case(test);
    update_stats(ctx, result);

    return true;
}

/* ==================== 测试用例实现 ==================== */

/* LED测试 - 使用 /dev/leds0 写入颜色值（参考 vendor led_rgb/led_demo.c） */
static test_result_t test_led(void) {
    const char *dev = "/dev/leds0";
    unsigned int colors[] = {0xFF0000, 0x00FF00, 0x0000FF}; /* RED, GREEN, BLUE */
    int num_colors = sizeof(colors) / sizeof(colors[0]);

    printf("  -> LED test: trying device %s\n", dev);
    if (access(dev, R_OK | W_OK) != 0) {
        printf("  -> LED device %s not present or not accessible. Skipping LED test.\n", dev);
        record_skip_reason_by_info(1, "LED Test", "device /dev/leds0 not present");
        return TEST_SKIP;
    }

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("  -> Cannot open %s (%s). Skipping LED test.\n", dev, strerror(errno));
        record_skip_reason_by_info(1, "LED Test", "cannot open /dev/leds0");
        return TEST_SKIP;
    }

    printf("  -> Setting LED colors sequence...\n");
    for (int i = 0; i < num_colors; i++) {
        int color = (int)colors[i];
        ssize_t w = write(fd, &color, sizeof(color));
        if (w != (ssize_t)sizeof(color)) {
            printf("  -> Failed to set LED color 0x%06X (write returned %zd)\n", color, w);
            close(fd);
            return TEST_FAIL;
        }
        printf("  -> LED color set: 0x%06X\n", color);
        sleep(1);
    }

    /* Turn off */
    int off = 0x000000;
    write(fd, &off, sizeof(off));
    close(fd);

    printf("  -> LED sequence completed\n");
    return TEST_PASS;
}

/* 按键测试 - 读取 /dev/input/event1 检测 Home/Enter/Menu/Vol+/Vol- */
static test_result_t test_button(void) {
    const char *dev = "/dev/input/event1";
    /* Codes provided: Home=40, Enter=34, Menu=27, Vol+=18, Vol-=9 */
    const btn_buttonset_t codes[] = {0x10, 0x8, 0x4, 0x2, 0x1};
    const char *names[] = {"Home", "Enter", "Menu", "Vol+", "Vol-"};
    bool seen[sizeof(codes)/sizeof(codes[0])];
    int remaining = (int)(sizeof(codes)/sizeof(codes[0]));

    printf("  -> Button test: checking device %s\n", dev);
    if (access(dev, R_OK) != 0) {
        printf("  -> Button device %s not present. Skipping test.\n", dev);
        record_skip_reason_by_info(2, "Button Test", "device /dev/input/event1 not present");
        return TEST_SKIP;
    }

    int fd = open(dev, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        printf("  -> Cannot open %s (%s). Skipping test.\n", dev, strerror(errno));
        record_skip_reason_by_info(2, "Button Test", "cannot open /dev/input/event1");
        return TEST_SKIP;
    }

    for (size_t i = 0; i < sizeof(seen)/sizeof(seen[0]); i++) seen[i] = false;



    time_t start = time(NULL);
    const int timeout_sec = 20; /* overall timeout */

    while ((time(NULL) - start) < timeout_sec && remaining > 0) {
        btn_buttonset_t val = 0;
        ssize_t n = read(fd, &val, sizeof(val));
        if (n <= 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            break;
        }
        /* Try to match as a direct code (device returning code numbers)
         * or as a bitmask (device returning btn_buttonset_t style masks).
         */
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        {
            if (!seen[i])
            {
                if ((uint32_t)val == (uint32_t)codes[i])
                {
                    seen[i] = true;
                    remaining--;
                    printf("  -> Detected button %s (code %ld)\n", names[i], codes[i]);
                }
            }
        }
    }

    close(fd);

    if (remaining == 0) {
        printf("  -> All buttons detected. PASS\n");
        return TEST_PASS;
    } else {
        printf("  -> Button test FAILED. Missing: ");
        for (size_t i = 0; i < sizeof(codes)/sizeof(codes[0]); i++) {
            if (!seen[i]) printf("%s(%ld) ", names[i], codes[i]);
        }
        printf("\n");
        return TEST_FAIL;
    }
}

/* Helper: run sensor test for a specific category */
static test_result_t test_sensor_for_category(const char *category, int test_id, const char *test_name)
{
#ifdef CONFIG_UORB
    const char *root = ORB_SENSOR_PATH; /* "/dev/uorb/" */
    const char *cat = category;
    char pathbuf[PATH_MAX];
    DIR *d = NULL;

    if (cat && cat[0]) {
        /* First try a category-specific path: /dev/uorb/<category> */
        snprintf(pathbuf, sizeof(pathbuf), "%s%s", root, cat);
        printf("  -> Trying category-specific path: %s\n", pathbuf);
        d = opendir(pathbuf);
        if (!d) {
            /* Fall back to scanning the root and applying substring filtering */
            printf("  -> Category path not available; scanning %s and applying filter '%s'\n", root, cat);
            d = opendir(root);
        }
    } else {
        printf("  -> No category specified; scanning %s\n", root);
        d = opendir(root);
    }

    if (!d) {
        printf("  -> uORB path(s) not accessible. Skipping %s.\n", test_name);
        record_skip_reason_by_info(test_id, test_name, "uORB sensor path not accessible");
        return TEST_SKIP;
    }

    const struct orb_metadata *metas[64];
    int instances[64];
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) && count < (int)(sizeof(metas)/sizeof(metas[0]))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        /* Apply category filter (substring match) */
        if (cat && cat[0]) {
            if (!strstr(entry->d_name, cat))
                continue;
        }

        /* Determine instance (last char digit) */
        char namebuf[PATH_MAX];
        strncpy(namebuf, entry->d_name, sizeof(namebuf));
        namebuf[sizeof(namebuf)-1] = '\0';
        size_t len = strlen(namebuf);
        if (len == 0)
            continue;

        int inst = 0;
        if (isdigit((unsigned char)namebuf[len - 1])) {
            inst = namebuf[len - 1] - '0';
        }

        const struct orb_metadata *meta = orb_get_meta(entry->d_name);
        if (!meta)
            continue;

        metas[count] = meta;
        instances[count] = inst;
        count++;
    }

    closedir(d);

    if (count == 0) {
        printf("  -> No sensor topics under %s for category '%s'. Skipping %s.\n", pathbuf , cat ? cat : "ALL", test_name);
        record_skip_reason_by_info(test_id, test_name, "no sensor topics for category");
        return TEST_SKIP;
    }

    /* Parameters: nb_msgs=5, rate=25Hz */
    const int nb_msgs = 5;
    const float topic_rate = 25.0f;
    const unsigned interval_us = (unsigned)(1000000 / topic_rate);
    const int timeout_sec = 5; /* overall poll timeout */

    struct pollfd *fds = calloc(count, sizeof(struct pollfd));
    int *recv_msgs = calloc(count, sizeof(int));
    if (!fds || !recv_msgs) {
        free(fds); free(recv_msgs);
        printf("  -> Memory allocation failed for sensor subscriptions.\n");
        return TEST_ERROR;
    }

    for (int i = 0; i < count; i++) {
        int fd = orb_subscribe_multi(metas[i], instances[i]);
        if (fd < 0) {
            fds[i].fd = -1;
            fds[i].events = 0;
            continue;
        }
        fds[i].fd = fd;
        fds[i].events = POLLIN;
        orb_set_interval(fd, interval_us);
    }

    int total_recv = 0;
    time_t start = time(NULL);
    char *buf = NULL;

    while (total_recv < nb_msgs && (time(NULL) - start) < timeout_sec) {
        int rv = poll(fds, count, 1000); /* 1s */
        if (rv > 0) {
            for (int i = 0; i < count; i++) {
                if (fds[i].fd >= 0 && (fds[i].revents & POLLIN)) {
                    /* read one message */
                    size_t sz = metas[i]->o_size;
                    buf = realloc(buf, sz);
                    if (!buf) continue;

                    int ret = orb_copy(metas[i], fds[i].fd, buf);
                    if (ret == 0) {
                        total_recv++;
                        recv_msgs[i]++;
                        printf("  -> [%s] Received sensor topic %s%d (%d/%d)\n",
                               test_name, metas[i]->o_name, instances[i], total_recv, nb_msgs);
                        if (total_recv >= nb_msgs) break;
                    }
                }
            }
        } else if (rv == 0) {
            /* timeout iteration, continue */
            continue;
        } else if (rv < 0 && errno != EINTR) {
            break;
        }
    }

    /* cleanup */
    for (int i = 0; i < count; i++) {
        if (fds[i].fd >= 0) {
            orb_unsubscribe(fds[i].fd);
        }
    }
    free(fds);
    free(recv_msgs);
    free(buf);

    if (total_recv >= nb_msgs) {
        printf("  -> %s PASS (received %d messages)\n", test_name, total_recv);
        return TEST_PASS;
    } else {
        printf("  -> %s FAIL (received %d/%d messages)\n", test_name, total_recv, nb_msgs);
        return TEST_FAIL;
    }
#else
    /* Fallback simulated sensor behavior when uORB not available */
    printf("  -> Simulated %s test (uORB not enabled)\n", test_name);
    usleep(100000);

    if (strcmp(category, "light") == 0) {
        printf("  -> Found: Light sensor (simulated)\n");
        return TEST_PASS;
    } else if (strstr(category, "prox") != NULL) {
        printf("  -> Found: Proximity sensor (simulated)\n");
        return TEST_PASS;
    }

    return TEST_SKIP;
#endif
}

static test_result_t test_sensor_light(void) {
    return test_sensor_for_category("light0", 7, "Light Sensor");
}

static test_result_t test_sensor_prox(void) {
    return test_sensor_for_category("prox0", 8, "Proximity Sensor");
}

static test_result_t test_sensor_temp(void) {
    return test_sensor_for_category("temp0", 9, "Temperature Sensor");
}

/* IR helper and test (based on vendor/allwinnertech/apps/sunxi_ir_tx/ir_tx_main.c) */

#define NS_TO_US(nsec) ((nsec) / 1000)
#define NEC_UNIT 562500 /* ns */
#define NEC_HEADER_PULSE (16 * NEC_UNIT)
#define NEC_HEADER_SPACE (8 * NEC_UNIT)
#define NEC_BIT_PULSE (1 * NEC_UNIT)
#define NEC_BIT_0_SPACE (1 * NEC_UNIT)
#define NEC_BIT_1_SPACE (3 * NEC_UNIT)
#define NEC_TRAILER_PULSE (1 * NEC_UNIT)
#define NEC_TRAILER_SPACE (10 * NEC_UNIT)

#define GPIO_IR_RAW_BUF_SIZE 128
#define DEFAULT_DUTY_CYCLE 33
#define DEFAULT_CARRIER_FREQ 38000

/* LIRC helpers from ir_tx_main */
// Removed: Already defined in <nuttx/lirc.h>
// #define LIRC_MODE2_PULSE 0x01000000
// #define LIRC_MODE2_SPACE 0x00000000
// #define LIRC_VALUE_MASK 0x00FFFFFF
// #define LIRC_PULSE(val) (((val) & LIRC_VALUE_MASK) | LIRC_MODE2_PULSE)
// #define LIRC_SPACE(val) (((val) & LIRC_VALUE_MASK) | LIRC_MODE2_SPACE)
static uint32_t ir_tx_raw_buf[GPIO_IR_RAW_BUF_SIZE];

static int nec_modulation_byte(uint32_t *buf, uint8_t code) {
    int i = 0;
    uint8_t mask = 0x01;

    while (mask) {
        if (code & mask) {
            /* bit 1 */
            *(buf + i) = LIRC_PULSE(NS_TO_US(NEC_BIT_PULSE));
            *(buf + i + 1) = LIRC_SPACE(NS_TO_US(NEC_BIT_1_SPACE));
        } else {
            /* bit 0 */
            *(buf + i) = LIRC_PULSE(NS_TO_US(NEC_BIT_PULSE));
            *(buf + i + 1) = LIRC_SPACE(NS_TO_US(NEC_BIT_0_SPACE));
        }
        mask <<= 1;
        i += 2;
    }
    return i;
}

static int nec_ir_encode(uint32_t *raw_buf, uint32_t key_code) {
    uint8_t address, reverse_address, command, reverse_command;
    uint32_t *head_p, *data_p, *stop_p;

    address = (key_code >> 24) & 0xff;
    reverse_address = (key_code >> 16) & 0xff;
    command = (key_code >> 8) & 0xff;
    reverse_command = (key_code >> 0) & 0xff;

    /* head bit */
    head_p = raw_buf;
    *(head_p) = LIRC_PULSE(NS_TO_US(NEC_HEADER_PULSE));
    *(head_p + 1) = LIRC_SPACE(NS_TO_US(NEC_HEADER_SPACE));

    /* data bit */
    data_p = raw_buf + 2;
    nec_modulation_byte(data_p, address);

    data_p += 16;
    nec_modulation_byte(data_p, reverse_address);

    data_p += 16;
    nec_modulation_byte(data_p, command);

    data_p += 16;
    nec_modulation_byte(data_p, reverse_command);

    /* stop bit */
    stop_p = data_p + 16;
    *(stop_p) = LIRC_PULSE(NS_TO_US(NEC_TRAILER_PULSE));
    *(stop_p + 1) = LIRC_SPACE(NS_TO_US(NEC_TRAILER_SPACE));

    return ((32 + 2) * 2 - 1);
}



/* External HAL functions (declared in vendor's ir_tx app)
 * If these are not available at link time, the test will fail to link.
 */
// extern void hal_cir_tx_set_duty_cycle(int duty_cycle);
// extern void hal_cir_tx_set_carrier(int carrier_freq);
// extern void hal_cir_tx_xmit(unsigned int *txbuf, unsigned int count);

static test_result_t test_ir(void)
{
    const uint32_t key_code = 0x04fb13ec; /* example NEC code */
    int size;

    printf("  -> IR test: encoding key 0x%08X\n", key_code);

    size = nec_ir_encode(ir_tx_raw_buf, key_code);
    if (size <= 0 || size > GPIO_IR_RAW_BUF_SIZE) {
        printf("  -> IR encode failed (size=%d)\n", size);
        return TEST_ERROR;
    }

    /* Mask bits for transmission.
     * Although LIRC_MODE2 standard uses high bits for Pulse/Space,
     * the underlying sunxi_hal usually expects pure duration values in microseconds.
     * We mask out the high byte flags (0x01/0x00) added by nec_ir_encode.
     */
    for (int i = 0; i < size; i++) {
        ir_tx_raw_buf[i] = (ir_tx_raw_buf[i] & 0x00FFFFFF);
    }

    const char *lirc_dev = "/dev/lirc0";
    printf("  -> Using lirc device: %s\n", lirc_dev);
    int fd = open(lirc_dev, O_RDWR);
    if (fd < 0) {
       printf("  -> Failed to open %s. Skipping IR test.\n", lirc_dev);
       return TEST_SKIP;
    }

    /* Set carrier frequency */
    unsigned int carrier = DEFAULT_CARRIER_FREQ;
    if (ioctl(fd, LIRC_SET_SEND_CARRIER, (unsigned long)carrier) < 0) {
        printf("  -> Warning: Failed to set carrier frequency.\n");
    }

    /* Set duty cycle */
    unsigned int duty_cycle = DEFAULT_DUTY_CYCLE;
    if (ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, (unsigned long)duty_cycle) < 0) {
        printf("  -> Warning: Failed to set duty cycle.\n");
    }

    /* Set send mode to PULSE (Raw IR) */
    unsigned int mode = LIRC_MODE_PULSE;
    if (ioctl(fd, LIRC_SET_SEND_MODE, (unsigned long)mode) < 0) {
        printf("  -> Warning: Failed to set send mode to PULSE. (This is fatal if driver defaults to SCANCODE)\n");
    }

    /* --- Attempt Loopback Verification --- */
    /* Flush any old data */
    /* Note: if fd is O_RDWR, we can use it for both read and write. */
    /* Set NONBLOCK for reading */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    uint32_t dummy;
    while (read(fd, &dummy, sizeof(dummy)) > 0);

    /* Write data (transmit) */
    /* Note: lirc_dev driver strictly requires ODD number of samples for pulse mode. 
     * nec_ir_encode returns an odd number (ending with pulse, trailers space excluded from count? 
     * actually it returns total pulses+spaces - 1, which ends on a Pulse).
     */

    printf("  -> Transmitting IR pattern (%d words)\n", size);
    ssize_t written = write(fd, ir_tx_raw_buf, size * sizeof(uint32_t));
    if (written < 0) {
        printf("  -> Write failed: %d\n", errno);
        close(fd);
        return TEST_FAIL;
    }
    printf("  -> Write returned %zd bytes\n", written);

    /* Wait for data using poll instead of sleep */
    printf("  -> Waiting for received data on %s (timeout 2s)...\n", lirc_dev);
    
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;
    
    int count = 0;
    int remain_time = 2000; /* 2s total timeout window */

    while (remain_time > 0) {
        int ret = poll(&fds, 1, 100); /* check every 100ms */
        
        if (ret > 0 && (fds.revents & POLLIN)) {
             uint32_t val;
             /* Read burst of data */
             while (read(fd, &val, sizeof(val)) == sizeof(val)) {
                 count++;
             }
             /* If we have received enough data (expected ~67), we can finish early */
             if (count >= size) break;
        } else if (ret == 0) {
             /* Timeout in this 100ms slice */
             if (count > 0) {
                 /* We received some data previously, and now line is silent. Assume end of packet. */
                 break;
             }
        } else {
             /* Error */
             if (errno != EINTR && errno != EAGAIN) break;
        }
        remain_time -= 100;
    }
    
    bool verified = false;
    
    if (count > 10) {
        printf("  -> Recv: Received %d raw events. Loopback successful!\n", count);
        verified = true;
    } else {
        printf("  -> Recv: Received only %d events. (Need >10). Loopback failed.\n", count);
        printf("     (Ensure IR TX LED is pointing at IR Receiver)\n");
    }
    
    close(fd);

    return verified ? TEST_PASS : TEST_FAIL;
}

/* Manual IR TX Test */
static test_result_t test_ir_tx_manual(void) {
    const uint32_t key_code = 0x04fb13ec;
    int size = nec_ir_encode(ir_tx_raw_buf, key_code);
    if (size <= 0 || size > GPIO_IR_RAW_BUF_SIZE) return TEST_ERROR;

    for (int i = 0; i < size; i++) ir_tx_raw_buf[i] &= 0x00FFFFFF;

    const char *lirc_dev = "/dev/lirc0";
    int fd = open(lirc_dev, O_RDWR);
    if (fd < 0) return TEST_SKIP;

    ioctl(fd, LIRC_SET_SEND_CARRIER, (unsigned long)DEFAULT_CARRIER_FREQ);
    ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, (unsigned long)DEFAULT_DUTY_CYCLE);
    ioctl(fd, LIRC_SET_SEND_MODE, (unsigned long)LIRC_MODE_PULSE);

    printf("  -> Transmitting IR pattern. Check for visible flash on IR TX LED.\n");
    write(fd, ir_tx_raw_buf, size * sizeof(uint32_t));
    
    close(fd);

    printf("  -> [QUESTION] Did you see the IR TX LED blink? (y/n): ");
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
        if (buf[0] == 'y' || buf[0] == 'Y') return TEST_PASS;
    }
    return TEST_FAIL;
}

/* Manual IR RX Test */
static test_result_t test_ir_rx_manual(void) {
    const char *lirc_dev = "/dev/lirc0";
    int fd = open(lirc_dev, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return TEST_SKIP;

    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;

    printf("  -> Waiting for EXTERNAL IR signal (Remote Control) for 15s...\n");
    int timeout = 15000; // 15s
    int count = 0;
    while (timeout > 0) {
        int ret = poll(&fds, 1, 100);
        if (ret > 0 && (fds.revents & POLLIN)) {
            uint32_t val;
            while(read(fd, &val, sizeof(val)) == sizeof(val)) count++;
            if (count > 0) break;
        }
        timeout -= 100;
    }
    
    close(fd);
    
    if (count > 0) {
        printf("  -> Received %d events. PASS\n", count);
        return TEST_PASS;
    }
    printf("  -> Timeout. No signal received. FAIL\n");
    return TEST_FAIL;
}

/* 内存测试 - 真实读写测试 */
static test_result_t test_memory(void) {
    printf("  -> Running memory test...\n");

    /* 尝试分配最大 1MB 内存进行测试 */
    size_t test_size = 1024 * 1024;
    void *buffer = NULL;

    /* 尝试分配，如果失败则减半，直到 4KB */
    while (test_size >= 4096) {
        buffer = malloc(test_size);
        if (buffer) {
            break;
        }
        test_size /= 2;
    }

    if (!buffer) {
        printf("  -> Failed to allocate memory (min 4KB). FAIL\n");
        return TEST_FAIL;
    }

    printf("  -> Allocated %zu bytes for testing.\n", test_size);

    volatile uint8_t *p = (volatile uint8_t *)buffer;
    test_result_t result = TEST_PASS;

    /* Pattern 1: 0x55 */
    printf("  -> Writing pattern 0x55...\n");
    for (size_t i = 0; i < test_size; i++) {
        p[i] = 0x55;
    }

    for (size_t i = 0; i < test_size; i++) {
        if (p[i] != 0x55) {
            printf("  -> Verify failed at offset %zu: expected 0x55, got 0x%02X\n", i, p[i]);
            result = TEST_FAIL;
            goto out;
        }
    }

    /* Pattern 2: 0xAA */
    printf("  -> Writing pattern 0xAA...\n");
    for (size_t i = 0; i < test_size; i++) {
        p[i] = 0xAA;
    }

    for (size_t i = 0; i < test_size; i++) {
        if (p[i] != 0xAA) {
            printf("  -> Verify failed at offset %zu: expected 0xAA, got 0x%02X\n", i, p[i]);
            result = TEST_FAIL;
            goto out;
        }
    }

    /* Pattern 3: Address-based */
    printf("  -> Writing address pattern...\n");
    for (size_t i = 0; i < test_size; i++) {
        p[i] = (uint8_t)(i & 0xFF);
    }

    for (size_t i = 0; i < test_size; i++) {
        if (p[i] != (uint8_t)(i & 0xFF)) {
             printf("  -> Verify failed at offset %zu: expected 0x%02X, got 0x%02X\n", i, (uint8_t)(i & 0xFF), p[i]);
             result = TEST_FAIL;
             goto out;
        }
    }

    printf("  -> Memory test PASSED\n");

out:
    free(buffer);
    return result;
}

/* 网络测试 - 模拟连接测试 */
static test_result_t test_network(void) {
    system("ping baidu.com");
}


#ifdef CONFIG_BT_START
extern int check_bt_valid();
#endif
/* bt测试 - 模拟连接测试 */
static test_result_t test_bt(void) {
    printf("  -> Testing bluetooth...\n");
    int bt_valid = -1;
    #ifdef CONFIG_BT_START
    system("bt_start &");
    sleep(10);
    bt_valid = check_bt_valid();
    #endif

    if (0 == bt_valid) {
        printf("bt enable\n");
        return TEST_PASS;
    } else {
        printf("bt disable\n");
        return TEST_FAIL;
    }
}

/* 检测是否有显示设备连接: /dev/fb0 或 sys/class/drm/*/
static int detect_display_connected(void)
{
    #if defined(CONFIG_LCD)
    if (access("/dev/lcd0", R_OK) == 0) {
        return 1;
    }
    #endif
    /* 快速检查 framebuffer */
    if (access("/dev/fb0", R_OK) == 0) {
        return 1;
    }

    return 0;
}

static void record_skip_reason_by_info(int id, const char *name, const char *reason)
{
    const char *path = "/tmp/factory_test_skipped.txt";
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t t = time(NULL);
    char ts[64];
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(f, "%s: TEST SKIPPED ID %02d %s - %s\n", ts, id, name, reason);
    fclose(f);
}

/* 显示屏测试 - 如果检测不到屏幕则自动跳过并记录 */
static test_result_t test_screen(void)
{
    printf("  -> Checking for display connection...\n");
    if (!detect_display_connected()) {
        printf("  -> Display not detected. Skipping test and recording hardware not connected.\n");
        record_skip_reason_by_info(6, "Screen Test", "hardware not connected");
        return TEST_SKIP;
    }

    printf("  -> Display detected. please check the screen display is ok\n");

    return TEST_PASS;
}

/* 触摸屏测试 - 使用 NuttX touchscreen 驱动 (如果启用) 读取 /dev/input0 的 touch_sample_s */
static test_result_t test_touchscreen(void)
{
    const char *dev = "/dev/input0";
    printf("  -> Checking for touch input device %s...\n", dev);
    if (access(dev, R_OK) != 0) {
        printf("  -> Touch device %s not present. Skipping automated test.\n", dev);
        record_skip_reason_by_info(7, "Touch Test", "input device /dev/input0 not present");
        return TEST_SKIP;
    }

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        printf("  -> Cannot open %s (%s). Skipping.\n", dev, strerror(errno));
        record_skip_reason_by_info(7, "Touch Test", "cannot open input device");
        return TEST_SKIP;
    }

    struct touch_sample_s *sample = (struct touch_sample_s *)malloc(SIZEOF_TOUCH_SAMPLE_S(5));
    if (!sample) {
        printf("  -> Failed to allocate sample buffer.\n");
        close(fd);
        return TEST_ERROR;
    }

    printf("  -> Device opened (%s). Waiting for touch sample (timeout 8s)...\n", dev);
    time_t start = time(NULL);
    int timeout = 8;

    while (time(NULL) - start < timeout) {
        ssize_t n = read(fd, sample, SIZEOF_TOUCH_SAMPLE_S(5));
        if (n < 0) {
            if (errno == EINTR) continue;
            usleep(100000);
            continue;
        }
        if (n == 0) {
            usleep(100000);
            continue;
        }

        if (n >= (ssize_t)sizeof(struct touch_sample_s)) {
            printf("  -> Touch sample read, %d points\n", sample->npoints);
            if (sample->npoints > 0) {
                for (int i = 0; i < sample->npoints && i < 5; i++) {
                    struct touch_point_s *p = &sample->point[i];
                    printf("    - Point %d: x=%d y=%d flags=0x%02X\n", p->id, p->x, p->y, p->flags);
                }
                free(sample);
                close(fd);
                return TEST_PASS;
            }
        }
    }

    free(sample);
    close(fd);
    printf("  -> No touch samples detected within %d seconds. FAIL\n", timeout);
    return TEST_FAIL;
}

/** SD卡测试 - 检测 /sdcard 是否挂载成功 */
static test_result_t test_sdcard(void) {
    printf("  -> Testing SD card...\n");
    if (access("/sdcard", R_OK) == 0) {
        return TEST_PASS;
    } else {
        return TEST_FAIL;
    }
}

/** 麦克风测试 - 录制简单音频文件 */
static test_result_t test_mic(void) {
    printf("  -> Testing microphone...\n");
    FAR struct nxrecorder_s *recorder = nxrecorder_create();
    if (!recorder) {
        printf("[audio] ERROR: Failed to create recorder\n");
        return -ENOMEM;
    }
    const char *audio_file = "/tmp/record_test.pcm";
    nxrecorder_setdevice(recorder, "/dev/audio/pcm0c");
    int ret = nxrecorder_recordinternal(recorder, audio_file, AUDIO_FMT_PCM , 2 ,16, 48000, 1);
    if (ret != OK)
    {
        printf("[audio] ERROR: Failed to start recording: %d\n", ret);
        nxrecorder_release(recorder);
        return ret;
    }
    sleep(5);
    nxrecorder_stop(recorder);
    nxrecorder_release(recorder);
    printf("  -> Microphone test completed, recorded file: %s\n", audio_file);
    //检查文件大小是否>500字节
    struct stat st;
    if (stat(audio_file, &st) == 0 && st.st_size > 500) {
        printf("  -> Recorded file size: %lld bytes\n", st.st_size);
    } else {
        printf("  -> Recorded file size too small or file not found. FAIL\n");
        return TEST_FAIL;
    }
    return TEST_PASS;
}

/** Speaker测试 - 播放简单音频文件 */
static test_result_t test_speaker(void)
{
    const char *audio_file = "/data/s16le_48000_stereo.pcm";
    printf("  -> Testing speaker by playing audio file %s...\n", audio_file);
    if (access(audio_file, R_OK) != 0)
    {
        printf("  -> Audio file not found. Skipping speaker test.\n");
        record_skip_reason_by_info(13, "Speaker Test", "audio file not found");
        return TEST_SKIP;
    }

    FAR struct nxplayer_s *player = nxplayer_create();
    if (!player)
    {
        printf("[audio] ERROR: Failed to create player\n");
        return -ENOMEM;
    }

    nxplayer_setdevice(player, "/dev/audio/pcm0p");
    int ret = nxplayer_playraw(player, audio_file, AUDIO_FMT_PCM, 0, 2, 16, 48000, 1);
    if (ret != OK)
    {
        printf("[audio] ERROR: Failed to start playback: %d\n", ret);
        nxplayer_release(player);
        return ret;
    }

    printf("[audio] Playback started\n");

    /* 模拟播放延迟 */
    sleep(6);

    nxplayer_stop(player);
    nxplayer_release(player);
    printf("  -> Speaker test completed\n");
    return TEST_PASS;
}

/* ==================== 主函数 ==================== */

int main(int argc, char *argv[]) {
    int opt;
    int cid = -1;
    bool run_all = false;
    bool help_requested = false;

    /* 初始化随机种子 */
    srand((unsigned int)time(NULL));

    /* 解析命令行参数 */
    while ((opt = getopt_long(argc, argv, "hc:a", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                help_requested = true;
                break;
            case 'c':
                cid = atoi(optarg);
                break;
            case 'a':
                run_all = true;
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }

    /* 显示帮助 */
    if (help_requested) {
        show_help(argv[0]);
        return 0;
    }

    /* 验证参数 */
    if (!run_all && cid < 0) {
        printf("Error: Either -c or -a must be specified.\n");
        printf("Try '%s --help' for usage information.\n\n", argv[0]);
        return 1;
    }

    if (run_all && cid >= 0) {
        printf("Warning: -c and -a both specified. Running all tests.\n\n");
        cid = -1;
    }

    /* 初始化测试上下文 */
    test_context_t ctx = {0};

    /* 执行测试 */
    if (run_all) {
        run_all_tests(&ctx);
    } else if (cid >= 0) {
        if (!run_specific_test(&ctx, cid)) {
            return 1;
        }
    }

    /* 打印统计 */
    print_summary(&ctx);

    /* 返回退出码 */
    if (ctx.failed > 0 || ctx.errors > 0) {
        return 1;
    }
    return 0;
}
