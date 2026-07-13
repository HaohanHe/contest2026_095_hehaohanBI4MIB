/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sound/snd_core.h>
#include "snd_sunxi_alg_cfg.h"

static void print_help_msg(void)
{
	printf("Usage:\n");
	printf("\n");
	printf("USAGE:\n");
	printf("  test_alg_cfg [OPTIONS] config_file\n");
	printf("OPTIONS:\n");
	printf("  -h          : print help message\n");
	printf("  -t          : specified alg type (0: MEQ, 1: PEQ, 2: 1BDRC, 3: 3BDRC, 4: HPF)\n");
	printf("  -l          : list reg val of specified specified alg type\n");
	printf("\n");
	printf("e.g.\n");
	printf("      test_alg_cfg -t 2 /data/DACDRC.conf\n");
	printf("      test_alg_cfg -t 2 -l\n");
	printf("\n");
}

void sunxi_alg_cfg_reg_show(struct alg_cfg_reg_domain *domain)
{
	unsigned int i, reg_val;

	printf("reg_base -> %p, reg range -> 0x%x to 0x%x\n",
		domain->reg_base, domain->reg_min, domain->reg_max);

	for (i = domain->reg_min; i <= domain->reg_max; i += 4) {
		reg_val = snd_readl(domain->reg_base + i);
		printf("reg: %p, reg_val: 0x%08x\n",
			domain->reg_base + i, reg_val);
	}

}

int main(int argc, char *argv[])
{
	int ret;
	int c;
	int do_list = 0;
	int type = 0;
	char *file_path = NULL;
	struct alg_cfg_reg_domain *domain;

	if (argc < 2) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	while ((c = getopt(argc, argv, "ht:l")) != -1) {
		switch(c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 'l':
			do_list = 1;
			break;
		case 't':
			type = atoi(optarg);
			if (type > SUNXI_ALG_CFG_DOMAIN_HPF) {
				ret = -1;
				printf("Invalid alg type arg.\r\n");
				goto out;
			}
			break;
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	ret = sunxi_alg_cfg_domain_get(&domain, type);
	if (ret) {
		printf("sunxi_alg_cfg_domain_get domain get failed\n");
		ret = -1;
		goto out;
	}

	if (!domain->reg_base || !domain->reg_min || !domain->reg_max) {
		printf("reg error, check if this domain is initialized\n");
		ret = -1;
		goto out;
	}

	if (domain->reg_min > domain->reg_max) {
		printf("reg range error\n");
		ret = -1;
		goto out;
	}

	if (do_list) {
		sunxi_alg_cfg_reg_show(domain);
		return 0;
	}

	file_path = argv[argc - 1];
	if (!strstr(file_path, ".conf")) {
		printf("%s is not config file\n", file_path);
		return -1;
	}

	ret = sunxi_alg_cfg(file_path, domain);
	if (ret) {
		printf("sunxi_alg_cfg failed\n");
		return -1;
	} else {
		printf("set alg_cfg success, you can play music to check.\n");
	}

out:
	return ret;
}
