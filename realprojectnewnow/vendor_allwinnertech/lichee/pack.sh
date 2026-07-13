#!/bin/env bash

if [ ! -f "${PWD}/tools/scripts/envsetup.sh" ]; then
	echo "ERROR: Please source execute the script in vendor/allwinnertech/lichee!"
	return -1
else
	RTOS_TOP="$(PWD= /bin/pwd)"
fi

if [ "x$1" == "xr528s3-evb4" ]; then
	if [ "x$2" == "x-s" ]; then
		${RTOS_TOP}/tools/scripts/pack_img.sh -c sun8iw20p1 -p rtos -b r528s3-evb4 -o nuttx -d uart0 -s secure -m normal -w none -v none -i none -t ${RTOS_TOP} -f r528s3/evb4_nand -g r528s3/evb4_nand
	else
		${RTOS_TOP}/tools/scripts/pack_img.sh -c sun8iw20p1 -p rtos -b r528s3-evb4 -o nuttx -d uart0 -s none -m normal -w none -v none -i none -t ${RTOS_TOP} -f r528s3/evb4_nand -g r528s3/evb4_nand
	fi
elif [ "x$1" == "xr528s3-x4b" ]; then
	if [ "x$2" == "x-s" ]; then
		${RTOS_TOP}/tools/scripts/pack_img.sh -c sun8iw20p1 -p rtos -b r528s3-x4b -o nuttx -d uart0 -s secure -m normal -w none -v none -i none -t ${RTOS_TOP} -f r528s3/x4b_nand -g r528s3/x4b_nand
	else
		${RTOS_TOP}/tools/scripts/pack_img.sh -c sun8iw20p1 -p rtos -b r528s3-x4b -o nuttx -d uart0 -s none -m normal -w none -v none -i none -t ${RTOS_TOP} -f r528s3/x4b_nand -g r528s3/x4b_nand
	fi
else
	echo "ERROR: Unsupported platform, please check your command!"
	echo "Usage: ./pack.sh + platform"
	echo "For example: ./pack.sh r528s3-x4b"
fi

