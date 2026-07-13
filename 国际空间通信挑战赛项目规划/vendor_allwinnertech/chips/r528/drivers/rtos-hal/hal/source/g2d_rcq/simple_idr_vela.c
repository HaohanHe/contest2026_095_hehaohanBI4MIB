/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


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

#include "simple_idr.h"

#ifdef USE_VELA_IDR
#include <nuttx/lib/lib.h>
#include <nuttx/idr.h>
#include <syslog.h>

/* silence when > LOG_DEBUG */
#define ID_DEFAULT_ERR_LOG_LEVEL		(LOG_ERR)
#define ID_DEFAULT_WARN_LOG_LEVEL		(LOG_WARNING)
#define ID_DEFAULT_INFO_LOG_LEVEL		(LOG_INFO)
#define ID_DEFAULT_DBG_LOG_LEVEL		(LOG_DEBUG + 1)

/* ------------------------------ log wrapper ------------------------------ */
#undef pr_debug
#undef pr_info
#undef pr_warn
#undef pr_err

static __attribute__((used)) unsigned long id_err_log_level  = ID_DEFAULT_ERR_LOG_LEVEL;
static __attribute__((used)) unsigned long id_warn_log_level = ID_DEFAULT_WARN_LOG_LEVEL;
static __attribute__((used)) unsigned long id_info_log_level = ID_DEFAULT_INFO_LOG_LEVEL;
static __attribute__((used)) unsigned long id_dbg_log_level  = ID_DEFAULT_DBG_LOG_LEVEL;

#define _CONTACT(__STR_X__, __STR_Y__)	__STR_X__##__STR_Y__
#define CONTACT(__STR_X__, __STR_Y__)	_CONTACT(__STR_X__, __STR_Y__)

#define id_log_level(_level)		CONTACT(CONTACT(id_, _level), _log_level)

#define id_log(_level, fmt, ...)	\
	do { \
		unsigned long level = id_log_level(_level); \
		if (level > LOG_DEBUG) \
			break; \
		syslog(level, fmt, ##__VA_ARGS__); \
	} while(0)

#define pr_err(fmt, ...)	do { id_log(err, fmt, ##__VA_ARGS__); } while(0)
#define pr_warn(fmt, ...)	do { id_log(warn, fmt, ##__VA_ARGS__); } while(0)
#define pr_info(fmt, ...)	do { id_log(info, fmt, ##__VA_ARGS__); } while(0)
#define pr_dbg(fmt, ...) 	
// #define pr_dbg(fmt, ...) 	do { id_log(dbg, fmt, ##__VA_ARGS__); } while(0)
#define pr_debug(fmt, ...) 	do { id_log(dbg, "[%s:%lu]" fmt, __func__, (unsigned long)__LINE__, ##__VA_ARGS__); } while(0)
/* ------------------------------ log wrapper ------------------------------ */

#define START_ID	(NO_ID + 1)
#define END_ID		(0xfffffff)

/* not used, compatible with previous code */
struct id_dir {
	int res;
};

int id_alloc(struct id_dir *dir, void *value)
{
	FAR struct idr_s *idr = (FAR struct idr_s *)dir;
	int id;

	if (!idr) {
		pr_err("%s invaild para: idr: %p\n", __func__, idr);
		return NO_ID;
	}

	id = idr_alloc(idr, value, START_ID, END_ID);
	if (id < 0) {
		pr_err("%s failed! ret: %d\n", __func__, id);
		return NO_ID;
	}

	pr_dbg("%s(%p, %p) return %d\n", __func__, idr, value, id);
	return id;
}

void id_free(struct id_dir *dir, int id)
{
	FAR struct idr_s *idr = (FAR struct idr_s *)dir;

	if (!idr || id < START_ID || id > END_ID) {
		pr_err("%s invaild para: idr: %p, id: %d\n", __func__, idr, id);
		return;
	}

	pr_dbg("%s(%p, %d)\n", __func__, idr, id);
	idr_remove(idr, id);
}

void *id_get(struct id_dir *dir, int id)
{
	FAR struct idr_s *idr = (FAR struct idr_s *)dir;
	void *ret;

	if (!idr || id < START_ID || id > END_ID) {
		pr_err("%s invaild para: idr: %p, id: %d\n", __func__, idr, id);
		return NULL;
	}

	ret = idr_get_next(idr, &id);
	pr_dbg("%s(%p, %d) return %p\n", __func__, idr, id, ret);
	return ret;
}

struct id_dir *id_creat(void)
{
	// id_alloc will return NO_ID when failed
	FAR struct idr_s *idr = idr_init_base(START_ID);

	if (!idr) {
		pr_err("%s failed\n", __func__);
		return NULL;
	}

	pr_dbg("%s() return %p\n", __func__, idr);
	return (struct id_dir *)idr;
}

void id_destroyed(struct id_dir *dir)
{
	FAR struct idr_s *idr = (FAR struct idr_s *)dir;

	if (!idr) {
		pr_err("%s invaild para: idr: %p\n", __func__, idr);
		return;
	}

	pr_dbg("%s(%p)\n", __func__, idr);
	idr_destroy(idr);
}

#endif