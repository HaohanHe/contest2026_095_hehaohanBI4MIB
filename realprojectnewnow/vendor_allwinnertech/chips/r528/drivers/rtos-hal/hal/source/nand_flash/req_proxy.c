#include <stdio.h>
#include "req_proxy.h"
#include "hal_time.h"
#include "hal_queue.h"
#include "hal_thread.h"
#include "hal_mem.h"
#include <semaphore.h>
#include <debug.h>

#include <nuttx/sched.h>
#include <nuttx/spinlock.h>
#include <nuttx/list.h>

/* silence when > LOG_DEBUG */
#define PROXY_DEFAULT_ERR_LOG_LEVEL		(LOG_ERR)
#define PROXY_DEFAULT_WARN_LOG_LEVEL		(LOG_WARNING)
#define PROXY_DEFAULT_INFO_LOG_LEVEL		(LOG_INFO)
#define PROXY_DEFAULT_DBG_LOG_LEVEL		(LOG_DEBUG + 1)

#define PROXY_REQ_TRACE
#define PROXY_REQ_CHECK
#define PROXY_VAILD_CHECK

#define PROXY_STATE_RUNNING	(0)
#define PROXY_STATE_STOPED	(1)

struct req_proxy_t {
#ifdef PROXY_VAILD_CHECK
	struct list_node list;
	unsigned long refcnt;
#endif
	void *data;
	hal_queue_t queue;
	sem_t thread_sem;
	void *thread;
	req_proxy_handler_t handler;
	int state;
	void *priv;
	size_t data_size;
	size_t queue_depth;
};

struct req_proxy_request_t {
	void *data;
	sem_t *sem;
#ifdef PROXY_REQ_TRACE
	pid_t pid;
#endif
#ifdef PROXY_REQ_CHECK
	unsigned long check;
#endif
};

/* ------------------------------ log wrapper ------------------------------ */
#undef pr_debug
#undef pr_info
#undef pr_warn
#undef pr_err

static __attribute__((used)) unsigned long proxy_err_log_level  = PROXY_DEFAULT_ERR_LOG_LEVEL;
static __attribute__((used)) unsigned long proxy_warn_log_level = PROXY_DEFAULT_WARN_LOG_LEVEL;
static __attribute__((used)) unsigned long proxy_info_log_level = PROXY_DEFAULT_INFO_LOG_LEVEL;
static __attribute__((used)) unsigned long proxy_dbg_log_level  = PROXY_DEFAULT_DBG_LOG_LEVEL;

#define _CONTACT(__STR_X__, __STR_Y__)	__STR_X__##__STR_Y__
#define CONTACT(__STR_X__, __STR_Y__)	_CONTACT(__STR_X__, __STR_Y__)

#define proxy_log_level(_level)		CONTACT(CONTACT(proxy_, _level), _log_level)

#define proxy_log(_level, fmt, ...)	\
	do { \
		unsigned long level = proxy_log_level(_level); \
		if (level > LOG_DEBUG) \
			break; \
		syslog(level, fmt, ##__VA_ARGS__); \
	} while(0)

#define pr_err(fmt, ...)	do { proxy_log(err, fmt, ##__VA_ARGS__); } while(0)
#define pr_warn(fmt, ...)	do { proxy_log(warn, fmt, ##__VA_ARGS__); } while(0)
#define pr_info(fmt, ...)	do { proxy_log(info, fmt, ##__VA_ARGS__); } while(0)
#define pr_dbg(fmt, ...) 	do { proxy_log(dbg, fmt, ##__VA_ARGS__); } while(0)
#define pr_debug(fmt, ...) 	do { proxy_log(dbg, "[%s:%lu]" fmt, __func__, (unsigned long)__LINE__, ##__VA_ARGS__); } while(0)
/* ------------------------------ log wrapper ------------------------------ */

#ifdef PROXY_VAILD_CHECK
static spinlock_t g_list_lock;
static struct list_node g_proxy_list = LIST_INITIAL_VALUE(g_proxy_list);

static inline void req_proxy_add(struct req_proxy_t *hdl)
{
	unsigned long flags;

	flags = spin_lock_irqsave(&g_list_lock);
	hdl->refcnt = 1;
	list_add_tail(&g_proxy_list, &hdl->list);
	spin_unlock_irqrestore(&g_list_lock, flags);
}

static inline int req_proxy_del(struct req_proxy_t *hdl)
{
	struct req_proxy_t *cur, *tmp;
	unsigned long flags, refcnt;
	int ret = -1;

	flags = spin_lock_irqsave(&g_list_lock);
	list_for_every_entry_safe(&g_proxy_list, cur, tmp, struct req_proxy_t, list) {
		if (cur == hdl) {
			list_delete(&hdl->list);
			hdl->refcnt--;
			if (hdl->refcnt > 0)
				goto wait_release;
			ret = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&g_list_lock, flags);
	return ret;
wait_release:
	while (1) {
		hal_msleep(100);
		flags = spin_lock_irqsave(&g_list_lock);
		refcnt = hdl->refcnt;
		spin_unlock_irqrestore(&g_list_lock, flags);
		if (refcnt == 0)
			break;
	}
	return 0;
}

static inline int req_proxy_get(struct req_proxy_t *hdl)
{
	struct req_proxy_t *cur, *tmp;
	unsigned long flags;
	int ret = -1;

	flags = spin_lock_irqsave(&g_list_lock);
	list_for_every_entry_safe(&g_proxy_list, cur, tmp, struct req_proxy_t, list) {
		if (cur == hdl) {
			hdl->refcnt++;
			ret = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&g_list_lock, flags);

	return ret;
}

static inline void req_proxy_put(struct req_proxy_t *hdl)
{
	unsigned long flags;

	flags = spin_lock_irqsave(&g_list_lock);
	hdl->refcnt--;
	spin_unlock_irqrestore(&g_list_lock, flags);
}
#else
static inline void req_proxy_add(struct req_proxy_t *hdl) { }
static inline int req_proxy_del(struct req_proxy_t *hdl) { return 0; }
static inline int req_proxy_get(struct req_proxy_t *hdl) { return 0; }
static inline void req_proxy_put(struct req_proxy_t *hdl) { }
#endif

#ifdef PROXY_REQ_CHECK
static inline int check_sum(void *_ptr, unsigned long len)
{
	unsigned long sum = 0;
	unsigned long *ptr = _ptr;

	while (len) {
		sum = *ptr;
		len -= sizeof(*ptr);
	}

	return sum;
}

static inline int check_req(struct req_proxy_request_t *req)
{
	unsigned long check = check_sum(req, offsetof(struct req_proxy_request_t, check));

	return check != req->check ? -1 : 0;
}

static inline void get_req_check_sum(struct req_proxy_request_t *req)
{
	req->check = check_sum(req, offsetof(struct req_proxy_request_t, check));
}
#endif

static void req_proxy_thread(void *arg)
{
	struct req_proxy_t *hdl = (struct req_proxy_t *)arg;
	hal_queue_t queue = hdl->queue;
	struct req_proxy_request_t req;
	req_proxy_handler_t handler = hdl->handler;
	void *priv = hdl->priv;

	while (hdl->state == PROXY_STATE_RUNNING) {
		int ret;
		if ((ret = hal_queue_recv(queue, &req, HAL_WAIT_FOREVER))) {
			//printf("%s: hal_queue_recv return %d\n", __func__, ret);
			continue;
		}
#ifdef PROXY_REQ_CHECK
		if (check_req(&req)) {
			pr_err("%s: req packet damage! req thread will block!\n", __func__);
#ifdef PROXY_REQ_TRACE
			pr_err("%s: req thread pid: %lu\n", __func__, (unsigned long)req.pid);
#endif
			continue;
		}
#endif

		if (!req.data)
			break;
		if ((ret = handler(priv, req.data))) {
#ifdef PROXY_REQ_TRACE
			struct tcb_s *tcb = nxsched_get_tcb(req.pid);
			const char *name = "(unknown)";

			if (tcb)
				name = tcb->name;
			pr_err("%s: handler return %d, req from pid %lu(%s)\n", __func__, ret, (unsigned long)req.pid, name);
#else
			pr_err("%s: handler return %d\n", __func__, ret);
#endif
			hdl->state = PROXY_STATE_STOPED;
		}
		nxsem_post(req.sem);
	}
	hdl->state = PROXY_STATE_STOPED;

	nxsem_post(&hdl->thread_sem);
	hal_thread_stop(hdl->thread);
}

int req_proxy_request(void *_hdl, void *data)
{
	struct req_proxy_t *hdl = (struct req_proxy_t *)_hdl;
	sem_t sem;

	if (req_proxy_get(hdl))
		return -ENODEV;

	nxsem_init(&sem, 0, 0);
	struct req_proxy_request_t req = {
		.data = data,
		.sem = &sem,
#ifdef PROXY_REQ_TRACE
		.pid = getpid(),
#endif
	};
	int ret = -1;

	if (hdl->state != PROXY_STATE_RUNNING)
		goto exit;

#ifdef PROXY_REQ_CHECK
	get_req_check_sum(&req);
#endif

	while (hal_queue_send(hdl->queue, &req)) {
		if (hdl->state != PROXY_STATE_RUNNING)
			goto exit;
		hal_msleep(1000);
	}

	ret = nxsem_wait_uninterruptible(&sem);

exit:
	nxsem_destroy(&sem);
	req_proxy_put(hdl);
	return ret;
}

static inline void req_proxy_exit(void *_hdl)
{
	struct req_proxy_t *hdl = (struct req_proxy_t *)_hdl;
	struct req_proxy_request_t req = {
		.data = NULL,
		.sem = NULL,
#ifdef PROXY_REQ_TRACE
		.pid = getpid(),
#endif
	};

	if (hdl->state != PROXY_STATE_RUNNING)
		goto exit;

#ifdef PROXY_REQ_CHECK
	get_req_check_sum(&req);
#endif

	hdl->state = PROXY_STATE_STOPED;
	while (hal_queue_send(hdl->queue, &req)) {
		if (hdl->state != PROXY_STATE_RUNNING)
			goto exit;
		hal_msleep(1000);
	}

exit:
	nxsem_wait_uninterruptible(&hdl->thread_sem);
}

void req_proxy_destroy(void *_hdl)
{
	struct req_proxy_t *hdl = (struct req_proxy_t *)_hdl;

	if (hdl) {
		if (req_proxy_del(hdl))
			pr_err("req_proxy_del error!\n");
		if (hdl->queue && hdl->thread) {
			req_proxy_exit(hdl);
		}
		if (hdl->data) {
			hal_free(hdl->data);
			hdl->data = NULL;
		}
		if (hdl->queue) {
			//hal_is_queue_empty(hdl->queue);
			hal_queue_delete(hdl->queue);
			hdl->queue = NULL;
		}

		nxsem_destroy(&hdl->thread_sem);
		hal_free(hdl);
	}
}

void *req_proxy_create(const char *name, req_proxy_handler_t handler, void *priv, size_t data_size, size_t queue_depth)
{
	struct req_proxy_t *hdl = hal_malloc(sizeof(*hdl));

	if (!hdl) {
		pr_err("no memory!\n");
		goto err;
	}
	memset(hdl, 0, sizeof(*hdl));

	nxsem_init(&hdl->thread_sem, 0, 0);
	hdl->state = PROXY_STATE_RUNNING;
	hdl->priv = priv;
	hdl->data_size = data_size;
	hdl->queue_depth = queue_depth;
	hdl->handler = handler;

	req_proxy_add(hdl);

	hdl->queue = hal_queue_create(name, sizeof(struct req_proxy_request_t), queue_depth);
	if (!hdl->queue) {
		pr_err("no memory!\n");
		goto err;
	}

	hdl->data = hal_malloc(hdl->data_size);
	if (!hdl->data) {
		pr_err("no memory!\n");
		goto err;
	}

	hdl->thread = hal_thread_create(req_proxy_thread, hdl, name, 8192, 120);
	if (!hdl->thread) {
		pr_err("no memory!\n");
		goto err;
	}
#ifdef CONFIG_SMP
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	nxsched_set_affinity((pid_t)hdl->thread, sizeof(cpuset), &cpuset);
#endif

	hal_thread_start(hdl->thread);

	return hdl;
err:
	req_proxy_destroy(hdl);
	return NULL;
}
