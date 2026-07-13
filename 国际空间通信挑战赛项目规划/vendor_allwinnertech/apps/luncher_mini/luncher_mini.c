/****************************************************************************
 * apps/vendor/allwinnertech/apps/luncher_mini/luncher_mini.c
 * Mini launcher application for OpenVela with LED control functionality.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   `http://www.apache.org/licenses/LICENSE-2.0` 
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_LUNCHER_MINI_APP

#include <unistd.h>
#include <sys/boardctl.h>
#include <stdio.h>
#include <string.h>
#include <lvgl/lvgl.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h> 

/* 包含LED控制库头文件 */
#include "lv_demo_panel_rgb_control.h"

/* 包含传感器相关头文件 */
#include <poll.h>
#include <sensor/temp.h>
#include <sensor/humi.h>
#include <sensor/prox.h>
#include <uORB/uORB.h>
#include <syslog.h> 

/* 条件包含 libuv 头文件 */
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#include <uv.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Should we perform board-specific driver initialization? There are two
 * ways that board initialization can occur:  1) automatically via
 * board_late_initialize() during bootupif CONFIG_BOARD_LATE_INITIALIZE
 * or 2).
 * via a call to boardctl() if the interface is enabled
 * (CONFIG_BOARDCTL=y).
 * If this task is running as an NSH built-in application, then that
 * initialization has probably already been performed otherwise we do it
 * here.
 */

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t uv_info;
  void *data;

  uv_loop_init(loop);

  lv_memset(&uv_info, 0, sizeof(uv_info));
  uv_info.loop = loop;
  uv_info.disp = result->disp;
  uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  uv_info.uindev = result->utouch_indev;
#endif

  data = lv_nuttx_uv_init(&uv_info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}
#endif

/****************************************************************************
 * 配置参数
 ****************************************************************************/
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240

/****************************************************************************
 * 传感器相关定义和结构体
 ****************************************************************************/
#define LV_DEMO_POLLFD_NUM 3 /* 保留温度、湿度和距离传感器 */

/* 传感器订阅者结构体 */
typedef struct {
  int temperature_sub;
  int humidity_sub;
  int prox_sub;
  struct pollfd fds[LV_DEMO_POLLFD_NUM];
  bool initialized;
} sensor_subscriber;

static sensor_subscriber sensor_sub = {
    .temperature_sub = -1,
    .humidity_sub = -1,
    .prox_sub = -1,
    .initialized = false
};

/* 传感器更新选项枚举 */
typedef enum {
    LV_DEMO_UPDATE_TEMPERATURE = 0x01,
    LV_DEMO_UPDATE_HUMIDITY    = 0x02,
    LV_DEMO_UPDATE_PROX        = 0x04,
    LV_DEMO_UPDATE_ALL         = 0x07  // 所有传感器的位或
} lv_demo_sensor_select_t;

/****************************************************************************
 * 全局变量声明
 ****************************************************************************/

/* UI相关变量 */
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *window[4];
static uint8_t time_counter = 0;
static lv_obj_t *bg_img;
static lv_obj_t *temp_label;      // 温度显示标签
static lv_obj_t *humidity_label;  // 湿度显示标签
static lv_obj_t *prox_label;      // 接近传感器显示标签

/* 灯光控制UI变量 */
static lv_obj_t *light_window = NULL;
static lv_obj_t *light_switch = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;
static bool light_initialized = false;

/* Gamble信息UI变量 */
static lv_obj_t *gamble_window = NULL;

/* 中文支持字体变量 */
static lv_font_t *g_misans_normal_11;
static lv_font_t *g_misans_normal_12;
static lv_font_t *g_misans_normal_16;

/* LED状态 */
static bool g_led_is_on = false;
static int32_t g_led_brightness = 100;

/* 时间和日期主题 */
static lv_subject_t hour_subject;
static lv_subject_t minute_subject;
static lv_subject_t second_subject;
static lv_subject_t week_day_name_subject;
static lv_subject_t month_day_subject;
static lv_subject_t month_name_subject;

/* 传感器主题 */
static lv_subject_t temperature_subject;  // 温度
static lv_subject_t humidity_subject;     // 湿度
static lv_subject_t prox_subject;        // 接近传感器

/****************************************************************************
 * 函数声明
 ****************************************************************************/

/* 主界面函数 */
static void create_main_screen(void);
static lv_obj_t* load_background_image(lv_obj_t *parent);
static void restore_color_cb(lv_timer_t *timer);
static void window_click_cb(lv_event_t *e);

/* 时间相关函数 */
static void update_time_cb(lv_timer_t *timer);
static void time_observer_cb(lv_observer_t *observer, lv_subject_t *subject);
static void date_observer_cb(lv_observer_t *observer, lv_subject_t *subject);

/* 灯光控制函数 */
static void create_light_control_window(void);
static void light_switch_event_cb(lv_event_t *e);
static void brightness_slider_event_cb(lv_event_t *e);
static void close_light_window_cb(lv_event_t *e);

/* Gamble信息函数 */
static void create_gamble_info_window(void);
static void close_gamble_window_cb(lv_event_t *e);

/* LED控制适配函数 */
static led_error_t led_adapter_init(void);
static led_error_t led_adapter_deinit(void);
static led_error_t led_adapter_on(void);
static led_error_t led_adapter_off(void);
static led_error_t led_adapter_set_brightness(int32_t brightness);
static void led_adapter_diagnose(void);

/* 传感器相关函数 */
static int lv_demo_init_sensor_subscriptions(void);
static void lv_demo_cleanup_sensor_subscriptions(void);
static int lv_demo_update_sensor_data(lv_demo_sensor_select_t sensors_to_update, void *ctx);
static void temperature_observer_cb(lv_observer_t *observer, lv_subject_t *subject);
static void humidity_observer_cb(lv_observer_t *observer, lv_subject_t *subject);
static void prox_observer_cb(lv_observer_t *observer, lv_subject_t *subject);
static void update_sensor_cb(lv_timer_t *timer);
static int init_sensors(void);

/* 字体初始化函数 */
static void init_fonts(void);

/****************************************************************************
 * LED控制适配函数实现
 ****************************************************************************/

static led_error_t led_adapter_init(void)
{
    led_error_t err = led_controller_init();
    if (err != LED_SUCCESS) {
        LV_LOG_ERROR("LED controller init failed: %s",
                     led_get_error_string(err));
        return err;
    }
    /* 初始化为关闭状态，白色，100%亮度 */
    g_led_is_on = false;
    g_led_brightness = 100;
    /* 设置初始颜色但不开启 */
    led_set_color(LED_COLOR_WHITE);
    led_set_brightness(g_led_brightness);
    LV_LOG_INFO("LED adapter initialized (brightness=%d%%)", g_led_brightness);
    return LED_SUCCESS;
}

static led_error_t led_adapter_deinit(void)
{
    /* 先关闭LED */
    led_adapter_off();
    /* 然后清理资源 */
    led_error_t err = led_controller_deinit();
    if (err != LED_SUCCESS) {
        LV_LOG_ERROR("LED adapter deinit error: %s", led_get_error_string(err));
    }
    LV_LOG_INFO("LED adapter deinitialized");
    return err;
}

static led_error_t led_adapter_on(void)
{
    led_error_t err = led_on();
    if (err == LED_SUCCESS) {
        g_led_is_on = true;
        LV_LOG_INFO("LED adapter: ON (brightness=%d%%)", g_led_brightness);
    } else {
        LV_LOG_ERROR("LED adapter: Failed to turn ON: %s", led_get_error_string(err));
    }
    return err;
}

static led_error_t led_adapter_off(void)
{
    led_error_t err = led_off();
    if (err == LED_SUCCESS) {
        g_led_is_on = false;
        LV_LOG_INFO("LED adapter: OFF");
    } else {
        LV_LOG_ERROR("LED adapter: Failed to turn OFF: %s", led_get_error_string(err));
    }
    return err;
}

static led_error_t led_adapter_set_brightness(int32_t brightness)
{
    if (brightness < 0 || brightness > 100) {
        LV_LOG_ERROR("LED adapter: Invalid brightness %d", brightness);
        return LED_ERROR_INVALID_PARAM;
    }
    g_led_brightness = brightness;
    /* 应用亮度设置 */
    led_error_t err = led_set_brightness(brightness);
    if (err == LED_SUCCESS) {
        LV_LOG_INFO("LED adapter: Brightness set to %d%%", brightness);
        
        /* 如果LED当前是开启的，需要重新应用设置 */
        if (g_led_is_on) {
            /* 临时关闭再开启以应用新亮度 */
            led_off();
            usleep(10000);  // 10ms延迟
            led_on();
        }
    } else {
        LV_LOG_ERROR("LED adapter: Failed to set brightness: %s", 
                     led_get_error_string(err));
    }
    return err;
}

static void led_adapter_diagnose(void)
{
    LV_LOG_INFO("=== LED Adapter Diagnosis ===");
    /* 检查初始化状态 */
    bool led_state = false;
    led_is_on(&led_state);
    LV_LOG_INFO("LED is on: %s", led_state ? "YES" : "NO");
    LV_LOG_INFO("Current brightness: %d%%", g_led_brightness);
    /* 获取当前颜色信息 */
    led_status_t status;
    if (led_get_status(&status) == LED_SUCCESS) {
        LV_LOG_INFO("Current color: 0x%06X (%s)",
                    status.color, led_get_color_name(status.color));
        LV_LOG_INFO("Current mode: %d", status.mode);
    }
    LV_LOG_INFO("=== End Diagnosis ===");
}

/****************************************************************************
 * 字体初始化函数
 ****************************************************************************/
static void init_fonts(void)
{
    #ifdef LV_USE_FREETYPE
    /* 加载MiSans-Normal.ttf字体 */
    g_misans_normal_11 = lv_freetype_font_create("/resource/fonts/MiSans-Normal.ttf",
                                               LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 11,
                                               LV_FREETYPE_FONT_STYLE_NORMAL);
    g_misans_normal_12 = lv_freetype_font_create("/resource/fonts/MiSans-Normal.ttf",
                                               LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
                                               LV_FREETYPE_FONT_STYLE_NORMAL);
    g_misans_normal_16 = lv_freetype_font_create("/resource/fonts/MiSans-Normal.ttf",
                                               LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16,
                                               LV_FREETYPE_FONT_STYLE_NORMAL);
    /* 检查字体加载是否成功，如果失败则回退到默认字体 */
    if(g_misans_normal_11 == NULL) {
        g_misans_normal_11 = &lv_font_montserrat_12;
        LV_LOG_WARN("Failed to load MiSans-Normal 11, fallback to default font");
    }
    if(g_misans_normal_12 == NULL) {
        g_misans_normal_12 = &lv_font_montserrat_12;
        LV_LOG_WARN("Failed to load MiSans-Normal 12, fallback to default font");
    }
    if(g_misans_normal_16 == NULL) {
        g_misans_normal_16 = &lv_font_montserrat_16;
        LV_LOG_WARN("Failed to load MiSans-Normal 16, fallback to default font");
    }
    #else
    /* 如果不支持FreeType，则使用cjk中文支持字体 */
    g_misans_normal_11 = &lv_font_simsun_16_cjk;
    g_misans_normal_12 = &lv_font_simsun_16_cjk;
    g_misans_normal_16 = &lv_font_simsun_16_cjk;
    #endif
}

/****************************************************************************
 * 更新时间定时器回调
 ****************************************************************************/
static void update_time_cb(lv_timer_t *timer)
{
    /* 获取系统实时时间 */
    time_t now;
    struct tm *utc_time;
    struct tm local_time;
    time(&now);
    utc_time = gmtime(&now);
    if (utc_time == NULL) {
        LV_LOG_ERROR("Failed to get UTC time");
        return;
    }
    /* 复制UTC时间到本地时间结构 */
    local_time = *utc_time;
    /* 设置时区偏移量，这里为东八区 (+8小时) */
    local_time.tm_hour += 8;
    /* 调整可能的日期变化 */
    mktime(&local_time);
    /* 添加调试信息，查看获取的时间 */
    LV_LOG_INFO("Current time: %d:%d:%d", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    /* 更新时间主题 */
    lv_subject_set_int(&hour_subject, local_time.tm_hour);
    lv_subject_set_int(&minute_subject, local_time.tm_min);
    lv_subject_set_int(&second_subject, local_time.tm_sec);
    /* 更新日期显示 */
    static const char *week_day_names[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    static const char *month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    uint8_t week_day = local_time.tm_wday;
    uint8_t month_day = local_time.tm_mday;
    uint8_t month = local_time.tm_mon;
    if (week_day < 7 && month < 12) {
        lv_subject_set_pointer(&week_day_name_subject, (void *)week_day_names[week_day]);
        lv_subject_set_int(&month_day_subject, month_day);
        lv_subject_set_pointer(&month_name_subject, (void *)month_names[month]);
    } else {
        LV_LOG_ERROR("Invalid time values: week_day=%d, month=%d", week_day, month);
    }
}

/****************************************************************************
 * 时间观察者回调函数
 ****************************************************************************/
static void time_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(subject);
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%02d:%02d:%02d", 
                lv_subject_get_int(&hour_subject), 
                lv_subject_get_int(&minute_subject),
                lv_subject_get_int(&second_subject));
    
    lv_label_set_text(label, buf);
}

/****************************************************************************
 * 传感器初始化函数
 ****************************************************************************/
static int lv_demo_init_sensor_subscriptions(void) {
  LV_LOG_INFO("Initializing temperature and humidity sensor subscriptions...");
  int pollfd_count = 0;
  bool any_success = false;
  /* 初始化温度传感器订阅 */
  sensor_sub.temperature_sub = orb_subscribe_multi(ORB_ID(sensor_temp), 0);
  if (sensor_sub.temperature_sub < 0) {
    LV_LOG_ERROR("Failed to subscribe to temperature topic");
  } else {
    LV_LOG_INFO("Temperature subscription initialized: fd=%d", sensor_sub.temperature_sub);
    /* 初始化温度传感器的pollfd */
    sensor_sub.fds[pollfd_count].fd = sensor_sub.temperature_sub;
    sensor_sub.fds[pollfd_count].events = POLLIN;
    pollfd_count++;
    any_success = true;
  }
  /* 初始化湿度传感器订阅 */
  sensor_sub.humidity_sub = orb_subscribe_multi(ORB_ID(sensor_humi), 0);
  if (sensor_sub.humidity_sub < 0) {
    LV_LOG_ERROR("Failed to subscribe to humidity topic");
  } else {
    LV_LOG_INFO("Humidity subscription initialized: fd=%d", sensor_sub.humidity_sub);
    /* 初始化湿度传感器的pollfd */
    sensor_sub.fds[pollfd_count].fd = sensor_sub.humidity_sub;
    sensor_sub.fds[pollfd_count].events = POLLIN;
    pollfd_count++;
    any_success = true;
  }
  /* 初始化距离传感器订阅 */
  sensor_sub.prox_sub = orb_subscribe_multi(ORB_ID(sensor_prox),0);
  if (sensor_sub.prox_sub < 0) {
    LV_LOG_ERROR("Failed to subscribe to prox_sub topic");
  } else {
    LV_LOG_INFO("Prox subscription initialized: fd=%d", sensor_sub.prox_sub);
    /* 初始化距离传感器的pollfd */
    sensor_sub.fds[pollfd_count].fd = sensor_sub.prox_sub;
    sensor_sub.fds[pollfd_count].events = POLLIN;
    pollfd_count++;
    any_success = true;
  }
  /* 初始化剩余的pollfd为无效值 */
  for (int i = pollfd_count; i < LV_DEMO_POLLFD_NUM; i++) {
    sensor_sub.fds[i].fd = -1;
    sensor_sub.fds[i].events = 0;
  }
  if (any_success) {
    sensor_sub.initialized = true;
    LV_LOG_INFO("Sensor subscriptions initialized successfully: %d sensors available", pollfd_count);
    return 0;
  } else {
    LV_LOG_ERROR("Failed to initialize any sensor subscriptions");
    return -1;
  }
}

/****************************************************************************
 * 传感器数据更新函数
 ****************************************************************************/
static int lv_demo_update_sensor_data(lv_demo_sensor_select_t sensors_to_update, void *ctx) {
    LV_UNUSED(ctx); /* 如果不需要上下文，可以忽略 */
    if (!sensor_sub.initialized) {
        LV_LOG_WARN("Sensor subscriptions not initialized");
        return -1;
    }
    /* 只poll有效的文件描述符 */
    int valid_fd_count = 0;
    for (int i = 0; i < LV_DEMO_POLLFD_NUM; i++) {
        if (sensor_sub.fds[i].fd != -1) {
            valid_fd_count++;
        }
    }
    if (valid_fd_count == 0) {
        LV_LOG_WARN("No valid sensor file descriptors available");
        return 0;
    }
    int ret = poll(sensor_sub.fds, LV_DEMO_POLLFD_NUM, 100);
    if (ret < 0) {
        LV_LOG_ERROR("Failed to poll sensor data: %d", ret);
        return -1;
    }
    if (ret == 0) {
        /* 没有可读数据是正常的，不是错误 */
        return 0;
    }
    int updated = 0;
    /* 更新温度数据 */
    if ((sensors_to_update & LV_DEMO_UPDATE_TEMPERATURE) &&
        (sensor_sub.temperature_sub >= 0) &&
        (sensor_sub.fds[0].revents & POLLIN)) {
        struct sensor_temp temp_data;
        if (orb_copy(ORB_ID(sensor_temp), sensor_sub.temperature_sub, &temp_data) == OK) {
            /* 直接更新全局温度主题 */
            lv_subject_set_int(&temperature_subject, (int)(temp_data.temperature * 10));
            LV_LOG_INFO("Temperature updated: %.1f°C", temp_data.temperature);
            updated++;
        } else {
            LV_LOG_WARN("Failed to copy temperature data");
        }
    }
    /* 更新湿度数据 */
    if ((sensors_to_update & LV_DEMO_UPDATE_HUMIDITY) &&
        (sensor_sub.humidity_sub >= 0) &&
        (sensor_sub.fds[1].revents & POLLIN)) {
        struct sensor_humi humi_data;
        if (orb_copy(ORB_ID(sensor_humi), sensor_sub.humidity_sub, &humi_data) == OK) {
            /* 直接更新全局湿度主题 */
            lv_subject_set_int(&humidity_subject, (int)(humi_data.humidity * 10));
            LV_LOG_INFO("Humidity updated: %.1f%%", humi_data.humidity);
            updated++;
        } else {
            LV_LOG_WARN("Failed to copy humidity data");
        }
    }
    /* 修改距离传感器数据处理部分 */
    if ((sensors_to_update & LV_DEMO_UPDATE_PROX) &&
        (sensor_sub.prox_sub >= 0) &&
        (sensor_sub.fds[2].revents & POLLIN)) {
        uint8_t raw_data[32];
        memset(raw_data, 0, sizeof(raw_data));

        if (orb_copy(ORB_ID(sensor_prox), sensor_sub.prox_sub, raw_data) == OK) {
            float distance_cm = 0.0f;

            float *float_ptr = (float*)(raw_data + 8);
            float distance_m = *float_ptr;
            distance_cm = distance_m * 100.0f;
            LV_LOG_INFO("Raw distance: %.3f m = %.1f cm", distance_m, distance_cm);
            /* 只确保最小值不为负，去掉最大值限制 */
            if (distance_cm < 0) {
                distance_cm = 0;
                LV_LOG_WARN("Distance < 0, clamped to 0 cm");
            }
            lv_subject_set_int(&prox_subject, (int)(distance_cm * 10));
            LV_LOG_INFO("Proximity updated: %.1f cm", distance_cm);
            updated++;
        }
    }
    //syslog(LOG_DEBUG, "Sensors updated: %d", updated);
    return updated;
}

/****************************************************************************
 * 传感器清理函数
 ****************************************************************************/
static void lv_demo_cleanup_sensor_subscriptions(void) {
  LV_LOG_INFO("Cleaning up sensor subscriptions");
  if (sensor_sub.temperature_sub >= 0) {
    orb_unsubscribe(sensor_sub.temperature_sub);
    sensor_sub.temperature_sub = -1;
  }
  if (sensor_sub.humidity_sub >= 0) {
    orb_unsubscribe(sensor_sub.humidity_sub);
    sensor_sub.humidity_sub = -1;
  }
  if (sensor_sub.prox_sub >= 0) {
    orb_unsubscribe(sensor_sub.prox_sub);
    sensor_sub.prox_sub = -1;
  }
  sensor_sub.initialized = false;
  LV_LOG_INFO("Sensor subscriptions cleaned up");
}

/****************************************************************************
 * 传感器更新定时器回调
 ****************************************************************************/
static void update_sensor_cb(lv_timer_t *timer) {
    LV_UNUSED(timer);
    /* 更新温度和湿度传感器数据 */
    int updated = lv_demo_update_sensor_data(LV_DEMO_UPDATE_ALL, NULL);
    if (updated < 0) {
        LV_LOG_ERROR("Failed to update sensor data (error code: %d)", updated);
    } else if (updated > 0) {
        LV_LOG_INFO("Sensors updated: %d sensors refreshed", updated);
    } else {
        LV_LOG_INFO("No sensor data available for update");
    }
}

/****************************************************************************
 * 初始化传感器
 ****************************************************************************/
static int init_sensors(void) {
    /* 直接初始化传感器订阅 */
    if (lv_demo_init_sensor_subscriptions() != 0) {
        LV_LOG_ERROR("Failed to initialize sensor subscriptions");
        return -1;
    }
    LV_LOG_INFO("Temperature and humidity and prox sensors initialized successfully");
    return 0;
}

/****************************************************************************
 * 日期观察者回调函数
 ****************************************************************************/
static void date_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(subject);
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%s, %d %s",
                (const char *)lv_subject_get_pointer(&week_day_name_subject),
                lv_subject_get_int(&month_day_subject),
                (const char *)lv_subject_get_pointer(&month_name_subject));
    
    lv_label_set_text(label, buf);
}

/****************************************************************************
 * 温度观察者回调函数
 ****************************************************************************/
static void temperature_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(subject);
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    /* 获取温度值（存储为10倍值） */
    int temp_value = lv_subject_get_int(&temperature_subject);
    int temp_integer = temp_value / 10;
    int temp_decimal = abs(temp_value % 10);
    char buf[32];
    /* 格式化为"23.5°C"的形式 */
    if (temp_value >= 0) {
        lv_snprintf(buf, sizeof(buf), "%d.%d°C", temp_integer, temp_decimal);
    } else {
        lv_snprintf(buf, sizeof(buf), "-%d.%d°C", -temp_integer, temp_decimal);
    }
    /* 根据温度值设置不同的颜色 */
    if (temp_integer < 18) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xE74C3C), 0); // 红色，偏冷
    } else if (temp_integer > 28) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xE74C3C), 0); // 红色，偏热
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0); // 白色，舒适
    }
    lv_label_set_text(label, buf);
    LV_LOG_INFO("Temperature displayed: %s", buf);
}

/****************************************************************************
 * 湿度观察者回调函数
 ****************************************************************************/
static void humidity_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(subject);
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    /* 获取湿度值（存储为10倍值） */
    int humi_value = lv_subject_get_int(&humidity_subject);
    int humi_integer = humi_value / 10;
    int humi_decimal = abs(humi_value % 10);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%d.%d%%", humi_integer, humi_decimal);
    /* 根据湿度值设置不同的颜色 */
    if (humi_integer < 30) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xE74C3C), 0); // 红色，干燥
    } else if (humi_integer > 70) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xE74C3C), 0); // 红色，潮湿
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0); // 白色，舒适
    }
    lv_label_set_text(label, buf);
    LV_LOG_INFO("Humidity displayed: %s", buf);
}

/****************************************************************************
 * 接近传感器观察者回调函数
 ****************************************************************************/
static void prox_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(subject);
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    int distance_value = lv_subject_get_int(&prox_subject);
    char buf[32];
    if (distance_value < 0) {
        lv_snprintf(buf, sizeof(buf), "--.- cm");
    } else {
        float distance_cm = distance_value / 10.0f;
        if (distance_cm >= 100.0f) {
            float distance_m = distance_cm / 100.0f;
            int m_int = (int)distance_m;
            int m_decimal = (int)((distance_m - m_int) * 10);
            lv_snprintf(buf, sizeof(buf), "%d.%d cm", m_int, m_decimal);
        } else {
            int cm_int = (int)distance_cm;
            int cm_decimal = (int)((distance_cm - cm_int) * 10);
            lv_snprintf(buf, sizeof(buf), "%d.%d cm", cm_int, cm_decimal);
        }
    }
    lv_label_set_text(label, buf);
}

/****************************************************************************
 * 加载背景图片函数
 ****************************************************************************/
static lv_obj_t* load_background_image(lv_obj_t *parent)
{
    lv_obj_t *img = lv_image_create(parent);
    const char *bg_path = "/resource/imgs/luncher_mini_bg_new.png";
    if (access(bg_path, F_OK) == 0)
    {
        lv_image_set_src(img, bg_path);
        LV_LOG_INFO("背景图片已从文件加载: %s", bg_path);
    }
    else
    {
        lv_color_t bg_color = lv_color_hex(0x1C2833);
        lv_obj_set_style_bg_color(img, bg_color, 0);
        lv_obj_set_style_bg_opa(img, LV_OPA_COVER, 0);
        LV_LOG_WARN("未找到背景图片文件，使用纯色背景");
    }
    lv_obj_set_size(img, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(img, 0, 0);
    return img;
}

/****************************************************************************
 * 恢复窗口颜色的回调函数
 ****************************************************************************/
static void restore_color_cb(lv_timer_t *timer)
{
    lv_obj_t *win = (lv_obj_t *)lv_timer_get_user_data(timer);
    // 恢复到预设的原始窗口颜色
    lv_obj_set_style_bg_color(win, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_70, 0);
    lv_timer_del(timer);
}

/****************************************************************************
 * 创建主界面
 ****************************************************************************/
static void create_main_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    /* 1. 加载背景图片 */
    bg_img = load_background_image(scr);
    /* 2. 创建主显示区域 */
    lv_obj_t *main_area = lv_obj_create(scr);
    lv_obj_set_size(main_area, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(main_area, 0, 0);
    lv_obj_set_style_bg_color(main_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(main_area, LV_OPA_0, 0);
    lv_obj_set_style_border_width(main_area, 0, 0);
    lv_obj_set_style_border_color(main_area, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_radius(main_area, 0, 0);
    /* 3. 创建时间日期合并显示区域 */
    lv_obj_t *datetime_area = lv_obj_create(main_area);
    lv_obj_set_size(datetime_area, SCREEN_WIDTH / 2, 100);
    lv_obj_align(datetime_area, LV_ALIGN_TOP_LEFT, 5, 15);
    lv_obj_set_style_bg_color(datetime_area, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_bg_opa(datetime_area, LV_OPA_0, 0);  /* 设置背景为透明 */
    lv_obj_set_style_border_width(datetime_area, 0, 0);
    lv_obj_set_style_radius(datetime_area, 10, 0);
    /* 时间数字显示 */
    time_label = lv_label_create(datetime_area);
    lv_obj_set_style_text_font(time_label, g_misans_normal_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, -8, 10);
    /* 日期显示 */
    date_label = lv_label_create(datetime_area);
    lv_obj_set_style_text_font(date_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    /* 4. 创建4个窗口 */
    const lv_color_t window_colors[4] = {
        lv_color_hex(0x2C3E50),  /* 深色半透明 - 窗口 */
        lv_color_hex(0x2C3E50),
        lv_color_hex(0x2C3E50),
        lv_color_hex(0x2C3E50),
    };
    const char *window_texts[4] = {
        "T&H",
        "Light",
        "Prox",
        "About",
    };
    int window_width = 60;
    int window_height = 50;
    int start_x = 5;
    int start_y = 140;
    for (int i = 0; i < 4; i++) {
        int x = start_x + i * (window_width + 10);
        int y = start_y;
        window[i] = lv_obj_create(main_area);
        lv_obj_set_size(window[i], window_width, window_height);
        lv_obj_set_pos(window[i], x, y);
        lv_obj_set_style_bg_color(window[i], window_colors[i], 0);
        lv_obj_set_style_bg_opa(window[i], LV_OPA_70, 0);  /* 半透明效果 */
        lv_obj_set_style_radius(window[i], 8, 0);
        lv_obj_set_style_border_width(window[i], 0, 0);  /* 无边框 */
        lv_obj_set_style_border_color(window[i], lv_color_hex(0xECF0F1), 0);
        /* 窗口1: 温湿度显示 */
        if (i == 0) {
            /* 温度显示 - 第一行 */
            temp_label = lv_label_create(window[i]);
            lv_label_set_text(temp_label, "T:25.0°C");
            lv_obj_set_style_text_font(temp_label, g_misans_normal_11, 0);
            lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 10);
            /* 湿度显示 - 第二行 */
            humidity_label = lv_label_create(window[i]);
            lv_label_set_text(humidity_label, "P:60.0%");
            lv_obj_set_style_text_font(humidity_label, g_misans_normal_11, 0);
            lv_obj_set_style_text_color(humidity_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(humidity_label, LV_ALIGN_BOTTOM_MID, 0, -10);
        }
        /* 窗口3: 接近传感器显示 */
        else if (i == 2) {
            /* PROX - 第一行 */
            lv_obj_t *distance_text = lv_label_create(window[i]);
            lv_label_set_text(distance_text, "PROX");
            lv_obj_set_style_text_font(distance_text, g_misans_normal_11, 0);
            lv_obj_set_style_text_color(distance_text, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(distance_text, LV_ALIGN_TOP_MID, 0, 10);
            /* 距离数值 - 第二行 */
            prox_label = lv_label_create(window[i]);
            lv_label_set_text(prox_label, "5.0 cm");
            lv_obj_set_style_text_font(prox_label, g_misans_normal_11, 0);
            lv_obj_set_style_text_color(prox_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(prox_label, LV_ALIGN_BOTTOM_MID, 0, -10);
        }
        /* 其他窗口: 保持原有显示 */
        else {
            lv_obj_t *title = lv_label_create(window[i]);
            lv_label_set_text(title, window_texts[i]);
            lv_obj_set_style_text_font(title, g_misans_normal_11, 0);
            lv_obj_set_style_text_color(title, lv_color_hex(0XFFFFFF), 0);
            lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
        }
        /* 添加点击事件 */
        lv_obj_add_event_cb(window[i], window_click_cb, LV_EVENT_CLICKED, NULL);
    }
}

/****************************************************************************
 * 按钮点击事件处理
 ****************************************************************************/
static void window_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED) {
        for (int i = 0; i < 4; i++) {
            if (obj == window[i]) {
                lv_color_t orig_color = lv_obj_get_style_bg_color(obj, 0);
                lv_color_t bright_color = lv_color_make(
                    LV_MIN(orig_color.red + 30, 255),
                    LV_MIN(orig_color.green + 30, 255),
                    LV_MIN(orig_color.blue + 30, 255)
                );
                lv_obj_set_style_bg_color(obj, bright_color, 0);
                lv_timer_t *timer = lv_timer_create(restore_color_cb, 300, NULL);
                lv_timer_set_user_data(timer, obj);
                lv_timer_set_repeat_count(timer, 1);
                LV_LOG_USER("Window %d clicked", i + 1);
                /* 如果是App 2，显示灯光控制界面 */
                if (i == 1) {
                    /* 确保about窗口已经被关闭 */
                    if (gamble_window != NULL) {
                        lv_obj_del(gamble_window);
                        gamble_window = NULL;
                    }
                    create_light_control_window();
                }
                /* 如果是App 4，显示Gamble信息界面 */
                else if (i == 3) {
                    /* 确保灯光控制窗口已经被关闭 */
                    if (light_window != NULL) {
                        lv_obj_del(light_window);
                        light_window = NULL;
                        light_switch = NULL;
                        brightness_slider = NULL;
                        brightness_label = NULL;
                    }
                    create_gamble_info_window();
                }
                /* 如果是窗口1或窗口3，仅记录点击事件，不显示新窗口 */
                else if (i == 0 || i == 2) {
                    /* 传感器窗口点击事件 */
                    LV_LOG_INFO("Sensor window %d clicked", i + 1);
                }
                break;
            }
        }
    }
}

/****************************************************************************
 * 创建灯光控制窗口
 ****************************************************************************/
static void create_light_control_window(void)
{
    if (light_window != NULL) {
        return; // 窗口已存在
    }
    /* 计算窗口居中位置 */
    lv_coord_t left = (SCREEN_WIDTH - 240) / 2;
    lv_coord_t top = (SCREEN_HEIGHT - 160) / 2;
    /* 创建窗口 */
    light_window = lv_obj_create(lv_scr_act());
    lv_obj_set_size(light_window, 240, 160);
    lv_obj_set_pos(light_window, left, top);
    /* 设置窗口样式 */
    lv_obj_set_style_bg_color(light_window, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(light_window, LV_OPA_90, 0);
    lv_obj_set_style_radius(light_window, 15, 0);
    lv_obj_set_style_border_width(light_window, 3, 0);
    lv_obj_set_style_border_color(light_window, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_shadow_width(light_window, 30, 0);
    lv_obj_set_style_shadow_color(light_window, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(light_window, LV_OPA_70, 0);
    lv_obj_set_style_shadow_spread(light_window, 5, 0);
    /* 创建关闭按钮 - 放在窗口左上角 */
    lv_obj_t *light_close_btn = lv_btn_create(light_window);
    lv_obj_set_size(light_close_btn, 60, 30);
    lv_obj_align(light_close_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(light_close_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_opa(light_close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(light_close_btn, 5, 0);
    lv_obj_set_style_border_width(light_close_btn, 1, 0);
    lv_obj_set_style_border_color(light_close_btn, lv_color_hex(0xC0392B), 0);
    lv_obj_t *light_close_btn_label = lv_label_create(light_close_btn);
    lv_label_set_text(light_close_btn_label, "Close");
    lv_obj_set_style_text_font(light_close_btn_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(light_close_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(light_close_btn_label);
    lv_obj_add_event_cb(light_close_btn, close_light_window_cb, LV_EVENT_CLICKED, NULL);
    /* 创建标题 */
    lv_obj_t *title = lv_label_create(light_window);
    lv_label_set_text(title, "Light Control");
    lv_obj_set_style_text_font(title, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xECF0F1), 0);
    lv_obj_align(title, LV_ALIGN_TOP_RIGHT, 0, 15);
    /* 创建开关 */
    light_switch = lv_switch_create(light_window);
    lv_obj_align(light_switch, LV_ALIGN_TOP_LEFT, 30, 50);
    /* 设置开关初始状态 - 使用适配层获取LED状态 */
    bool led_state = false;
    led_is_on(&led_state);
    if (led_state) {
        lv_obj_add_state(light_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(light_switch, LV_STATE_CHECKED);
    }
    /* 添加开关事件 */
    lv_obj_add_event_cb(light_switch, light_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    /* 创建开关标签 */
    lv_obj_t *switch_label = lv_label_create(light_window);
    lv_label_set_text(switch_label, "Light Switch");
    lv_obj_set_style_text_font(switch_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(switch_label, lv_color_hex(0xBDC3C7), 0);
    lv_obj_align_to(switch_label, light_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    /* 创建亮度滑块 */
    brightness_slider = lv_slider_create(light_window);
    lv_obj_set_width(brightness_slider, 180);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 110);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, g_led_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    /* 创建亮度标签 */
    brightness_label = lv_label_create(light_window);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", g_led_brightness);
    lv_label_set_text(brightness_label, buf);
    lv_obj_set_style_text_font(brightness_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x95A5A6), 0);
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    LV_LOG_USER("Light control window created");
}

/****************************************************************************
 * 灯光开关事件处理
 ****************************************************************************/
static void light_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (state) {
        led_adapter_on();
    } else {
        led_adapter_off();
    }
    LV_LOG_USER("Light switched %s", state ? "ON" : "OFF");
}

/****************************************************************************
 * 亮度滑块事件处理
 ****************************************************************************/
static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(obj);
    led_adapter_set_brightness(value);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", value);
    lv_label_set_text(brightness_label, buf);
    LV_LOG_USER("Brightness set to %d%%", value);
}

/****************************************************************************
 * 关闭灯光控制窗口
 ****************************************************************************/
static void close_light_window_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (light_window != NULL) {
        /* 删除灯光控制窗口 */
        lv_obj_del(light_window);
        light_window = NULL;
        light_switch = NULL;
        brightness_slider = NULL;
        brightness_label = NULL;
        /* 确保About窗口没有被错误创建 */
        if (gamble_window != NULL) {
            lv_obj_del(gamble_window);
            gamble_window = NULL;
        }
        LV_LOG_USER("Light control window closed, returned to main screen");
    }
}

/****************************************************************************
 * 创建Gamble信息窗口
 ****************************************************************************/
static void create_gamble_info_window(void)
{
    if (gamble_window != NULL) {
        return; // 窗口已存在
    }
    /* 计算窗口居中位置 */
    lv_coord_t left = (SCREEN_WIDTH - 260) / 2;
    lv_coord_t top = (SCREEN_HEIGHT - 200) / 2;
    /* 创建窗口 */
    gamble_window = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gamble_window, 260, 200);
    lv_obj_set_pos(gamble_window, left, top);
    /* 设置窗口样式 */
    lv_obj_set_style_bg_color(gamble_window, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(gamble_window, LV_OPA_90, 0);
    lv_obj_set_style_radius(gamble_window, 10, 0);
    lv_obj_set_style_border_width(gamble_window, 1, 0);
    lv_obj_set_style_border_color(gamble_window, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_pad_all(gamble_window, 0, 0);
    /* 设置窗口为弹性布局容器 */
    lv_obj_set_flex_flow(gamble_window, LV_FLEX_FLOW_COLUMN);
    /* 创建顶部标题栏 */
    lv_obj_t *header_cont = lv_obj_create(gamble_window);
    lv_obj_set_size(header_cont, LV_PCT(100), 40);
    lv_obj_set_style_pad_all(header_cont, 5, 0);
    lv_obj_set_flex_flow(header_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(header_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_cont, 0, 0);
    /* 创建关闭按钮 */
    lv_obj_t *about_close_btn = lv_btn_create(header_cont);
    lv_obj_set_size(about_close_btn, 30, 25);
    lv_obj_set_style_bg_color(about_close_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_opa(about_close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(about_close_btn, 3, 0);
    lv_obj_set_style_border_width(about_close_btn, 0, 0);
    lv_obj_t *about_close_btn_label = lv_label_create(about_close_btn);
    lv_label_set_text(about_close_btn_label, "X");
    lv_obj_set_style_text_font(about_close_btn_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(about_close_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(about_close_btn_label);
    lv_obj_add_event_cb(about_close_btn, close_gamble_window_cb, LV_EVENT_CLICKED, NULL);
    /* 创建标题 */
    lv_obj_t *title_label = lv_label_create(header_cont);
    lv_label_set_text(title_label, "Gemini-s1 开发板介绍");
    lv_obj_set_style_text_font(title_label, g_misans_normal_12, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xECF0F1), 0);
    /* 创建可滚动内容区域 */
    lv_obj_t *scroll_cont = lv_obj_create(gamble_window);
    lv_obj_set_size(scroll_cont, LV_PCT(100), 160);
    lv_obj_set_style_pad_all(scroll_cont, 15, 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    /* 创建文本内容 */
    lv_obj_t *content_label = lv_label_create(scroll_cont);
    lv_label_set_text(content_label, "\n"
                                   "Gemini-s1 开发板介绍\n\n"
                                   "具备高性能主控   R528双核ARM Cortex-A7\n\n"
                                   "音频处理   麦克风阵列\n\n"
                                   "可视化交互   配备显示屏\n\n"
                                   "具备多模协同能力   WIFI,蓝牙无线通信\n\n"
                                   "提供丰富外设接口   GPIO、I2C、SPI、UART、ADC、PCM等\n\n"
                                   "扩展多类环境传感器   温湿度，光照，人体感应、空气质量等\n");
    lv_obj_set_style_text_font(content_label, g_misans_normal_11, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0xECF0F1), 0);
    lv_obj_set_style_text_align(content_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(content_label, LV_PCT(100));
    /* 启用滚动 */
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_scroll_to_y(scroll_cont, 0, LV_ANIM_OFF);
    LV_LOG_USER("Gamble info window created");
}

/****************************************************************************
 * 关闭Gamble信息窗口
 ****************************************************************************/
static void close_gamble_window_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (gamble_window != NULL) {
        /* 删除Gamble信息窗口 */
        lv_obj_del(gamble_window);
        gamble_window = NULL;
        /* 确保灯光控制窗口没有被错误创建 */
        if (light_window != NULL) {
            lv_obj_del(light_window);
            light_window = NULL;
            light_switch = NULL;
            brightness_slider = NULL;
            brightness_label = NULL;
        }
        LV_LOG_USER("Gamble info window closed, returned to main screen");
    }
}

/****************************************************************************
 * 主函数
 ****************************************************************************/
int luncher_mini_main(int argc, FAR char *argv[])
{
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
    if (lv_is_initialized()) {
        LV_LOG_ERROR("LVGL already initialized");
        return -1;
    }
    #ifdef NEED_BOARDINIT
        boardctl(BOARDIOC_INIT, 0);
    #endif
    /* LVGL初始化 */
    lv_init();
    lv_nuttx_dsc_init(&info);
    #ifdef CONFIG_LV_USE_NUTTX_LCD
        info.fb_path = "/dev/lcd0";
    #endif 
    #ifdef CONFIG_INPUT_TOUCHSCREEN
        info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
    #endif
    #ifdef CONFIG_LV_USE_NUTTX_LIBUV
        uv_loop_t ui_loop;
        lv_memzero(&ui_loop, sizeof(ui_loop));
    #endif
    /* NuttX后端初始化 */
    lv_nuttx_init(&info, &result);
    /* 必要延时，影响初始化顺序 */
    usleep(100000);
    if (result.disp == NULL) {
        LV_LOG_ERROR("LVGL initialization failed");
        return 1;
    }
    /* 初始化中文支持字体 */
    init_fonts();
    /* 创建界面 */
    create_main_screen();
    /* 初始化时间和日期主题 */
    lv_subject_init_int(&hour_subject, 0);
    lv_subject_init_int(&minute_subject, 0);
    lv_subject_init_int(&second_subject, 0);
    static const char *default_day = "Sunday";
    static const char *default_month = "January";
    lv_subject_init_pointer(&week_day_name_subject, (void *)default_day);
    lv_subject_init_int(&month_day_subject, 1);
    lv_subject_init_pointer(&month_name_subject, (void *)default_month);
    /* 为时间和日期标签添加观察者 */
    lv_subject_add_observer_obj(&hour_subject, time_observer_cb, time_label, NULL);
    lv_subject_add_observer_obj(&minute_subject, time_observer_cb, time_label, NULL);
    lv_subject_add_observer_obj(&second_subject, time_observer_cb, time_label, NULL);
    lv_subject_add_observer_obj(&week_day_name_subject, date_observer_cb, date_label, NULL);
    lv_subject_add_observer_obj(&month_day_subject, date_observer_cb, date_label, NULL);
    lv_subject_add_observer_obj(&month_name_subject, date_observer_cb, date_label, NULL);
    /* 立即更新一次时间和日期 */
    update_time_cb(NULL);
    /* 传感器相关初始化 */
    /* 初始化传感器主题 */
    lv_subject_init_int(&temperature_subject, 250);  // 默认25.0°C
    lv_subject_init_int(&humidity_subject, 600);     // 默认60.0%
    lv_subject_init_int(&prox_subject, 50);          // 默认5.0cm
    /* 为传感器标签添加观察者 */
    lv_subject_add_observer_obj(&temperature_subject, temperature_observer_cb, temp_label, NULL);
    lv_subject_add_observer_obj(&humidity_subject, humidity_observer_cb, humidity_label, NULL);
    lv_subject_add_observer_obj(&prox_subject, prox_observer_cb, prox_label, NULL);
    /* 初始化传感器 */
    if (init_sensors() != 0) {
        LV_LOG_ERROR("Failed to initialize sensors");
    } else {
        LV_LOG_INFO("Sensors initialized successfully");
        /* 创建传感器数据更新定时器 */
        lv_timer_t *sensor_timer = lv_timer_create(update_sensor_cb, 1000, NULL);
        if (sensor_timer == NULL) {
            LV_LOG_ERROR("Failed to create sensor update timer");
        } else {
            lv_timer_set_repeat_count(sensor_timer, -1);
            LV_LOG_INFO("Sensor update timer created successfully");
        }
    }
    /* 初始化LED适配器 */
    LV_LOG_INFO("Initializing LED adapter...");
    led_error_t led_err = led_adapter_init();
    if (led_err != LED_SUCCESS) {
        LV_LOG_ERROR("Failed to initialize LED adapter: %s", 
                     led_get_error_string(led_err));
        light_initialized = false;
    } else {
        light_initialized = true;
        LV_LOG_INFO("LED adapter initialized successfully");
        /* 运行LED诊断 */
        led_adapter_diagnose();
        /* 快速测试LED */
        LV_LOG_INFO("Quick LED test...");
        led_adapter_on();
        usleep(200000);
        led_adapter_off();
        usleep(200000);
        // led_adapter_on(); // 默认关闭灯光
        LV_LOG_INFO("LED test completed");
    }
    /* 创建更新时间定时器 */
    lv_timer_t *time_timer = lv_timer_create(update_time_cb, 1000, NULL);
    if (time_timer == NULL) {
        LV_LOG_ERROR("Failed to create time update timer");
    } else {
        lv_timer_set_repeat_count(time_timer, -1);
        LV_LOG_INFO("Time update timer created successfully");
    }
    /* 显示分辨率信息 */
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t disp_width = lv_disp_get_hor_res(disp);
    lv_coord_t disp_height = lv_disp_get_ver_res(disp);
    LV_LOG_INFO("Display: %dx%d, App area: %dx%d",
                disp_width, disp_height, SCREEN_WIDTH, SCREEN_HEIGHT);
    /* 主循环 */
    #ifdef CONFIG_LV_USE_NUTTX_LIBUV
        lv_nuttx_uv_loop(&ui_loop, &result);
    #else
        while (1) {
            uint32_t idle = lv_timer_handler();
            usleep(idle ? idle * 1000 : 5000);
        }
    #endif
    /* 清理资源 */
    if (light_initialized) {
        led_adapter_deinit();
    }
    return 0;
}

#endif /* CONFIG_LUNCHER_MINI_APP */