#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sunxi_secure_storage.h"
#include "hal_mem.h"
#include <debug.h>
#include "triad_ca_api.h"

/*
static void sunxi_dump(const void *addr, unsigned int len)
{
	unsigned int i;
	const unsigned char *p = addr;
	len++;

	for (i = 1; i < len; ++i) {
		sec_info("%02x ", *p++);
		if (i % 16 == 0) {
			sec_info("\r\n");
		}
	}
	sec_info("\r\n");
}

*/

static int sunxi_secure_storage_list(void)
{
	int ret = -1;
	int index = 1;
	int data_len = 0;
	char name[64];
	char str[64] = {0};
	char *buffer = hal_malloc(4096);

	if(!buffer) {
		sec_err("<%s:%d> hal_malloc failed!\n", __func__, __LINE__);
		return -1;
	}

	sec_info("[secure storage]\n");

	while(!sunxi_secure_storage_get_name_by_index(index, name, 64)){
		memset(buffer, 0, 4096);
		ret = sunxi_secure_storage_read(name, buffer, 4096, &data_len);
        if(!ret) {
			snprintf(str, data_len + 1, "%s", buffer);
			sec_info("%d: %s = %s\n", index, name, str);
		//	sunxi_dump(buffer, data_len);
		} else {
			sec_err("read %s failed!\n", name);
			goto exit;
		}
		index++;
	}
#if defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
	uint8_t did[8] = {0}, key[16] = {0};
	int ret_did = -1, ret_key = -1;
	char miio_key[16] = {0};
	ret_did = triad_load_did(did, 8);
	ret_key = triad_load_key(key, 16);
	if (!ret_did) {
		sec_info("miio_did = %u%u%u%u%u%u%u%02" PRIx8 "\n",
			did[0], did[1], did[2], did[3], did[4], did[5], did[6], did[7]);
		index ++;
	}
	if (!ret_key) {
		for (int i = 0; i < 16; i++)
			miio_key[i] = (char)key[i];
		sec_info("miio_key = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
				miio_key[0], miio_key[1], miio_key[2], miio_key[3], miio_key[4], miio_key[5], miio_key[6], miio_key[7],
				miio_key[8], miio_key[9], miio_key[10], miio_key[11], miio_key[12], miio_key[13], miio_key[14], miio_key[15]);
		index ++;
	}
#endif

exit:
	sec_info("end index: %d\n", index - 1);
	hal_free(buffer);
	return ret;
}

static void usage(void)
{
	sec_info("usage: mikey get|set|erase <key_name> <key_file>\n");
	sec_info(" \tmikey set <key_name> <key_string>\t write key named [key_name] with key string\n");
	sec_info(" \tmikey get <key_name>\t read key named [key_name]\n");
	sec_info(" \tmikey list\t read all key in secure storage\n");
#ifdef CONFIG_X4B_FACTEST
	sec_info(" \tmikey erase <key_name/all>\t erase key named <key_name/all> in secure storage\n");
#endif
}

int main(int argc, char *const argv[])
{
	int ret = -1;
	int data_len = 0;
	char str[64] = {0};

	if (argc > 4 || argc < 2) {
		sec_err("wrong argc\n");
		usage();
		return -1;
	}

	if (sunxi_secure_storage_init() < 0) {
		sec_err("%s secure storage init err\n", __func__);
		return -1;
	}

	if (argc == 3 && !strncmp("erase", argv[1], strlen("erase"))) {
#ifdef CONFIG_X4B_FACTEST
		if (!strncmp("all", argv[2], strlen("all"))) {
			ret = sunxi_secure_storage_erase_all();
			if (!ret) {
				sec_info("secure storage erase all ok!\n");
			}
		}
		else {
			ret = sunxi_secure_storage_erase_data_only(argv[2]);
			if (ret < 0) {
				sec_err("%s secure storage erase err\n", __func__);
				return -1;
			}
		}
#else
		sec_info("Do not allow to erase mikey except factest!\n");
#endif
	} else if (argc == 2 && !strncmp("list", argv[1], strlen("list"))) {

		ret = sunxi_secure_storage_list();

	} else if (argc == 3 && !strncmp("get", argv[1], strlen("get"))) {
#if defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
		if (!strncmp("miio_did", argv[2], strlen("miio_did"))) {
			uint8_t did[8] = {0};
			ret = triad_load_did(did, 8);
			if (!ret) {
				sec_info("get miio_did key data: %u%u%u%u%u%u%u%" PRIx8 "\n",
					did[0], did[1], did[2], did[3], did[4], did[5], did[6], did[7]);
				return ret;
			}
		 } else if (!strncmp("miio_key", argv[2], strlen("miio_key"))) {
			uint8_t key[16] = {0};
			char miio_key[16] = {0};
			ret = triad_load_key(key, 16);
			for (int i = 0; i < 16; i++)
				miio_key[i] = (char)key[i];
			if (!ret) {
				sec_info("get miio_key key data: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
					miio_key[0], miio_key[1], miio_key[2], miio_key[3], miio_key[4], miio_key[5], miio_key[6], miio_key[7],
					miio_key[8], miio_key[9], miio_key[10], miio_key[11], miio_key[12], miio_key[13], miio_key[14], miio_key[15]);
				return ret;
			}
		}
#endif
		char *buffer = hal_malloc(4096);

		if (!buffer) {
			sec_err("<%s:%d> hal_malloc failed!\n", __func__, __LINE__);
			ret = -1;
			goto exit;
		}
		memset(buffer, 0, 4096);
		ret = sunxi_secure_storage_read(argv[2], buffer, 4096, &data_len);
		if (!ret) {
			//sunxi_dump(buffer, data_len);
			snprintf(str, data_len + 1, "%s", buffer);
			sec_info("get %s key data(%d): %s\n", argv[2], data_len, str);
		} else {
			sec_err("read %s failed!\n", argv[2]);
		}
		hal_free(buffer);
	} else if (argc == 4 && !strncmp("set", argv[1], strlen("set"))) {
#if defined(CONFIG_X4B_FACTEST)
		if (!strncmp("miio_did", argv[2], strlen("miio_did"))) {
			uint8_t did[8] = {0};
			for (uint8_t i = 0; i < 8; i ++) {
				did[i] = argv[3][i] - '0';
			}
			if (strlen(argv[3]) == 9)
				did[7] = (argv[3][7] - '0') * 16 + (argv[3][8] - '0');
			ret = triad_store_did(did, 8);
			if (!ret) {
				sec_info("set miio_did key success!\n");
				return ret;
			}
		} else if (!strncmp("miio_key", argv[2], strlen("miio_key"))) {
			uint8_t key[16] = {0};
			for (uint8_t i = 0; i < 16; i ++) {
				key[i] = (uint8_t)argv[3][i];
			}
			ret = triad_store_key(key, 16);
			if (!ret) {
				sec_info("set miio_key key success!\n");
				return ret;
			}
		}

		ret = sunxi_secure_storage_write(argv[2], argv[3], strlen(argv[3]));

		if (!ret)
			sec_info("set %s key success!\n", argv[2]);
		else
			sec_err("set %s key failed!\n", argv[2]);
#else
		sec_info("Do not allow to set mikey except factest!\n");
#endif
	} else {
		usage();
	}

	if (sunxi_secure_storage_exit() < 0) {
		sec_err("secure storage exit fail\n");
		goto exit;
	}

exit:
	return ret;
}
