/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <aw_common.h>
#include <nand_inc.h>
#include <nuttx/fs/fs.h>
#include <nand_phy.h>
#include <nand_log.h>

typedef int (*show_op)(void *buf, unsigned int len);
typedef int (*store_op)(const void *buf, unsigned int len);

struct nand_dbg_node {
#define NAND_DBG_NODE_NAME_LEN 32
	char name[NAND_DBG_NODE_NAME_LEN];
	char *buf;
	unsigned int bytes;
	show_op show;
	store_op store;
	struct nand_dbg_node *next;
};

struct nand_dbg {
	struct nand_dbg_node *head;
	struct _nftl_blk *blk;
	int removed;
} ndbg;

#define min_t(t, x, y) ((t)(x) > (t)(y) ? (y) : (x))
#define file_to_node(file) ((file)->f_inode->i_private)
static int nand_dbg_open(FAR struct file *filep)
{
	return 0;
}

static ssize_t nand_dbg_read(FAR struct file *f, FAR char *buf,
		size_t nbytes)
{
	struct nand_dbg_node *node = file_to_node(f);

	if (!node)
		return -EIO;
	if (ndbg.removed)
		return -EBUSY;
	if (!node->show)
		return -EBUSY;

	if (!node->buf) {
		node->buf = malloc(4096);
		if (!node->buf)
			return -ENOMEM;
		memset(node->buf, 0, 4096);
		node->bytes = node->show(node->buf, 4096);
		if (node->bytes < 0)
			goto free;
	}
	if (f->f_pos > node->bytes)
		goto free;

	nbytes = min_t(unsigned int, nbytes, node->bytes - f->f_pos);
	memcpy(buf, node->buf + f->f_pos, nbytes);
	f->f_pos += nbytes;
	return nbytes;
free:
	free(node->buf);
	return -EIO;
}

static ssize_t nand_dbg_write(FAR struct file *f, FAR const char *buf,
                   size_t nbytes)
{
	struct nand_dbg_node *node = file_to_node(f);

	if (!node)
		return -EIO;
	if (ndbg.removed)
		return -EBUSY;
	if (!node->store)
		return -EBUSY;

	return node->store(buf, nbytes);
}

static int nand_dbg_close(FAR struct file *f)
{
	struct nand_dbg_node *node = file_to_node(f);

	if (ndbg.removed)
		return -EBUSY;
	if (node->buf) {
		free(node->buf);
		node->buf = NULL;
		node->bytes = 0;
	}
	return 0;
}

static off_t nand_dbg_seek(FAR struct file *f, off_t off, int whence)
{
	struct nand_dbg_node *node = file_to_node(f);

	if (ndbg.removed)
		return -EBUSY;

	switch (whence) {
	case SEEK_CUR:
	    off = f->f_pos + off;
	    break;
	case SEEK_END:
	    off = node->bytes - 1 + off;
	    break;
	case SEEK_SET:
	    break;
	default: return -EINVAL;
	}

	if (off >= node->bytes || off < 0)
		return -EINVAL;

	f->f_pos = off;
	return 0;
}

static struct file_operations nand_dbg_fops = {
	.open = nand_dbg_open,
	.read = nand_dbg_read,
	.write = nand_dbg_write,
	.close = nand_dbg_close,
	.seek = nand_dbg_seek,
};

static int nand_dbg_register_os_node(struct nand_dbg_node *node)
{
	pr_info("register %s to nuttx\n", node->name);
	return register_driver(node->name, &nand_dbg_fops, 0444, (void *)node);
}

static int nand_dbg_unregister_os_node(struct nand_dbg_node *node)
{
	return unregister_driver(node->name);
}

static void nand_dbg_add_list(struct nand_dbg_node *node)
{
	if (!ndbg.head) {
		ndbg.head = node;
	} else {
		node->next = ndbg.head;
		ndbg.head = node;
	}
}

static int nand_dbg_add_node(const char *name, show_op show, store_op store)
{
	int ret;
	struct nand_dbg_node *node;

	if (name[0] == '\0')
		return -EINVAL;

	node = malloc(sizeof(*node));
	if (!node)
		return -ENOMEM;
	memset(node, 0, sizeof(*node));

	snprintf(node->name, NAND_DBG_NODE_NAME_LEN, "/dev/nand_debug/%s", name);
	node->show = show;
	node->store = store;
	node->buf = NULL;
	node->next = NULL;
	node->bytes = 0;

	ret = nand_dbg_register_os_node(node);
	if (ret)
		free(node);
	else
		nand_dbg_add_list(node);
	return ret;
}

static void nand_dbg_del_node(struct nand_dbg_node *node)
{
	nand_dbg_unregister_os_node(node);
	free(node);
}

static void nand_dbg_del_all_nodes(void)
{
	while (ndbg.head) {
		struct nand_dbg_node *next = ndbg.head->next;

		nand_dbg_del_node(ndbg.head);
		ndbg.head = next;
	}
}

static int nand_dbg_show_arch(void *buf, unsigned int len)
{
	return PHY_GetArchInfo_Str(buf);
}

static int nand_dbg_show_gcinfo(void *buf, unsigned int len)
{
	return ndbg.blk->gc_stat(ndbg.blk, buf, 4096);
}

static int nand_dbg_show_badblock(void *buf, unsigned int len)
{
	return sprintf(buf, "cnt: %d\n", ndbg.blk->badblk(ndbg.blk));
}

static int nand_dbg_show_version(void *buf, unsigned int len)
{
	return snprintf(buf, len, "nftl: %u.%u.%u\n",
			ndbg.blk->ver_main,
			ndbg.blk->ver_mid,
			ndbg.blk->ver_sub);
}

int nand_dbg_init(struct _nftl_blk *blk)
{
	ndbg.blk = blk;
	ndbg.removed = 0;
	ndbg.head = NULL;

#define add_node(name, show, store) {				\
	if (nand_dbg_add_node(name, show, store)) {			\
		pr_err("add %s for nand debug failed\n", name); \
		goto err;					\
	}							\
}
	add_node("arch", nand_dbg_show_arch, NULL);
	add_node("gcinfo", nand_dbg_show_gcinfo, NULL);
	add_node("badblock", nand_dbg_show_badblock, NULL);
	add_node("version", nand_dbg_show_version, NULL);
#undef add_node
	pr_info("nand debug init OK\n");
	return 0;
err:
	return -EIO;
}

void nand_dbg_exit(void)
{
	nand_dbg_del_all_nodes();
	ndbg.blk = NULL;
	ndbg.removed = 1;
}
