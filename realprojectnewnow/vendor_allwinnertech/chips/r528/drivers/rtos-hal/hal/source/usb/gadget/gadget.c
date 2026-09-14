/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
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
#include <nuttx/usb/usbdev.h>
#include <aw_list.h>
#include <debug.h>
#include "gadget.h"
#include "udc.h"

typedef void * usb_handle_t;
static LIST_HEAD(g_func_list);
static usb_handle_t g_usb_handle = NULL;
static hal_spinlock_t gadget_lock;

#define GADGET_READ     (0)
#define GADGET_WRITE    (1)

static struct usb_function_driver *usb_gadget_function_find(const char *name)
{
	struct usb_function_driver *fd = NULL;

	if (!name)
		return NULL;
	list_for_each_entry(fd, &g_func_list, list) {
		if (!strcmp(fd->name, name))
			return fd;
	}
	return NULL;
}

int usb_gadget_function_register(struct usb_function_driver *newf)
{
	struct usb_function_driver *fd = NULL;

	if (!newf)
		return -1;
	fd = usb_gadget_function_find(newf->name);
	if (fd != NULL) {
		_info("%s has been registed\n", newf->name);
		return -1;
	}
	list_add_tail(&newf->list, &g_func_list);
	return 0;
}

int usb_gadget_function_unregister(struct usb_function_driver *fd)
{
	list_del(&fd->list);
	return 0;
}

static void usb_gadget_notifier(struct usb_function_driver *fd, int mode)
{
	int ret = 0;
	hal_sem_t schd = NULL;
	hal_sem_t finish_schd = NULL;

	gadget_debug("");
	if (mode == GADGET_READ) {
		schd = fd->read_schd;
		finish_schd = fd->read_finish_schd;
	} else if (mode == GADGET_WRITE) {
		schd = fd->write_schd;
		finish_schd = fd->write_finish_schd;
	} else {
		gadget_err("unknown mode");
	}

	if (!schd) {
		return;
	}
	ret |= hal_sem_post(schd);
	ret |= hal_sem_post(finish_schd);
	if (ret == 0) {
		return;
	} else {
		gadget_err("gadget notifier failed, mode=%d\n", mode);
	}
}

/*#define GADGET_WRITE_COMPLETE_DEBUG*/
#ifdef GADGET_WRITE_COMPLETE_DEBUG
int write_complete = 1;
#endif
static udc_errno_t usb_gadget_callback(uint8_t ep_addr,
				       udc_callback_event_t event,
				       void *data,
				       uint32_t len)
{
	uint8_t ep_idx;
	uint8_t is_in;
	udc_errno_t ret = UDC_ERRNO_SUCCESS;
	struct usb_ctrlrequest *crq;
	struct usb_function_driver *fd = (struct usb_function_driver *)g_usb_handle;

	gadget_debug("ep:0x%02x, event:%d, len:%d\n", ep_addr, event, len);

	ep_idx = ep_addr & 0x7f;
	is_in = ep_addr & USB_DIR_IN;

	if (ep_idx == 0) { /* handle ep0 */
		crq = (struct usb_ctrlrequest *)data;
		switch (event) {
		case UDC_EVENT_RX_STANDARD_REQUEST:
			/*ret = usb_adb_standard_request_handler(crq);*/
			if (fd->standard_req) {
				fd->standard_req(crq);
			}
			break;
		case UDC_EVENT_RX_CLASS_REQUEST:
			/*ret = usb_msg_class_request_handler(crq);*/
			if (fd->class_req)
				fd->class_req(crq);
			break;
		case UDC_EVENT_RX_DATA:
			if (fd->ep0_rx_complete)
				fd->ep0_rx_complete(crq, data, len);
			break;
		default:
			ret = UDC_ERRNO_CMD_NOT_SUPPORTED;
			break;
		}
	} else { /* handle ep1~4 */
		if (is_in) {
			/* TODO: maybe useless? */
		} else {
			gadget_debug("event:0x%x\n", event);
			switch (event) {
			case UDC_EVENT_RX_DATA:
				fd->read_size = len;
				usb_gadget_notifier(fd, GADGET_READ);
				break;
			case UDC_EVENT_TX_COMPLETE:
				gadget_debug("tx complete..\n");
				// write_complete = 1;
				usb_gadget_notifier(fd, GADGET_WRITE);
				break;
			default:
				ret = UDC_ERRNO_CMD_NOT_SUPPORTED;
				break;
			}
		}
	}

	return ret;
}

static int sunxi_udc_configure(struct usbdev_ep_s *ep,
		const struct usb_epdesc_s *desc,
		bool last)
{
	int addr = 0;
	int epno = desc->addr & 0x7f;
	int is_in = (desc->addr & USB_DIR_MASK) == USB_REQ_DIR_IN;
	int ep_type = (desc->attr & USB_EP_ATTR_XFERTYPE_MASK);
#ifdef CONFIG_USBDEV_DUALSPEED
	int maxpacket = 512;

	if (epno == 0)
		maxpacket = 64;
#else
	int maxpacket = 64;
#endif

	if (epno > 10) {
		_err("sunxi udc's ep out of ep range\n");
		return -EINVAL;
	}

	if (is_in)
		addr = epno | USB_DIR_IN;
	else
		addr = epno | USB_DIR_OUT;

	ep->eplog = addr;
	ep->maxpacket = maxpacket;

	udc_ep_enable(addr, maxpacket, ep_type & USB_ENDPOINT_XFERTYPE_MASK);

	return 0;
}

static int sunxi_udc_disable(struct usbdev_ep_s *ep)
{
	udc_ep_disable(ep->eplog);
	return 0;
}

static struct usbdev_req_s * sunxi_udc_allocreq(struct usbdev_ep_s *ep)
{
	struct sunxi_req_s *privreq;

	privreq = (struct sunxi_req_s *)malloc(sizeof (struct sunxi_req_s));
	if (privreq == NULL)
		return NULL;
	INIT_LIST_HEAD(&privreq->queue);

	return &privreq->req;
}

static void sunxi_udc_freereq(struct usbdev_ep_s *ep, struct usbdev_req_s *req)
{
	free(req);
}

#ifdef CONFIG_USBDEV_DMA
static void * sunxi_ep_allocbuffer(struct usbdev_ep_s *ep, uint16_t nbytes)
{
	return malloc(nbytes);
}

static void sunxi_ep_freebuffer(struct usbdev_ep_s *ep, void *buf)
{
	free(buf);
}
#endif

void sunxi_udc_done(struct sunxi_ep_s *ep,
		struct sunxi_req_s *req, int status)
{
	list_del_init(&req->queue);
	req->req.result = status;

	req->req.callback(&ep->ep, &req->req);
}

static int sunxi_udc_cancel(struct usbdev_ep_s *ep, struct usbdev_req_s *req)
{
	struct sunxi_req_s *privreq = (struct sunxi_req_s *)req;
	struct sunxi_ep_s *privep = (struct sunxi_ep_s *)ep;
	int ret = -EINVAL;

	list_for_each_entry(privreq, &privep->queue, queue) {
		if (&privreq->req == req) {
			list_del_init(&privreq->queue);
			req->result = -EAGAIN;
			ret = 0;
			break;
		}
	}
	if (ret == 0)
		sunxi_udc_done(privep, privreq, -ECONNRESET);

	return ret;
}
extern int sunxi_udc_submit(FAR struct usbdev_ep_s *ep, FAR struct usbdev_req_s *req);
extern struct usbdev_ops_s sunxi_udc_ops;
extern struct usbdev_s sunxi_usbdev;
static const struct usbdev_epops_s sunxi_udc_epops = {
	.configure = sunxi_udc_configure,
	.disable = sunxi_udc_disable,
	.allocreq= sunxi_udc_allocreq,
	.freereq= sunxi_udc_freereq,
#ifdef CONFIG_USBDEV_DMA
	.allocbuffer = sunxi_ep_allocbuffer,
	.freebuffer = sunxi_ep_freebuffer,
#endif
	.submit= sunxi_udc_submit,
	.cancel= sunxi_udc_cancel,
};

struct sunxi_usbdev_s g_usbdev;
struct sunxi_usbdev_s* get_usbdev(void)
{
	return &g_usbdev;
}

extern int is_controller_alive;
static struct usbdev_ep_s * sunxi_udc_allocep(FAR struct usbdev_s *dev,
		uint8_t epphy, bool in, uint8_t eptype)
{
	struct sunxi_usbdev_s *priv = (struct sunxi_usbdev_s *)dev;
	struct sunxi_ep_s *privep;
	uint8_t addr;

	epphy &= 0x7f;

	if (epphy > 0) {
		if (epphy > 10) {
			_err("sunxi udc's ep out of ep range\n");
			return NULL;
		}
		if (in)
			addr = epphy | USB_DIR_IN;
		else
			addr = epphy | USB_DIR_OUT;

		privep = &priv->eplist[epphy];
		privep->ep.eplog = addr;
		return &privep->ep;
	}

	return NULL;
}

static void sunxi_udc_freeep(FAR struct usbdev_s *dev,
		FAR struct usbdev_ep_s *ep)
{
	ep->eplog = 0;
}

static int sunxi_udc_get_frame(FAR struct usbdev_s *dev)
{
	if (!is_controller_alive) {
		hal_log_info("%s_%d: usb device is not active\n",
				__func__, __LINE__);
		return 0;
	}
	hal_log_info("sunxi_udc_get_frame is no susport\n");

	return 0;
}

static int sunxi_udc_wakeup(FAR struct usbdev_s *dev)
{
	if (!is_controller_alive) {
		hal_log_info("%s_%d: usb device is not active\n",
				__func__, __LINE__);
		return 0;
	}

	return 0;
}

static int sunxi_udc_set_selfpowered(FAR struct usbdev_s *dev, bool selfpowered)
{
	if (!is_controller_alive) {
		hal_log_info("%s_%d: usb device is not active\n",
				__func__, __LINE__);
		return 0;
	}

	return 0;
}

static int sunxi_udc_pullup(FAR struct usbdev_s *dev, bool enable)
{
	usbc_enable_dpdm_pullup(enable);
	usbc_enable_id_pullup(enable);

	return 0;
}

struct usbdev_ops_s sunxi_udc_ops = {
	.allocep = sunxi_udc_allocep,
	.freeep = sunxi_udc_freeep,
	.getframe = sunxi_udc_get_frame,
	.wakeup = sunxi_udc_wakeup,
	.selfpowered = sunxi_udc_set_selfpowered,
	.pullup = sunxi_udc_pullup,
};
static void sunxi_sw_setup(struct sunxi_usbdev_s *priv)
{
	int epno = 0;

	memset(priv, 0, sizeof(struct sunxi_usbdev_s));
	priv->usbdev.ops = &sunxi_udc_ops;
	priv->usbdev.ep0 = &priv->eplist[0].ep;

	for (epno = 0; epno < 11; epno++) {
		priv->eplist[epno].ep.ops = &sunxi_udc_epops;
		priv->eplist[epno].ep.eplog  = epno;
		priv->eplist[epno].dev = priv;
#ifdef CONFIG_USBDEV_DUALSPEED
		if (epno == 0)
			priv->eplist[epno].ep.maxpacket = 64;
		else
			priv->eplist[epno].ep.maxpacket = 512;
#else
			priv->eplist[epno].ep.maxpacket = 64;
#endif
		INIT_LIST_HEAD(&priv->eplist[epno].queue);
	}
}

/* static int sunxi_adb_init(void); */
static int sunxi_adb_deinit(void);

void arm_usbinitialize(void)
{
	struct sunxi_usbdev_s *priv = &g_usbdev;

	sunxi_sw_setup(priv);

	return;
}

void arm_usbuninitialize(void)
{
	_info("arm_usbuninitialize()\n");
}

int usbdev_register(struct usbdevclass_driver_s *driver)
{
	struct sunxi_usbdev_s *priv = &g_usbdev;
	int ret = 0;

	if (!driver || !driver->ops->bind || !driver->ops->unbind ||
			!driver->ops->disconnect || !driver->ops->setup) {
		_err("usbdev driver has no func, return failed\n");
		return -1;
	}
	if (!priv->usbdev.ep0->ops->allocreq) {
		_err("!!no ep0 allocreq\n");
		return -1;
	}

	if (priv->driver) {
		_warn("usbdev has been registed\r\n");
		return -EBUSY;
	}
	priv->driver = driver;
	//sunxi_adb_init();
	hal_udc_init();

	ret = CLASS_BIND(driver, &priv->usbdev);
	if (!ret) {
#ifdef CONFIG_USBDEV_DUALSPEED
		priv->usbdev.speed = USB_SPEED_HIGH;
		priv->usbdev.dualspeed = 1;
#else
		priv->usbdev.speed = USB_SPEED_FULL;
		priv->usbdev.dualspeed = 0;
#endif
	} else {
		_warn("class bind failed\n");
	}

	return ret;
}

int usbdev_unregister(struct usbdevclass_driver_s *driver)
{
	struct sunxi_usbdev_s *priv = &g_usbdev;

	if (driver != priv->driver) {
		_err("usbdev driver is not the same! " \
			"drvier: %p" \
			"prin->driver: %p" \
			"unregister failed\n", \
			driver, priv->driver);
		return -EINVAL;
	}

	CLASS_UNBIND(driver, &priv->usbdev);

	sunxi_adb_deinit();

	priv->driver = NULL;

	return 0;
}

static int usb_gadget_init(struct usb_function_driver *fd)
{
	/* if (g_usb_handle != NULL || !fd->desc_init) */
	if (g_usb_handle != NULL) {
		_err("usb gadget is not NULL, registe failed, 0x%lx\n", (uint32_t)g_usb_handle);
		return -1;
	}
	// fd->read_schd = xSemaphoreCreateBinary();
	// fd->write_schd = xSemaphoreCreateBinary();
	fd->read_schd = hal_sem_create(1);
	fd->write_schd = hal_sem_create(1);
	fd->read_finish_schd = hal_sem_create(0);
	fd->write_finish_schd = hal_sem_create(0);
	memset(fd->strings, 0, USB_GADGET_MAX_IDX * sizeof(uint16_t *));

	if (fd->desc_init) {
		(*fd->desc_init)(fd);
	}

	fd->ep_addr = calloc(3, sizeof(uint8_t));
	if (!fd->ep_addr) {
		gadget_err("no memory.\n");
		return -1;
	}
	fd->ep_addr[0] = 0;
	fd->ep_addr[1] = 0x1;
	fd->ep_addr[2] = 0x82;


	hal_udc_init();
	hal_udc_register_callback(usb_gadget_callback);

	g_usb_handle = (usb_handle_t)fd;
	fd->enabled = 1;
	memset(fd->strings, 0, USB_GADGET_MAX_IDX * sizeof(uint16_t *));  // from rtos

	return 0;
}

static int usb_gadget_deinit(struct usb_function_driver *fd)
{
	fd->enabled = 0;

	// memset(fd->strings, 0, USB_GADGET_MAX_IDX * sizeof(uint16_t *));
	g_usb_handle = NULL;
	hal_udc_register_callback(NULL);
	hal_udc_deinit();
	if (fd->desc_deinit) {
		(*fd->desc_deinit)(fd);
	}
	hal_sem_delete(fd->read_schd);
	hal_sem_delete(fd->write_schd);
	hal_sem_delete(fd->read_finish_schd);
	hal_sem_delete(fd->write_finish_schd);
	return 0;
}

int usb_gadget_function_enable(const char *name)
{
	struct usb_function_driver *fd = NULL;

	if (!name)
		return -1;
	fd = usb_gadget_function_find(name);
	if (!fd) {
		gadget_err("usb gadget, can't find %s function\n", name);
		return -1;
	}
	return usb_gadget_init(fd);
}

int usb_gadget_function_disable(const char *name)
{
	struct usb_function_driver *fd = NULL;

	if (!name)
		return -1;
	fd = usb_gadget_function_find(name);
	if (!fd) {
		gadget_err("usb gadget, can't find %s function\n", name);
		return -1;
	}
	usb_gadget_deinit(fd);
	/* unsupport now */
	return 0;
}

int usb_gadget_function_read(int ep_idx, char *buf, int size)
{
	struct usb_function_driver *fd = (struct usb_function_driver *)g_usb_handle;
	/* int count; */
	uint32_t flags;

	gadget_debug("ep_idx:%d\n", ep_idx);
	hal_sem_wait(fd->read_schd);
	flags = hal_spin_lock_irqsave(&gadget_lock);
	hal_udc_ep_read(fd->ep_addr[ep_idx], buf, size);
	hal_spin_unlock_irqrestore(&gadget_lock, flags);
	// if (count >= 0) {
	// 	gadget_debug("read data:%d", count);
	// 	return count;
	// }
	gadget_debug("wait ep%u[%x], recv data finish\n", ep_idx, fd->ep_addr[ep_idx]);
	hal_sem_wait(fd->read_finish_schd);
	gadget_debug("receive %u bytes\n", fd->read_size);
	return fd->read_size;
}

int usb_gadget_function_read_timeout(int ep_idx, char *buf, int size, int ms)
{
	struct usb_function_driver *fd = (struct usb_function_driver *)g_usb_handle;
	/* int count; */

	hal_sem_timedwait(fd->read_schd, MS_TO_OSTICK(ms));
	hal_udc_ep_read(fd->ep_addr[ep_idx], buf, size);
	/* gadget_debug("count=%d", count); */
	hal_sem_timedwait(fd->read_finish_schd, MS_TO_OSTICK(ms));
	return fd->read_size;
}

/* BLOCK mode */
int usb_gadget_function_write(int ep_idx, char *buf, int size)
{
	struct usb_function_driver *fd = (struct usb_function_driver *)g_usb_handle;
	/* int count = 0; */

	gadget_debug("write ep%u[0x%x], size=%u", ep_idx, fd->ep_addr[ep_idx], size);
	hal_sem_wait(fd->write_schd);
	gadget_debug("write data, len:%u", size);
	hal_udc_ep_write(fd->ep_addr[ep_idx], (void *)buf, size);
	// if (count < 0) {
	// 	gadget_err("hal_udc_ep_write failed, return %d", count);
	// 	return -1;
	// }
	gadget_debug("wait ep%u [%x], send data finish\n", ep_idx, fd->ep_addr[ep_idx]);
	hal_sem_wait(fd->write_finish_schd);
	/* gadget_debug("count=%d, size=%d", count, size); */
	gadget_debug("write %u bytes complete", size);
	return size;
}

int usb_gadget_function_exit(void)
{
	struct usb_function_driver *fd = (struct usb_function_driver *)g_usb_handle;
	int val = 0;

	if (fd->write_schd) {
		hal_sem_getvalue(fd->write_schd, &val);
		if (val == 0) {
			hal_sem_post(fd->write_schd);
		}
	}
	if (fd->write_finish_schd) {
		hal_sem_getvalue(fd->write_finish_schd, &val);
		if (val == 0) {
			hal_sem_post(fd->write_finish_schd);
		}
	}
	if (fd->read_schd) {
		hal_sem_getvalue(fd->read_schd, &val);
		if (val == 0) {
			hal_sem_post(fd->read_schd);
		}
	}
	if (fd->read_finish_schd) {
		hal_sem_getvalue(fd->read_finish_schd, &val);
		if (val == 0) {
			hal_sem_post(fd->read_finish_schd);
		}
	}
	return 0;
}

int usb_gadget_string_set(struct usb_function_driver *fd, char *str, unsigned int idx)
{
	unsigned int slen, blen;
	char *buffer = NULL;
	int i;

	if (!fd || idx >= USB_GADGET_MAX_IDX)
		return -1;

	if (fd->enabled != 0) {
		gadget_err("usb function[%s] already enabled", fd->name);
		return -1;
	}
	slen = strlen(str);
	if (slen <= 0)
		return -1;
	blen = 2 + (2 * slen);
	buffer = malloc(blen);
	if (!buffer) {
		gadget_err("no memory");
		return -1;
	}
	buffer[0] = blen;
	buffer[1] = USB_DT_STRING;
	for (i = 0; i < slen; i++) {
		buffer[2 + 2 * i + 0] = str[i];
		buffer[2 + 2 * i + 1] = 0;
	}

	if (fd->strings[idx] != NULL)
		free(fd->strings[idx]);
	fd->strings[idx] = (uint16_t *)buffer;

	return 0;
}

int usb_gadget_function_string_set(char *name, char *str, unsigned int idx)
{
	struct usb_function_driver *fd;

	fd = usb_gadget_function_find(name);
	if (!fd)
		return -1;
	return usb_gadget_string_set(fd, str, idx);
}

void *usb_copy_config_descriptors(struct usb_descriptor_header **src)
{
	struct usb_descriptor_header **tmp;
	unsigned bytes;
	unsigned n_desc;
	void *mem;
	void *ret;

	for (bytes = 0, n_desc = 0, tmp = src; *tmp; tmp++, n_desc++) {
		bytes += (*tmp)->bLength;
	}

	mem = malloc(bytes);
	if (!mem) {
		gadget_err("no memory");
		return NULL;
	}

	/* set usb_config_descriptor wTotalLength */
	((struct usb_config_descriptor *)(*src))->wTotalLength = bytes;

	ret = (void *)mem;
	while (*src) {
		memcpy(mem, *src, (*src)->bLength);
		mem += (*src)->bLength;
		src++;
	}

	return ret;
}

static struct usb_function_driver adb_usb_func = {
	.name = "adb",
};

#if 0
static int sunxi_adb_init(void)
{
	/* struct usb_function_driver *fd = NULL; */
	int ret = 0;

	/* usb_gadget_function_unregister(&adb_usb_func); */
	ret = usb_gadget_function_register(&adb_usb_func);
	if (ret) {
		_err("usb_gadget_function_register failed\n");
		return -1;
	}

	ret = usb_gadget_function_enable("adb");
	if (ret) {
		_err("usb_gadget_function_enable failed\n");
		return -1;
	}

	return 0;
}
#endif

static int sunxi_adb_deinit(void)
{
	usb_gadget_function_disable("adb");
	usb_gadget_function_unregister(&adb_usb_func);

	return 0;
}

