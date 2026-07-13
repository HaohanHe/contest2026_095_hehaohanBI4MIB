#!/bin/bash
# Fix archive order bug in openvela-build
# Usage: bash fix_archive_order.sh
# Run after build.sh fails with "undefined reference" errors
# Then re-run build.sh to link successfully

cd /home/bi4mib/openvela-build
STAGING=nuttx/staging/libapps.a

echo "=== Fixing archive order bug ==="

# mbedTLS
find apps/crypto/mbedtls/mbedtls/library -name "*.o" > /tmp/obj_mbedtls.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_mbedtls.txt | tr '\n' ' ')
echo "mbedTLS: $(wc -l < /tmp/obj_mbedtls.txt) objects"

# c-ares
find external/c-ares -name "*.o" > /tmp/obj_cares.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_cares.txt | tr '\n' ' ')
echo "c-ares: $(wc -l < /tmp/obj_cares.txt) objects"

# zlib
find . -path "*/zlib*" -name "*.o" > /tmp/obj_zlib.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_zlib.txt | tr '\n' ' ')
echo "zlib: $(wc -l < /tmp/obj_zlib.txt) objects"

# LVGL (all)
find apps/graphics/lvgl -name "*.o" > /tmp/obj_lvgl.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_lvgl.txt | tr '\n' ' ')
echo "LVGL: $(wc -l < /tmp/obj_lvgl.txt) objects"

# libuv (excluding process-spawn)
find apps/system/libuv -name "*.o" | grep -v "process-spawn" > /tmp/obj_libuv.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_libuv.txt | tr '\n' ' ')
echo "libuv: $(wc -l < /tmp/obj_libuv.txt) objects"

# process-spawn patched
arm-none-eabi-ar r $STAGING /tmp/process-spawn-patched.o
echo "process-spawn: patched"

# libpng
find external/libpng -name "*.o" > /tmp/obj_libpng.txt
arm-none-eabi-ar r $STAGING $(cat /tmp/obj_libpng.txt | tr '\n' ' ')
echo "libpng: $(wc -l < /tmp/obj_libpng.txt) objects"

# Remove conflicting _1.o version
arm-none-eabi-ar d $STAGING "process-spawn.c.home.bi4mib.openvela-build.apps.system.libuv_1.o" 2>/dev/null

echo "=== Done! Re-run build.sh to link ==="
