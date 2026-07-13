#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sunxi_hal_efuse.h>
#include <hal_cmd.h>
#include <debug.h>

#undef  HEXDUMP_LINE_CHR_CNT
#define HEXDUMP_LINE_CHR_CNT 16

static int sunxi_hexdump(const unsigned char *buf, int bytes)
{
    unsigned char line[HEXDUMP_LINE_CHR_CNT] = {0};
    int addr;

    for (addr = 0; addr < bytes; addr += HEXDUMP_LINE_CHR_CNT)
    {
        int len = ((bytes-addr)<HEXDUMP_LINE_CHR_CNT ? (bytes-addr) : HEXDUMP_LINE_CHR_CNT);
	int i;

        memcpy(line, buf + addr, len);
        memset(line + len, 0, HEXDUMP_LINE_CHR_CNT - len);

        /* print addr */
        cryptinfo("0x%.8X: ", addr);
        /* print hex */
        for (i = 0; i < HEXDUMP_LINE_CHR_CNT; i++)
        {
            if (i < len)
            {
                cryptinfo("%.2X ", line[i]);
            }
            else { cryptinfo("   "); }
        }
        /* print char */
        cryptinfo("|");
        for (i = 0; i < HEXDUMP_LINE_CHR_CNT; i++)
        {
            if (i < len)
            {
                if (line[i] >= 0x20 && line[i] <= 0x7E)
                {
                    cryptinfo("%c", line[i]);
                }
                else
                {
                    cryptinfo(".");
                }
            }
            else
            {
                cryptinfo(" ");
            }
        }
        cryptinfo("|\n");
    }
    return 0;
}
#ifdef CONFIG_KERNEL_FREERTOS
int cmd_test_efuse(int argc, char **argv)
#elif defined(CONFIG_OS_NUTTX)
int main(int argc, char **argv)
#endif

{
	char buffer[16] = {0};
	char buffer2[12] = {0};
	char buffer3[1] = {0};
	char buffer4[3] = {0};
	char buffer5[3] = {0};
	char buffer6[3] = {0};
	cryptinfo("==========TEST READ CHIPID=========\n");
	hal_efuse_get_chipid((unsigned char *)buffer);
	sunxi_hexdump((unsigned char *)buffer, sizeof(buffer));

	cryptinfo("==========TEST READ MAC============\n");
	hal_efuse_get_mac((unsigned char *)buffer2);
	cryptinfo("MAC:\n");
	sunxi_hexdump((unsigned char *)buffer2, sizeof(buffer2));
	hal_efuse_get_mac_version((unsigned char *)buffer3);
	cryptinfo("MAC_version:\n");
	sunxi_hexdump((unsigned char *)buffer3, sizeof(buffer3));
	hal_efuse_get_mac1((unsigned char *)buffer4);
	cryptinfo("MAC1:\n");
	sunxi_hexdump((unsigned char *)buffer4, sizeof(buffer4));
	hal_efuse_get_mac2((unsigned char *)buffer5);
	cryptinfo("MAC2:\n");
	sunxi_hexdump((unsigned char *)buffer5, sizeof(buffer5));
	hal_efuse_get_mac3((unsigned char *)buffer6);
	cryptinfo("MAC3:\n");
	sunxi_hexdump((unsigned char *)buffer6, sizeof(buffer6));

	/* TODO: add more APIs to test */
	cryptinfo("===================================\n");
	cryptinfo("Test Finished.\n");

	return 0;
}
#ifdef CONFIG_KERNEL_FREERTOS
FINSH_FUNCTION_EXPORT_CMD(cmd_test_efuse, hal_efuse, efuse hal APIs tests)
#endif
