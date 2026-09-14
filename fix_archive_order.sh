#!/bin/bash
# fix_archive_order.sh - 自动修复openvela build archive order bug
# 每次distclean后运行此脚本，然后重新构建即可
# Usage: ./fix_archive_order.sh [openvela-build路径]

# 显示帮助信息
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "用法: $0 [openvela-build路径]"
    echo ""
    echo "功能: 自动修复openvela构建系统的archive order bug"
    echo ""
    echo "说明:"
    echo "  1. 每次distclean后构建会报undefined reference错误"
    echo "  2. 运行此脚本可以自动修复这些问题"
    echo "  3. 修复后重新构建即可成功"
    echo ""
    echo "示例:"
    echo "  $0                          # 使用默认路径 ~/openvela-build"
    echo "  $0 /path/to/openvela-build  # 使用指定路径"
    exit 0
fi

BUILD_DIR="${1:-$HOME/openvela-build}"
STAGING="$BUILD_DIR/nuttx/staging/libapps.a"
ARM_TOOLS="$BUILD_DIR/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin"

if [ ! -f "$STAGING" ]; then
    echo "ERROR: $STAGING 不存在，请先运行一次构建"
    exit 1
fi

export PATH="$ARM_TOOLS:$PATH"

echo "=== 修复archive order bug ==="
echo "libapps.a: $STAGING"
echo ""

# 所有需要添加的模块路径（按依赖顺序）
MODULE_PATHS=(
    "apps/crypto/mbedtls"
    "external/c-ares"
    "external/unqlite"
    "apps/netutils/cjson"
    "apps/graphics/lvgl"
    "apps/frameworks/system/utils"
    "apps/external/iperf2"
    "external/libssh"
    "external/libpng"
    "apps/frameworks/connectivity/bluetooth"
    "apps/external/zblue"
    "external/zlib"
)

TOTAL=0
for module in "${MODULE_PATHS[@]}"; do
    full_path="$BUILD_DIR/$module"
    if [ -d "$full_path" ]; then
        count=$(find "$full_path" -name "*.o" 2>/dev/null | wc -l)
        if [ "$count" -gt 0 ]; then
            find "$full_path" -name "*.o" 2>/dev/null | xargs arm-none-eabi-ar r "$STAGING" 2>/dev/null
            echo "  ✅ $module: $count objects"
            TOTAL=$((TOTAL + count))
        fi
    fi
done

# 额外查找zlib对象文件（可能在不同位置）
find "$BUILD_DIR" -path "*/zlib*" -name "*.o" 2>/dev/null | xargs arm-none-eabi-ar r "$STAGING" 2>/dev/null

# 修复CJK字体：添加到liblvgl.a和libapps.a
CJK_OBJ=$(find "$BUILD_DIR/apps/graphics/lvgl" -name "*simsun*.o" -not -name "*.su" 2>/dev/null | head -1)
if [ -n "$CJK_OBJ" ]; then
    LVGL_LIB="$BUILD_DIR/apps/graphics/lvgl/liblvgl.a"
    if [ -f "$LVGL_LIB" ]; then
        arm-none-eabi-ar r "$LVGL_LIB" "$CJK_OBJ" 2>/dev/null
        echo "  ✅ CJK字体已添加到liblvgl.a"
    fi
    arm-none-eabi-ar r "$STAGING" "$CJK_OBJ" 2>/dev/null
    echo "  ✅ CJK字体已添加到libapps.a"
    TOTAL=$((TOTAL + 1))
fi

echo ""
echo "=== 修复完成 ==="
echo "共添加约 $TOTAL 个对象文件到 libapps.a"
echo ""
echo "下一步:"
echo "  1. 重新构建: cd $BUILD_DIR && ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j1"
echo "  2. 打包固件: cd $BUILD_DIR/vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack"
