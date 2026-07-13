#include <assert.h>
#include <errno.h>
#include <nuttx/kmalloc.h>
#include <sys/param.h>
#include <sunxi_hal_ce.h>

#define AES_MODE_ECB        (0)
#define AES_MODE_CBC        (1)
#define AES_MODE_CTR        (2)
#define AES_MODE_CTS        (3)
#define AES_MODE_OFB        (4)
#define AES_MODE_CFB        (5)

int hw_aes_crypto(void *out, const void *in, size_t size, const void *iv,
                    const void *key, size_t keysize, int mode, int encrypt);
