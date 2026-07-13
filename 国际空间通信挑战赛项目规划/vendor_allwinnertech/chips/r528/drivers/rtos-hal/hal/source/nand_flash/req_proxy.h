#ifndef __REQUEST_PROXY_H__
#define __REQUEST_PROXY_H__

#include <stddef.h>

typedef int (* req_proxy_handler_t)(FAR void *priv, FAR void *data);

int req_proxy_request(FAR void *_hdl, FAR void *data);
void req_proxy_destroy(FAR void *_hdl);
void *req_proxy_create(FAR const char *name, FAR req_proxy_handler_t handler, FAR void *priv, size_t data_size, size_t queue_depth);

#endif /* __REQUEST_PROXY_H__ */
