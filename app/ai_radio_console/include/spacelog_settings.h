#ifndef __SPACELOG_SETTINGS_H
#define __SPACELOG_SETTINGS_H

/****************************************************************************
 * SpaceLog 配置与配网模块
 * 提供可扩展的 LVGL 设置页面，支持 WiFi/ASR/LLM/API Key/音频增益等配置
 * 参考 ai_agent 框架的 config-wizard skill 和 ble_cmd_handler 设计
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

/* 配置项输入类型 */
typedef enum {
    CFG_TYPE_TEXT,       /* 普通文本输入 */
    CFG_TYPE_PASSWORD,   /* 密码掩码输入 */
    CFG_TYPE_SELECT,     /* 下拉选择（如模型选择） */
    CFG_TYPE_INFO,       /* 只读信息显示 */
    CFG_TYPE_ACTION,     /* 触发动作（如"连接WiFi"按钮） */
} spacelog_cfg_type_t;

/* 配置项定义 */
typedef struct {
    const char *key;              /* config_store 中的 key */
    const char *label;            /* 显示名称 */
    spacelog_cfg_type_t type;     /* 输入类型 */
    const char *default_val;      /* 默认值 */
    const char *const *options;   /* SELECT 类型的选项数组 */
    int num_options;              /* SELECT 选项数量 */
    void (*on_change)(const char *key, const char *val);  /* 变更回调 */
} spacelog_cfg_item_t;

/* 配置页面上下文，用于管理覆盖层状态 */
typedef struct {
    lv_obj_t *overlay;            /* 覆盖层根对象 */
    lv_obj_t *list;               /* 滚动列表 */
    lv_obj_t *kb;                 /* 键盘对象 */
    lv_obj_t *ta;                 /* 文本输入框 */
    lv_obj_t *edit_label;         /* 当前编辑项标签 */
    int editing_idx;              /* 当前编辑项索引 */
    bool visible;                 /* 设置页面是否可见 */
} spacelog_settings_ctx_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * 初始化设置模块，从 config_store 读取当前配置
 * 返回 0 成功，负数失败
 */
int spacelog_settings_init(void);

/**
 * 创建设置页面（全屏覆盖层）
 * @param parent 父屏幕对象
 * 返回 0 成功，负数失败
 */
int spacelog_settings_create(lv_obj_t *parent);

/**
 * 显示设置页面
 */
void spacelog_settings_show(void);

/**
 * 隐藏设置页面
 */
void spacelog_settings_hide(void);

/**
 * 获取配置项当前值
 * @param key 配置项 key
 * @param buf 输出缓冲区
 * @param cap 缓冲区大小
 * 返回 0 成功，负数失败
 */
int spacelog_settings_get_value(const char *key, char *buf, size_t cap);

/**
 * 设置配置项值并持久化
 * @param key 配置项 key
 * @param val 新值
 * 返回 0 成功，负数失败
 */
int spacelog_settings_set_value(const char *key, const char *val);

/**
 * 获取配置项表（用于扩展）
 * @param count 输出配置项数量
 * 返回配置项表指针
 */
const spacelog_cfg_item_t *spacelog_settings_get_items(int *count);

/**
 * 设置页面是否可见
 */
bool spacelog_settings_is_visible(void);

#endif /* __SPACELOG_SETTINGS_H */
