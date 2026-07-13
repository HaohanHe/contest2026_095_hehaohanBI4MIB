define POSTBUILD
	if [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xbl" ]; then \
		if [ "x$$(echo ${TOPDIR} | grep out)" != "x" ]; then \
			cp ${TOPDIR}/../../nuttx/nuttx.bin ${TOPDIR}/../../../../../nuttx/$(CONFIG_IMAGE_PACK_PATH); \
		else \
			cp ${TOPDIR}/nuttx.bin $(CONFIG_IMAGE_PACK_PATH); \
		fi \
	else \
		if [ "x$$(echo ${TOPDIR} | grep out)" != "x" ]; then \
			$(STRIP) ${TOPDIR}/../../nuttx/nuttx -o ${TOPDIR}/../../../../../nuttx/$(CONFIG_IMAGE_PACK_PATH); \
			if [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xap" ]; then \
				AVB_PARTITION_PATH=$${TOPDIR}/../../../../$(CONFIG_ARCH_BOARD_PARTITION_TABLE); \
				${TOPDIR}/../../../../../frameworks/system/ota/tools/avb_sign.sh ${TOPDIR}/../../../../../nuttx/$(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /resource/ap/ap.fex -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_AP_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 0"; \
			elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xtee" ]; then \
				${TOPDIR}/../../../../../frameworks/system/ota/tools/avb_sign.sh ${TOPDIR}/../../../../../nuttx/$(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /dev/tee -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_TEE_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 1"; \
			elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xota" ]; then \
				${TOPDIR}/../../../../../frameworks/system/ota/tools/avb_sign.sh ${TOPDIR}/../../../../../nuttx/$(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /ota/vela_ota.bin -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_OTA_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 2"; \
			fi \
		else \
			$(STRIP) ${TOPDIR}/nuttx -o $(CONFIG_IMAGE_PACK_PATH); \
			if [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xap" ]; then \
				AVB_PARTITION_PATH=$${TOPDIR}/$(CONFIG_ARCH_BOARD_PARTITION_TABLE); \
				${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /resource/ap/ap.fex -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_AP_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 0"; \
			elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xtee" ]; then \
				${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /dev/tee -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_TEE_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 1"; \
			elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xota" ]; then \
				${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $(CONFIG_IMAGE_PACK_PATH) 0 -o --dynamic_partition_size -P /ota/vela_ota.bin -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_OTA_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 2"; \
			else  \
				cp ${TOPDIR}/nuttx.bin $(CONFIG_IMAGE_PACK_PATH); \
			fi \
		fi \
	fi

	cp ${TOPDIR}/nuttx     ${TOPDIR}/vela_$(CONFIG_ARCH_BOARD_CUSTOM_NAME).elf
	cp ${TOPDIR}/nuttx.map ${TOPDIR}/vela_$(CONFIG_ARCH_BOARD_CUSTOM_NAME).map
	VELA_ELF=${TOPDIR}/vela_$(CONFIG_ARCH_BOARD_CUSTOM_NAME).bin; \
	if [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xap" ]; then \
		$(STRIP) ${TOPDIR}/nuttx -o $$VELA_ELF; \
		AVB_PARTITION_PATH=$${TOPDIR}/$(CONFIG_ARCH_BOARD_PARTITION_TABLE); \
		${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $$VELA_ELF 0 -o --dynamic_partition_size -P /dev/ap -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_AP_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 0"; \
	elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xtee" ]; then \
		$(STRIP) ${TOPDIR}/nuttx -o $$VELA_ELF; \
		${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $$VELA_ELF 0 -o --dynamic_partition_size -P /dev/tee -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_TEE_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 1"; \
	elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xota" ]; then \
		$(STRIP) ${TOPDIR}/nuttx -o $$VELA_ELF; \
		${TOPDIR}/../frameworks/system/ota/tools/avb_sign.sh $$VELA_ELF 0 -o --dynamic_partition_size -P /ota/vela_ota.bin -o "--block_size $$((128*1024))" -o "--rollback_index $(CONFIG_X4B_OTA_AVB_ROLLBACK_INDEX)" -o "--rollback_index_location 2"; \
	elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xfactest" ]; then \
		$(STRIP) ${TOPDIR}/nuttx -o $$VELA_ELF; \
	elif [ "x$(CONFIG_ARCH_BOARD_CUSTOM_NAME)" = "xbl" ]; then \
		cp -v ${TOPDIR}/nuttx.bin $$VELA_ELF; \
	fi
endef
