\[ English | [简体中文](../../../../zh-cn/api/framework/quickapp/basic.md) \]

# QuickApp Framework Introduction

The openvela QuickApp framework (hereinafter referred to as "application framework") is the [Quick App](https://doc.quickapp.cn/) runtime implementation on openvela. Compared to the mobile runtime, the openvela QuickApp framework has the following characteristics:

- Follows the Quick App Alliance standard, re-implemented for the openvela system, with some features and functionalities trimmed.
- Adapted for Real-Time Operating Systems (RTOS), focusing on runtime performance with high execution efficiency under low memory consumption.
- Easy to develop and deploy, effectively shortening the application development cycle.

This document introduces the overall design, implementation approach, and technical highlights of the application framework, rather than focusing on application development itself.

## Related Documentation

- [Xiaomi openvela QuickApp Development Manual](https://iot.mi.com/vela/quickapp/zh/content/intro.html) — Complete development guide for application developers
- [Feature Framework API](../feature/index.md) — Native extension APIs for QuickApp (JS and C/C++ interop)

## Build Configuration

The application framework itself has few configuration items, but has many dependencies. Among the dependencies, the renderer has the most requirements. For specific configuration, please refer to the documentation provided by the graphics team.

### Main Configuration

```kconfig
CONFIG_QUICKAPP_VAPP=y                      # QuickApp main configuration
CONFIG_QUICKAPP_VAPP_XMS=y                  # QuickApp xms integrated version, depends on xms service framework
CONFIG_QUICKAPP_LOG_LEVEL=1                 # Log level, default INFO
CONFIG_QUICKAPP_MICRO_FRAMEWORK_MODE=y      # Micro framework
CONFIG_QUICKAPP_PRIORITY=100                # Priority
CONFIG_HAP_APP_PATH="/data"                 # rpk installation path
CONFIG_QUICKAPP_THREADSTACKSIZE=1048576     # JS thread stack size
CONFIG_QUICKAPP_JSSTACKSIZE=524288          # JS engine stack size
CONFIG_QUICKAPP_JSHEAPSIZE=4194304          # JS engine heap memory limit, default 4MB for watch devices, adjust based on application complexity
CONFIG_CURL=y                               # Enable curl support, required for framework network feature
CONFIG_QUICKAPP_RPK_DIR="/resource/package" # AMS application installation path
CONFIG_QUICKAPP_BYTECODE_OPTIMIZATION=y     # QuickJS bytecode optimization (string merging)
CONFIG_QUICKAPP_FOLME_ANIMENGINE_ADAPTER=n  # folme animation engine
CONFIG_WIDGET_IMAGE_USE_CACHE_MANAGER=y     # Enable widget image cache manager

# Font-related configuration
CONFIG_FONT_DEFAULT_NORMAL_NAME="MiSansW_Regular"
CONFIG_FONT_DEFAULT_BOLD_NAME="MiSansW_Demibold"
CONFIG_FONT_DEFAULT_SIZE=30
CONFIG_PROMPT_TOAST_FONT_SIZE=24
CONFIG_PROMPT_DIALOG_TITLE_FONT_SIZE=36
CONFIG_PROMPT_DIALOG_MSG_FONT_SIZE=34
```

### Debug Configuration

```kconfig
CONFIG_DOM_TRACE_ENABLE=n                   # vdom tree printing
CONFIG_JS_USE_SCHED_NOTE=n                  # Framework startup trace
CONFIG_QUICKAPP_MEMORY_STATUS=n             # Framework JS engine memory info printing
CONFIG_WIDGET_LOG_ENABLE=y                  # LVGL widget log
CONFIG_WIDGET_LOG_LEVEL=1                   # widget log level, default warning
CONFIG_WIDGET_ASSERT_ENABLE=n               # widget assert
CONFIG_WIDGET_TRACE_ENABLE=n               # widget trace check
CONFIG_WIDGET_PERF_ENABLE=n                 # widget performance monitor
CONFIG_WIDGET_DUMP_TREE_ENABLE=n            # dump LVGL widget tree
CONFIG_WIDGET_DUMP_TREE_IN_LAYOUT=n         # dump widget tree in layout task
CONFIG_WIDGET_SHOW_YOGA_NODE_ENABLE=n       # widget show yoga node
CONFIG_WIDGET_DEBUG_DRAW_OUTLINE=n          # widget draw outline for debug
CONFIG_CSS_ATTR_LIST_ENABLE=n               # Enable widget get css/attr function
```

### Dependencies

```kconfig
CONFIG_LIBUV=y
CONFIG_LVGL_EXTENSION=y
CONFIG_LIBUV_EXTENSION=y
CONFIG_LVX_USE_FONT_MANAGER=y
CONFIG_LIB_YOGA=y
CONFIG_PROTOBUF_C=y
CONFIG_LIB_PNG=y
CONFIG_LV_USE_LIBPNG=y
CONFIG_LV_USE_NUTTX_LIBUV=y
CONFIG_USE_QUICKJS=y
```
