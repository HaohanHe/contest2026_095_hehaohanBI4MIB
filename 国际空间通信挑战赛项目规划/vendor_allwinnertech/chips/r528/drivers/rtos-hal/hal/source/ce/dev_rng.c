#include <nuttx/config.h>

#include <poll.h>
#include <string.h>
#include <sys/random.h>

#include <nuttx/arch.h>
#include <nuttx/drivers/drivers.h>
#include <nuttx/fs/fs.h>

#include <sunxi_hal_ce.h>
#include <hal_mem.h>

#include <stdio.h>
#include <debug.h>

#if defined(CONFIG_DEV_RANDOM) || defined(CONFIG_DEV_URANDOM)

#if 0
static ssize_t devurand_read(FAR struct file *filep, FAR char *buffer, size_t len)
{
    int ret;
	crypto_rng_req_ctx_t *rng_ctx = NULL;
    uint8_t *rng_buf = NULL;

	rng_ctx = (crypto_rng_req_ctx_t *)hal_malloc_align(sizeof(crypto_rng_req_ctx_t), max(CE_ALIGN_SIZE, CACHELINE_LEN));
	if (rng_ctx == NULL) {
		crypterr("malloc rng ctx fail\n");
		ret = -1;
		goto exit;
	}

	rng_buf = (uint8_t *)hal_malloc_align(len, max(CE_ALIGN_SIZE, CACHELINE_LEN));
	if (rng_buf == NULL) {
		crypterr("malloc rng buffer fail\n");
		ret = -1;
		goto exit;
	}

    rng_ctx->rng_buf = rng_buf;
    rng_ctx->rng_len = len;
    rng_ctx->mode = 0x30; /*CE_METHOD_TRNG*/
    rng_ctx->key = NULL;
    rng_ctx->key_len = 0;

    ret = do_rng_gen(rng_ctx);
    if (0 > ret) {
        crypterr("do_rng_gen fail\n");
		goto exit;
    }

    memcpy(buffer, rng_buf, len);
    ret = len;
exit:
	if (rng_ctx)
		hal_free_align(rng_ctx);

	if (rng_buf)
		hal_free_align(rng_buf);

    return ret;
}

#else

#ifndef ALIGN_UP
#define ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))
#endif

static ssize_t devurand_read(FAR struct file *filep, FAR char *buffer, size_t len)
{
    int ret;
	crypto_rng_req_ctx_t *rng_ctx = NULL;
    uint8_t *rng_buf = NULL;
    size_t align_len = ALIGN_UP(len, 32);

	rng_ctx = (crypto_rng_req_ctx_t *)hal_malloc_align(sizeof(crypto_rng_req_ctx_t), max(CE_ALIGN_SIZE, CACHELINE_LEN));
	if (rng_ctx == NULL) {
		crypterr("malloc rng ctx fail\n");
		ret = -1;
		goto exit;
	}
	memset(rng_ctx, 0x0, sizeof(crypto_rng_req_ctx_t));

	rng_buf = (uint8_t *)hal_malloc_align(align_len, max(CE_ALIGN_SIZE, CACHELINE_LEN));
	if (rng_buf == NULL) {
		crypterr("malloc rng bufrer fail\n");
		ret = -1;
		goto exit;
	}
	memset(rng_buf, 0x0, sizeof(align_len));

    rng_ctx->rng_buf = rng_buf;
    rng_ctx->rng_len = align_len;
    rng_ctx->mode = 0x30; /*CE_METHOD_TRNG*/
    rng_ctx->key = NULL;
    rng_ctx->key_len = 0;

    ret = do_rng_gen_sha256(rng_ctx);
    if (0 > ret) {
        crypterr("do_rng_gen fail\n");
		goto exit;
    }

    memcpy(buffer, rng_buf, len);
    ret = len;
exit:
	if (rng_ctx)
		hal_free_align(rng_ctx);

	if (rng_buf)
		hal_free_align(rng_buf);

    return ret;
}
#endif

static const struct file_operations g_rngops =
{
    NULL,            /* open */
    NULL,            /* close */
    devurand_read,   /* read */
};

#endif

#ifdef CONFIG_DEV_RANDOM
void devrandom_register(void)
{
    register_driver("/dev/random", &g_rngops, 0444, NULL);
}
#endif

#ifdef CONFIG_DEV_URANDOM
void devurandom_register(void)
{
    register_driver("/dev/urandom", &g_rngops, 0444, NULL);
}
#endif
