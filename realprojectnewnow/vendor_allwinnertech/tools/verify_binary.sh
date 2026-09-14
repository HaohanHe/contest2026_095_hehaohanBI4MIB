#!/bin/bash

# Configuration
VENDOR_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Allwinnertech Binary-only Verification Tool"
echo "============================================"
echo "This script will REMOVE protected .c files to verify the binary-only build."
echo "CAUTION: This is destructive. Make sure you have a backup or use Git."
echo ""

printf "Are you sure you want to proceed? (y/n) "
read REPLY
echo
if [ "$REPLY" != "y" ] && [ "$REPLY" != "Y" ]; then
    exit 1
fi

# 1. Protect the 'glue' files we decided to keep in source
# Realtek wrappers, micro_sd_driver.c, etc. are already whitelisted in Makefiles.
# But for visual verification, we leave them alone.

echo "Performing aggressive cleanup of source files..."
# We keep only:
# 1. Headers (.h)
# 2. Build system files (Makefile, Make.defs, Kconfig, .mk)
# 3. Whitelisted 'glue' or 'app' code
# 4. Pre-compiled libraries (.a)

# Find and delete .c and .S files that are NOT whitelisted
# We explicitly EXCLUDE .ld.S and .ld files as they are linker scripts.
find "${VENDOR_DIR}" -type f \( -name "*.c" -o -name "*.S" \) \
    ! -path "*/test/*" \
    ! -path "*/apps/*" \
    ! -path "*/lichee/*" \
    ! -name "*.ld.S" \
    ! -name "*.ld" \
    ! -name "r528_wlan.c" \
    -delete

echo "Pruning empty directories..."
# Clean up empty directories to make the tree cleaner
find "${VENDOR_DIR}" -type d -empty -delete

echo "Cleaning up chip sources in board dir..."
find "${VENDOR_DIR}/boards/r528/r528s3-gemini-s1/src" -type f \( -name "*.c" -o -name "*.S" \) -delete

echo "============================================"
echo "Aggressive cleanup complete."
echo "The tree now mostly contains headers and build logic."
echo "If CONFIG_ALLWINNER_BINARY_ONLY=y is set, it should still work!"
