subdir-ccflags-y += -I$(srctree)/ekernel/subsys/avframework/multimedia/tplayer \
                    -I$(srctree)/ekernel/subsys/avframework/multimedia/tplayer/awsink \
                    -I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarc/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/external/include/adecoder \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/libcore/playback/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/libcore/common/iniparser \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/libcore/parser/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/libcore/stream/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/libcore/base/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libcedarx/xplayer/include \
					-I$(srctree)/ekernel/subsys/avframework/multimedia/libuapi/src/include \

lib-y += tplayer.o
lib-y += awsink/tsound_ctrl.o
lib-y += awsink/tlayer_ctrl.o
lib-y += awsink/tdeinterlace_ctrl.o
lib-y += awsink/tsubtitlectrl_ctrl.o

TARGET := $(obj)/libtplayer.a

include $(MELIS_BASE)/scripts/Makefile.rename

