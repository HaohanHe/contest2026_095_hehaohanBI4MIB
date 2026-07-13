/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *

 */
#include <common.h>
#include <malloc.h>
#include <private_toc.h>
#include <sprite_verify.h>
#include "../nand_bsp.h"
#include <sunxi_nand.h>
#include <sunxi_nand_partitions.h>
#include <fdt_support.h>

#define PHY_SPACE_MAP_TO_LOGIC_SPACE

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
//#include "nand_common_interface.h"
#undef ERR_TIMEOUT
#include "sunxi_nand_errno.h"

struct nand_phy_part {
	char path[30];
	char by_name[30];
	unsigned int index;
	size_t start_page;
	size_t npage;
	/*          scan      erase        first write            */
	/* not init  ->  used  ->   erased      ->     in program */
#define PHY_PART_STATE_USED			(0)
#define PHY_PART_STATE_ERASED		(1)
#define PHY_PART_STATE_IN_PROG		(2)
	size_t state;
#define BAD_BLK_BIT	(1<<0)
	unsigned char *blk_info;
	unsigned int bad_blk_cnt;
};

extern __u32 NAND_GetPageCntPerBlk(void);
extern __u32 NAND_GetPageSize(void);

static long long logic_sector_size = (256 * 1024 * 1024 / 512);
static long long phy_sector_offset = 0 - (256 * 1024 * 1024 / 512);
size_t phy_npart = 0;
struct nand_phy_part *phy_parts;


#ifndef NAND_MAX_SPARE_SIZE
#define NAND_MAX_SPARE_SIZE (256)
#endif

#define NAND_PHY_PATH	"/dev/nand1"

#endif

#ifndef SZ_4K
#define SZ_4K				0x00001000
#endif

#define NAND_BOOT0_BLK_START			0
#define NAND_BOOT0_BLK_CNT				2
#define NAND_UBOOT_BLK_START			(NAND_BOOT0_BLK_START+NAND_BOOT0_BLK_CNT)
#define NAND_UBOOT_BLK_CNT				6
#define NAND_BOOT0_PAGE_CNT_PER_COPY	64


static char nand_para_store[256];
static int  flash_scaned;
static struct _nand_info *g_nand_info;
static int nand_partition_num;
int nandphy_had_init;

extern int nand_open_count;
extern int nand_open_times;

int mbr_burned_flag;
PARTITION_MBR nand_mbr = {0};
extern __u32 boot_mode;
extern void *NAND_Malloc(unsigned int Size);
extern void NAND_Free(void *pAddr, unsigned int Size);
extern int NAND_Print(const char *str, ...);
extern int NAND_Print_DBG(const char *str, ...);
extern int NAND_set_boot_mode(__u32 boot);
__u32 NAND_GetNandCapacityLevel(void);
extern int get_uboot_start_block(void);
extern int get_uboot_next_block(void);
extern __u32 get_storage_type(void);
extern  int rawnand_is_blank(void);
extern void nand_common1_show_version(void);
extern int get_boot_work_mode(void);
#if defined(CONFIG_MACH_SUN8IW7) || defined(CONFIG_MACH_SUN8IW18) \
	|| (defined(CONFIG_SUNXI_RTOS) && (defined(CONFIG_MACH_SUN8IW20) || defined(CONFIG_MACH_SUN20IW1)))
	extern __u32 NAND_GetPageSize(void);
#endif

extern int get_boot_work_mode(void);

#if defined(CONFIG_MACH_SUN8IW7) || defined(CONFIG_MACH_SUN8IW18) \
	|| (defined(CONFIG_SUNXI_RTOS) && (defined(CONFIG_MACH_SUN8IW20) || defined(CONFIG_MACH_SUN20IW1)))
int nand_info_init(struct _nand_info *nand_info, uchar chip, uint16 start_block, uchar *mbr_data);
#else
extern int nand_info_init(struct _nand_info *nand_info, int state);
#endif

struct uboot_status ubootsta;

struct nand_partitions nand_parts;

struct nand_partitions *get_nand_parts(void)
{
	return &nand_parts;
}

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE

struct nand_phy_part *get_phy_partition_info(uint nSectNum, uint nSectorCnt)
{
	size_t i;
	struct nand_phy_part *part;
	unsigned int page_size = NAND_GetPageSize();

	if (!phy_parts || !phy_npart) {
		NAND_Print("%s: no phy partition info\n", __func__);
		return NULL;
	}

	nSectNum += phy_sector_offset;

	if ((nSectNum * 512) % page_size) {
		NAND_Print("%s: nSectNum(%u) not align to page(%u)\n", __func__, nSectNum, page_size);
		return NULL;
	}

	for (i = 0; i < phy_npart; i++) {
		part = &phy_parts[i];
		if ((nSectNum * 512 / page_size) < part->start_page)
			continue;
		if (((nSectNum + nSectorCnt) * 512 / page_size) > (part->start_page + part->npage))
			continue;
		return part;
	}

	return NULL;
}

int phy_partition_scan(struct nand_phy_part *part)
{
	unsigned int page_cnt_per_blk = NAND_GetPageCntPerBlk();
	unsigned int start_blk, nblk, block, bad_blk = 0;

	if (!part || !part->blk_info)
		return 0;

	start_blk = part->start_page / page_cnt_per_blk;
	nblk = part->npage / page_cnt_per_blk;

	for (block = start_blk; block < (start_blk + nblk); block++) {
		if (nand_physic_bad_block_check(0, block)) {
			bad_blk++;
			part->blk_info[block - start_blk] |= BAD_BLK_BIT;
		}
#if 0 // make feak bad block, for test
		else if ((block - start_blk) > 0 && 0 == ((block - start_blk) & (block - start_blk - 1))) {
			bad_blk++;
			part->blk_info[block - start_blk] |= BAD_BLK_BIT;
		}
#endif
	}

	part->bad_blk_cnt = bad_blk;
	return bad_blk;
}

int get_phy_partition_blk_offset(struct nand_phy_part *part, uint block)
{
	size_t i;
	unsigned int page_cnt_per_blk = NAND_GetPageCntPerBlk();
	unsigned int start_blk, nblk;
	uint offset = 0;

	if (!part || !part->blk_info)
		return 0;

	start_blk = part->start_page / page_cnt_per_blk;
	nblk = part->npage / page_cnt_per_blk;
	if (block < start_blk || block >= (start_blk + nblk)) {
		NAND_Print("%s: blk%u out of phy partition blk range: [%u, %u)\n", __func__, block,
			start_blk, nblk);
		return 0;
	}

	block -= start_blk;

	for (i = 0; i <= block; i++) {
		if (part->blk_info[i] & BAD_BLK_BIT) {
			offset++;
			block++;
		}

	}

	if ((block + offset) > nblk) {
		NAND_Print("%s: blk%u(%u + %u) out of phy partition blk range: [%u, %u)\n", __func__,
			block + offset, block, offset, start_blk, nblk);
		return -1;
	}

	return offset;
}

int is_phy_sector(uint nSectNum, uint nSectorCnt)
{
	if (nSectNum > logic_sector_size)
		return 1;

	return 0;
}

int phy_sector_to_pa(uint nSectNum, uint *chip, uint *block, uint *page)
{
	unsigned int page_size = NAND_GetPageSize();
	unsigned int page_cnt_per_blk = NAND_GetPageCntPerBlk();
	unsigned int pa;

	nSectNum += phy_sector_offset;

	// check align
	if ((nSectNum * 512) % page_size)
		return -1;

	pa = (nSectNum * 512) / page_size;
	*chip = 0;
	*block = pa / page_cnt_per_blk;
	*page = pa % page_cnt_per_blk;

	return 0;
}

int phy_read(uint nSectNum, uint nSectorCnt, void *pBuf)
{
	unsigned char spare[NAND_MAX_SPARE_SIZE];
	unsigned int sector_cnt_per_page = NAND_GetPageSize() / 512;
	unsigned int chip, block_raw, block_off, block, page, sector;
	unsigned char *mbuf;
	struct nand_phy_part *part;
	int ret;

	/* */ NAND_Print_DBG("%s: nSectNum: %u, nSectorCnt: %u\n", __func__, nSectNum, nSectorCnt);

	part = get_phy_partition_info(nSectNum, nSectorCnt);
	if (!part)
		NAND_Print("%s: can get phy partition info\n", __func__);

	for (sector = 0; sector < nSectorCnt; sector += sector_cnt_per_page) {
		if (phy_sector_to_pa(nSectNum + sector, &chip, &block_raw, &page)) {
			NAND_Print("%s: phy_sector_to_pa %u error\n", __func__, nSectNum + sector);
			goto err_out;
		}
		ret = get_phy_partition_blk_offset(part, block_raw);
		if (ret < 0) {
			goto err_out;
		}
		block_off = ret;
		block = block_raw + block_off;
		/* */ NAND_Print_DBG("%s: block%u(%u+%u) page%u\n", __func__, block, block_raw, block_off, page);
		if (nand_physic_bad_block_check(chip, block)) {
			NAND_Print("%s: block%u page%u bad block\n", __func__, block, page);
			goto err_out;
		}
		mbuf = sector * 512 + (unsigned char *)pBuf;
		memset(spare, 0, NAND_MAX_SPARE_SIZE);
		ret = nand_physic_read_page(chip, block, page, sector_cnt_per_page, mbuf, spare);
		if (ret == ERR_ECC) {
			NAND_Print("%s: block%u page%u ecc error\n", __func__, block, page);
			goto err_out;
		}
	}

	return 0;
err_out:
	return -1;
}

#if 0
static void hex_dump(unsigned long addr, unsigned long len)
{
	int i = 0;

	printf("addr:0x%08lx, len: 0x%lx", addr, len);
	for (i = 0; i < (len / 4); i++) {
		unsigned long cur_addr = addr + i * 4;

		if (!(i % 4))
			printf("\n0x%08lx: %08lx ", cur_addr, *(unsigned long *)cur_addr);
		else
			printf("%08lx ", *(unsigned long *)cur_addr);
	}
	printf("\n");
}
#endif

int page_is_writen(uint chip, uint block, uint page, unsigned char *mbuf, unsigned char *spare)
{
	unsigned int i, page_size = NAND_GetPageSize();
	int ret;

	memset(mbuf, 0xa5, page_size);
	memset(spare, 0x5a, NAND_MAX_SPARE_SIZE);
	ret = nand_physic_read_page(chip, block, page, page_size / 512, mbuf, spare);
	if (ret == ERR_ECC) {
		NAND_Print("%s: block%u page%u ecc error\n", __func__, block, page);
		return -1;
	}
	for (i = 0; i < page_size; i++) {
		if (mbuf[i] != 0xff) {
#if 0
			NAND_Print("%s: block%u page%u is writen\n", __func__, block, page);
			NAND_Print("%s: mbuf:\n", __func__);
			hex_dump((unsigned long)mbuf, (unsigned long)page_size);
			NAND_Print("%s: spare:\n", __func__);
			hex_dump((unsigned long)spare, (unsigned long)NAND_MAX_SPARE_SIZE);
#endif
			return -1;
		}
	}

	return 0;
}

__attribute__((aligned(64))) unsigned char g_tmpbuf[4096];
__attribute__((aligned(64))) unsigned char g_tmpspare[NAND_MAX_SPARE_SIZE];
int phy_partition_erase(struct nand_phy_part *part)
{
	uint chip, block, start_block, nblock, i, page, need_erase;
	unsigned int page_cnt_per_blk = NAND_GetPageCntPerBlk();

	chip = 0;
	start_block = part->start_page / page_cnt_per_blk;
	nblock = part->npage / page_cnt_per_blk;

	for (i = 0; i < nblock; i++) {
		if (part->blk_info[i] & BAD_BLK_BIT)
			continue;
		block = i + start_block;
		need_erase = 0;
		for (page = 0; page < page_cnt_per_blk; page++) {
			if (page_is_writen(chip, block, page, g_tmpbuf, g_tmpspare)) {
				need_erase = 1;
				break;
			}
		}
		if (!need_erase)
			continue;
		if (nand_physic_erase_block(chip, block)) {
			NAND_Print("%s: erase block%u failed\n", __func__, block);
		}
	}
	part->state = PHY_PART_STATE_ERASED;

	return 0;
}

int phy_partition_write_perpare(struct nand_phy_part *part)
{
	if (!part)
		return -1;
	if (part->state == PHY_PART_STATE_IN_PROG)
		return 0;
	if (part->state == PHY_PART_STATE_ERASED) {
		part->state = PHY_PART_STATE_IN_PROG;
		return 0;
	}
	// PHY_PART_STATE_USED
	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		NAND_Print("erase %s <- %s for writing\n", (const char *)part->path, (const char *)part->by_name);
		phy_partition_erase(part);
		part->state = PHY_PART_STATE_IN_PROG;
		NAND_Print("ready to write\n");
		return 0;
	}

	return 0;
}

int phy_write(uint nSectNum, uint nSectorCnt, void *pBuf)
{
	unsigned char spare[NAND_MAX_SPARE_SIZE];
	unsigned int sector_cnt_per_page = NAND_GetPageSize() / 512;
	unsigned int chip, block_raw, block_off, block, page, sector;
	unsigned char *mbuf;
	struct nand_phy_part *part;
	int ret;

	/* */ NAND_Print_DBG("%s: nSectNum: %u, nSectorCnt: %u\n", __func__, nSectNum, nSectorCnt);

	if (sector_cnt_per_page == 0) {
		NAND_Print("%s: sector_cnt_per_page error, %u\n", __func__, sector_cnt_per_page);
	}

	part = get_phy_partition_info(nSectNum, nSectorCnt);
	if (!part)
		NAND_Print("%s: can get phy partition info\n", __func__);

	phy_partition_write_perpare(part);

	for (sector = 0; sector < nSectorCnt; sector += sector_cnt_per_page) {
		//NAND_Print("%s: phy_sector %u\n", __func__, nSectNum + sector);
		if (phy_sector_to_pa(nSectNum + sector, &chip, &block_raw, &page)) {
			NAND_Print("%s: phy_sector_to_pa %u error\n", __func__, nSectNum + sector);
			goto err_out;
		}
		ret = get_phy_partition_blk_offset(part, block_raw);
		if (ret < 0) {
			goto err_out;
		}
		block_off = ret;
		block = block_raw + block_off;
		if (part->bad_blk_cnt && block_off && page == 0)
			NAND_Print("%s: block%u(%u+%u) page%u\n", __func__, block, block_raw, block_off, page);
		if (nand_physic_bad_block_check(chip, block)) {
			NAND_Print("%s: block%u page%u bad block\n", __func__, block, page);
			goto err_out;
		}
		if (page_is_writen(chip, block, page, g_tmpbuf, spare)) {
			NAND_Print("%s: block%u page%u is writen\n", __func__, block, page);
#if 0
			goto err_out;
#else // for part download
			NAND_Print("%s: erase block%u to write\n", __func__, block);
			if (nand_physic_erase_block(chip, block)) {
				NAND_Print("%s: erase block%u failed\n", __func__, block);
				goto err_out;
			}
#endif
		}
		mbuf = sector * 512 + (unsigned char *)pBuf;
		memset(spare, 0xff, NAND_MAX_SPARE_SIZE);
		ret = nand_physic_write_page(chip, block, page, sector_cnt_per_page, mbuf, spare);
		if (ret == ERR_ECC) {
			NAND_Print("%s: block%u page%u ecc error\n", __func__, block, page);
			goto err_out;
		}
	}

	return 0;
err_out:
	return -1;
}

int init_phy_partition_info(struct _nand_info *nand_info)
{
	struct _nand_phy_partition *phy_part = nand_info->phy_partition_head;
	struct _nand_disk *disk, *disks = phy_part->disk;
	size_t used_sects = 0, sector_per_page = nand_info->SectorNumsPerPage, page_per_blk = nand_info->PageNumsPerBlk;
	int index_start, index;
	struct nand_phy_part *part;
	int bad_blk;
	int nodeoffset;
	uint32_t need_erase_flag = 0;
	size_t state = PHY_PART_STATE_USED;

	if (phy_npart) {
		NAND_Print("%s: scanned already\n", __func__);
		return 0;
	}

	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		nodeoffset = fdt_path_offset (working_fdt,"/soc/platform");
		fdt_getprop_u32(working_fdt, nodeoffset, "eraseflag", &need_erase_flag);

		switch (need_erase_flag) {
		case 0x12: // force erase all
		case 0x11: // force erase format
		case 0x1: // erase format
			state = PHY_PART_STATE_ERASED;
			break;
		case 0x0: // erase normal
			state = PHY_PART_STATE_USED;
			break;
		default:
			NAND_Print("%s: unknown eraseflag: 0x%x\n", __func__, need_erase_flag);
			state = PHY_PART_STATE_USED;
			break;
		}
	}
	NAND_Print("%s: state: %u\n", __func__, state);

	// skip logic partition
	index = 0;
	for (disk = &disks[index]; disk->name[0] != 0xFF; disk = &disks[++index]) {
		used_sects += disk->size;
		if (!strncmp((const char *)disk->name, "dummy", 5))
			break;
	}

	if (disks[index].name[0] == 0xFF) {
		NAND_Print("%s: not found phy partition\n", __func__);
		logic_sector_size = 0x7fffffffffffffff; // LONG_LONG_MAX
		phy_sector_offset = -logic_sector_size;
		phy_npart = 0;
		return 0;
	}

	// The phy partition is located behind the dummy partition
	logic_sector_size = used_sects;
	phy_sector_offset = -logic_sector_size;
	index++;
	index_start = index;

	for (disk = &disks[index]; disk->name[0] != 0xFF; disk = &disks[++index]) {
		if (!strncmp((const char *)disk->name, "UDISK", 6))
			break;
		phy_npart++;
	}

	NAND_Print("detected %u phy partition, offset: %x\n",
		(unsigned int)phy_npart, (unsigned int)(-phy_sector_offset));

	phy_parts = NAND_Malloc(phy_npart * sizeof(struct nand_phy_part));
	if (!phy_parts)
		return -ENOMEM;
	memset(phy_parts, 0, phy_npart * sizeof(struct nand_phy_part));

	used_sects = 0;
	for (index = 0; index < phy_npart; index++) {
		part = &phy_parts[index];
		disk = &disks[index + index_start];
		if (disk->size % (sector_per_page * page_per_blk)) {
			NAND_Print("%s: partition size not align!\n", __func__);
			NAND_Print("%s: %s sector range: [%u, %u) (%u)\n", __func__,
				(const char *)disk->name, (unsigned int)used_sects,
				(unsigned int)(used_sects + disk->size), (unsigned int)disk->size);
			goto err_out;
		}

		part->index = index + 1;
		part->start_page = used_sects / sector_per_page;
		part->npage = disk->size / sector_per_page;
		used_sects += disk->size;

		part->blk_info = NAND_Malloc(sizeof(part->blk_info) * part->npage / page_per_blk);
		if (!part->blk_info)
			return -ENOMEM;
		memset(part->blk_info, 0, sizeof(part->blk_info) * part->npage / page_per_blk);
		bad_blk = phy_partition_scan(part);
		part->state = state;

		snprintf(part->path, sizeof(part->path), "%sp%d", NAND_PHY_PATH, part->index);
		snprintf(part->by_name, sizeof(part->by_name), "/dev/%s", disk->name);
		NAND_Print("page range: [%u, %u) (%u), %s <- %s, bad blk: %d\n",
			(unsigned int)part->start_page,
			(unsigned int)(part->start_page + part->npage), (unsigned int)part->npage,
			(const char *)part->path, (const char *)part->by_name, (int)bad_blk);
	}

	return 0;
err_out:
	return -1;
}
#endif

__u32 __attribute__((weak)) NAND_GetBootFlag(__u32 flag)
{
	return 0;
}

int __NAND_UpdatePhyArch(void)
{
	NAND_Print("call null __NAND_UpdatePhyArch()!!!\n");
    return 0;
}
int NAND_UpdatePhyArch(void)
	__attribute__((weak, alias("__NAND_UpdatePhyArch")));


int msg(const char *str, ...)
{
	NAND_Print(str);
	return 0;
}

void nand_get_ubootstat(void)
{
	if (get_boot_work_mode() != WORK_MODE_BOOT)
		ubootsta.state = UBOOT_IN_PRODUCT;
	else
		ubootsta.state = UBOOT_IN_BOOT;
}

int NAND_PhyInit(void)
{
	struct _nand_info *nand_phy_info;

	nand_common1_show_version();

	NAND_Print("NB1 : enter phy init\n");

	nand_phy_info = NandHwInit();
	if (nand_phy_info == NULL) {
		NAND_Print("NB1 : nand phy init fail\n");
		return -1;
	}

	nandphy_had_init = true;
	NAND_Print("NB1 : nand phy init ok\n");
	return 0;

}
int NAND_PhyExit(void)
{
	NAND_Print("NB1 : enter phy Exit\n");
	NandHwExit();
	nandphy_had_init = false;
	return 0;
}

int NAND_LogicWrite(uint nSectNum, uint nSectorCnt, void *pBuf)
{
	//NAND_Print("NB1: write(%u, %u, )\n", (unsigned int)nSectNum, (unsigned int)nSectorCnt);

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	if (is_phy_sector(nSectNum, nSectorCnt))
		return phy_write(nSectNum, nSectorCnt, pBuf);
#endif

	return nftl_write(nSectNum, nSectorCnt, pBuf);
}

int NAND_LogicRead(uint nSectNum, uint nSectorCnt, void *pBuf)
{
	//NAND_Print("NB1: read(%u, %u, )\n", (unsigned int)nSectNum, (unsigned int)nSectorCnt);

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	if (is_phy_sector(nSectNum, nSectorCnt))
		return phy_read(nSectNum, nSectorCnt, pBuf);
#endif

	return nftl_read(nSectNum, nSectorCnt, pBuf);
}

int NAND_LogicRead_History(uint nSectNum, uint nSectorCnt, void *pBuf)
{
	return nftl_read_history(nSectNum, nSectorCnt, pBuf);
}


extern void do_nand_interrupt(u32 no);
int NAND_LogicInit(int boot_mode)
{
	__s32  result = 0;
	__s32 ret = -1;
	__s32 i, nftl_num, capacity_level;
	struct _nand_info *nand_info;
	/* char* mbr; */

	NAND_Print("NB1: enter NAND_LogicInit\n");
	nand_common1_show_version();

#if defined(CONFIG_MACH_SUN8IW7)
	NAND_set_boot_mode(boot_mode);
#endif
	if (g_nand_info) {
		/* NAND_LogicExit() may not call NandHwExit */
		NAND_Print("NB1: nand phy already init\n");
		nand_info = g_nand_info;
	} else {
		nand_info = NandHwInit();
	}

	capacity_level = NAND_GetNandCapacityLevel();
	set_capacity_level(nand_info, capacity_level);

	g_nand_info = nand_info;
	if (nand_info == NULL) {
		NAND_Print("NB1: nand phy init fail\n");
		return ret;
	}
	nandphy_had_init = true;

#if defined(CONFIG_MACH_SUN8IW7) || defined(CONFIG_MACH_SUN8IW18) \
	|| (defined(CONFIG_SUNXI_RTOS) && (defined(CONFIG_MACH_SUN8IW20) || defined(CONFIG_MACH_SUN20IW1)))
	if ((!boot_mode) && (nand_mbr.PartCount != 0) && (mbr_burned_flag == 0)) {
		NAND_Print("burn nand partition table! mbr tbl: 0x%x, part_count:%d\n", (__u32)(unsigned long)(&nand_mbr), nand_mbr.PartCount);
		result = nand_info_init(nand_info, 0, 8, (uchar *)&nand_mbr);
		mbr_burned_flag = 1;
	} else {
		NAND_Print("not burn nand partition table!\n");
		result = nand_info_init(nand_info, 0, 8, NULL);
	}
#else
	result = nand_info_init(nand_info, ubootsta.state);
#endif
	if (result != 0) {
		NAND_Print("NB1: nand_info_init fail\n");
		return -5;
	}
	if (boot_mode) {
		nftl_num = get_phy_partition_num(nand_info);
		NAND_Print("NB1: nftl num: %d\n", nftl_num);
		if ((nftl_num < 1) || (nftl_num > 5)) {
			NAND_Print("NB1: nftl num: %d error\n", nftl_num);
			return -1;
		}

		nand_partition_num = 0;
		for (i = 0; i < ((nftl_num > 1)?(nftl_num-1):1); i++) {
			nand_partition_num++;
			NAND_Print("init nftl: %d\n", i);
			result = nftl_build_one(nand_info, i);
		}
	} else {
		result = nftl_build_all(nand_info);
		nand_partition_num = get_phy_partition_num(nand_info);
	}

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	init_phy_partition_info(nand_info);
#endif

	if (result != 0) {
		NAND_Print("NB1: nftl_build_all fail\n");
		return -5;
	}

	NAND_Print("NB1: NAND_LogicInit ok, result = 0x%x\n", result);

	return result;
}

int NAND_LogicExit(void)
{
	nftl_flush_write_cache();
#if 0 //defined(CONFIG_MACH_SUN8IW7)
	if (!boot_mode) {
		printf("burn write end !!\n");
		nftl_write_end();
	}
#endif

	/* burn step logic don't exit the phy layer, which exit after burn boot0*/
	printf("%s boot_mode:%d\n", __func__, get_boot_work_mode());
	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		NAND_Print("NB1: nand phy not deinit\n");
		return 0;
	}

	nand_open_times = 0;
	nand_open_count = 0;

	NandHwExit();
	nandphy_had_init = false;
	g_nand_info = NULL;

	return 0;
}

int NAND_build_all_partition(void)
{
	int result, i;
	int nftl_num;

	if (g_nand_info == NULL) {
		NAND_Print("NAND_build_all_partition fail\n");
		return -1;
	}

	nftl_num = get_phy_partition_num(g_nand_info);
	if (nftl_num == nand_partition_num)
		return 0;

	if ((nand_partition_num >= nftl_num) || (nand_partition_num == 0)) {
		NAND_Print("NAND_build_all_partition fail %d\n", nand_partition_num);
		return -1;
	}

	for (i = nand_partition_num; i < nftl_num; i++) {
		NAND_Print(" init nftl: %d\n", i);
		result = nftl_build_one(g_nand_info, i);
		if (result != 0) {
			NAND_Print("NAND_build_all_partition fail %d %d\n", result, i);
			return -1;
		}
	}

	return 0;
}

int NAND_VersionGet(unsigned char *version)
{
	__u32 nand_version;

	nand_version = nand_get_nand_version();
	/* bad block flag */
	version[0] = 0xff;
	/* reserved, set to 0x00 */
	version[1] = 0x00;
	/* nand driver version 0, current vresion is 0x02 */
	version[2] = (nand_version>>16);
	/* nand driver version 1, current vresion is 0x00 */
	version[3] = (nand_version>>24);

	return 0;
}

int NAND_VersionCheck(void)
{
	struct boot_physical_param boot0_readop;
	/*struct boot_physical_param *boot0_readop = NULL;*/
	uint block_index;
	int version_match_flag = -1;
	unsigned char  oob_buf[OOB_BUF_SIZE];
	unsigned char  nand_version[4];
	int uboot_start_block, uboot_next_block;

    /********************************************************************************
    *   nand_version[2] = 0xFF;          //the sequnece mode version <
    *   nand_version[2] = 0x01;          //the first interleave mode version, care ecc
    *                                      2010-06-05
    *   nand_version[2] = 0x02;          //the current version, don't care ecc
    *                                      2010-07-13
    *   NOTE:  need update the nand version in update_boot0 at the same time
	********************************************************************************/
	NAND_VersionGet(nand_version);

	uboot_start_block = get_uboot_start_block();
	uboot_next_block = get_uboot_next_block();
	NAND_Print_DBG("uboot_start_block %d uboot_next_block %d.\n", uboot_start_block, uboot_next_block);

	NAND_Print_DBG("check nand version start.\n");
	NAND_Print_DBG("Current nand driver version is %x %x %x %x\n", nand_version[0], nand_version[1], nand_version[2], nand_version[3]);


	/*init boot0_readop*/
	boot0_readop.block = 0x0;
	boot0_readop.chip = 0;

	boot0_readop.oobbuf = oob_buf;
	boot0_readop.page = 0;

	/*scan boot1 area blocks*/
	for (block_index = uboot_start_block; block_index < uboot_next_block; block_index++) {

		boot0_readop.block = block_index;
		nand_physic_read_page(boot0_readop.chip, boot0_readop.block,
				boot0_readop.page, 0, NULL, boot0_readop.oobbuf);

		/*check the current block is a bad block*/
		if (oob_buf[0] != 0xFF) {
			NAND_Print("block %u is bad block %x.\n", block_index, oob_buf[0]);
			continue;
		}

		if ((oob_buf[1] == 0x00) || (oob_buf[1] == 0xFF)) {
			NAND_Print("Media version is valid in block %u, version info is %x %x %x %x\n", block_index, oob_buf[0], oob_buf[1], oob_buf[2], oob_buf[3]);
			if (oob_buf[2] == nand_version[2]) {
				NAND_Print("nand driver version match ok in block %u.\n", block_index);
				version_match_flag = 0;
				break;
			} else {
				NAND_Print("nand driver version match fail in block %u.\n", block_index);
				version_match_flag = 1;
				break;
			}

		} else {
			NAND_Print("Media version is invalid in block %u, version info is %x %x %x %x\n", block_index, oob_buf[0], oob_buf[1], oob_buf[2], oob_buf[3]);
		}
	}

	if (block_index >= uboot_next_block) {
		NAND_Print("can't find valid version info in boot blocks.\n");
		version_match_flag = -1;
	}


	return version_match_flag;
}

/*extern void nand_special_test(void);*/
/* #define FORCE_ERASE_ALL_INCLUDE_UBOOT_BLOCKS */
int  NAND_EraseBootBlocks(void)
{
	int i, boot0_block_cnt;

	NAND_Print("has cleared the boot blocks.\n");
	/*nand_special_test();*/
	boot0_block_cnt = get_uboot_start_block();

	for (i = 0; i < boot0_block_cnt; i++) {
		nand_physic_erase_block(0, i);
	}
	return 0;
}

#ifdef FORCE_ERASE_ALL_INCLUDE_UBOOT_BLOCKS
int  NAND_EraseUbootBlocks(void)
{
	int i, uboot_start_block, uboot_end_block;

	uboot_start_block = get_uboot_start_block();
	uboot_end_block = get_uboot_next_block();
	NAND_Print("clear uboot from %d to %d.\n", uboot_start_block, uboot_end_block);
	for (i = uboot_start_block; i < uboot_end_block; i++) {
		nand_physic_erase_block(0, i);
	}
	NAND_Print("has cleared the uboot blocks.\n");
	return 0;
}
#endif

int  NAND_EraseChip(void)
{
	return nand_uboot_erase_all_chip(0);
}

int  NAND_EraseChip_force(void)
{
	return nand_uboot_erase_all_chip(1);
}

int NAND_BadBlockScan(void)
{
	return 0;
}

int NAND_UbootInit(int boot_mode)
{
	int ret = 0;

	int s = get_timer_masked();
	NAND_Print("NAND_UbootInit start\n");

	nand_get_ubootstat();
	NAND_set_boot_mode(boot_mode);
	/* logic init */
	ret |= NAND_LogicInit(boot_mode);

	if (!flash_scaned) {
		nand_get_param((boot_nand_para_t *)nand_para_store);
		flash_scaned = 1;
	}

	NAND_Print("NAND_UbootInit end: 0x%x, %dms\n", ret, get_timer_masked()-s);

	return ret;
}


int NAND_UbootExit(void)
{
	int ret = 0;
	NAND_Print("NAND_UbootExit\n");

	ret = NAND_LogicExit();

	return ret;
}

int NAND_UbootProbe(void)
{
	int ret = 0;

	NAND_Print_DBG("NAND_UbootProbe start\n");
#ifdef FORCE_ERASE_ALL_INCLUDE_UBOOT_BLOCKS
	NAND_Print_DBG("erase all begin\n");
	NAND_Uboot_Erase(1);
	NAND_Print_DBG("erase all end and just return FAIL directly\n");
	return -1;
#endif
	NAND_GetBootFlag(0);
	nand_get_ubootstat();
	/* logic init */
	if (nandphy_had_init == false)
		ret = NAND_PhyInit();
	/*NAND_PhyExit();*/

	NAND_Print_DBG("NAND_UbootProbe end: 0x%x\n", ret);

	return ret;

}

uint NAND_UbootRead_History(uint start, uint sectors, void *buffer)
{
	return NAND_LogicRead_History(start, sectors, buffer);
}

uint NAND_UbootRead(uint start, uint sectors, void *buffer)
{
	return NAND_LogicRead(start, sectors, buffer);
}

uint NAND_UbootWrite(uint start, uint sectors, void *buffer)
{
	return NAND_LogicWrite(start, sectors, buffer);
}

extern int nand_super_page_test(unsigned int chip, unsigned int block, unsigned int page);
int NAND_Uboot_Erase(int erase_flag)
{
	int version_match_flag;
	int nand_erased = 0;

	NAND_Print("erase_flag = %d\n", erase_flag);
	NAND_Print("%s %d\n", __func__, __LINE__);

	if (nandphy_had_init == false)
		NAND_PhyInit();

	if (erase_flag) {
		NAND_Print("erase by flag %d\n", erase_flag);
		NAND_EraseBootBlocks();
#ifndef FORCE_ERASE_ALL_INCLUDE_UBOOT_BLOCKS
		NAND_EraseChip();
		NAND_UpdatePhyArch();
#else
		NAND_EraseChip();
		NAND_EraseUbootBlocks();
#endif
		nand_erased = 1;
	} else {
		version_match_flag = NAND_VersionCheck();
		NAND_Print("nand version = %x\n", version_match_flag);
		NAND_EraseBootBlocks();

		if (get_storage_type() == 1) {
			if (rawnand_is_blank() == 1)
				NAND_EraseChip();
		}

		if (version_match_flag > 0) {
			NAND_Print("nand version fail,please select erase nand\n");
			nand_erased = -1;
		}
	}
	NAND_Print("NAND_Uboot_Erase\n");
	/*NAND_PhyExit();*/
	NAND_Print("%s %d nand_erased:%d\n", __func__, __LINE__, nand_erased);
	return nand_erased;
}

int NAND_Uboot_Force_Erase(void)
{
	NAND_Print("force erase\n");

	if (nandphy_had_init == false) {
		if (NAND_PhyInit()) {
			NAND_Print("phy init fail\n");
			return -1;
		}
	}

	NAND_EraseChip_force();

	/*NAND_PhyExit();*/

	return 0;
}

int NAND_ReadBoot0(uint length, void *buffer)
{
	return nand_read_nboot_data(buffer, length);
}

int NAND_BurnBoot0(uint length, void *buffer)
{
	return nand_write_nboot_data(buffer, length);
}



int NAND_BurnUboot(uint length, void *buffer)
{
	return nand_write_uboot_data(buffer, length);
}

int NAND_GetParam_store(void *buffer, uint length)
{

#if defined(CONFIG_MACH_SUN8IW7)
	boot_nand_para_t *t;
	if (!flash_scaned) {
		printf("sunxi flash: force flash init to begin hardware scanning\n");
		if (nandphy_had_init == false)
			NAND_PhyInit();
		nand_get_param((boot_nand_para_t *)nand_para_store);
		/*NAND_PhyExit();*/
		printf("sunxi flash: hardware scan finish\n");
	}
	memcpy(buffer, nand_para_store, length);

	t = (boot_nand_para_t *)buffer;

	printf("NAND_GetParam_store 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	       t->NandChipId[0], t->NandChipId[1], t->NandChipId[2],
	       t->NandChipId[3], t->NandChipId[4]);
#else
	if (!flash_scaned) {
		NAND_Print("force flash init to begin hardware scanning\n");
		if (nandphy_had_init == false)
			NAND_PhyInit();
		/*NAND_PhyExit();*/
		NAND_Print("hardware scan finish\n");
	}

	nand_get_param_for_uboottail((void *)nand_para_store);

	memcpy(buffer, nand_para_store, length);
#endif

	return 0;
}

int NAND_FlushCache(void)
{
#if defined(CONFIG_MACH_SUN8IW18) || defined(CONFIG_MACH_SUN8IW7) \
	|| (defined(CONFIG_SUNXI_RTOS) && (defined(CONFIG_MACH_SUN8IW20) || defined(CONFIG_MACH_SUN20IW1)))
	unsigned int pagesize = NAND_GetPageSize();
#else
	unsigned int pagesize = nand_get_chip_page_size(BYTE);
#endif



	/*nftl_write_end() is for some flash, which need share pages
	 * write together, and SLC nand don't need do that. we think
	 * vaguely that SLC nand pagesize would be small than 4k byte
	 * and SLC nand don't to do that to avoid write more dummy data,
	 * which would lead to slow in boot*/
	if (pagesize > SZ_4K) {
		if (boot_mode) {
			pr_info("write pair page\n");
			nftl_write_end();
		}
	}
	return nftl_flush_write_cache();
}

/* called by nand_replace_boot0_with_toc0 in nand lib, check toc0 before replacing */
int nand_verify_toc0(unsigned char *buffer, unsigned int len)
{
	toc0_private_head_t *toc0 = (toc0_private_head_t *)buffer;

	debug("%s\n", (char *)toc0->name);
	if (strncmp((const char *)toc0->name, TOC0_MAGIC, strlen(TOC0_MAGIC))) {
		printf("%s, not secure image\n", __func__);
		return -1;
	}
	if (toc0->length > len) {
		printf("%s, %d %d\n", __func__, toc0->length, len);
		return -1;
	}
	if (sunxi_sprite_verify_checksum(buffer, toc0->length,
					 toc0->check_sum)) {
		printf("%s: toc0 checksum is error\n", __func__);
		return -1;
	}
	return 0;
}

