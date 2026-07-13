#include "hw_hash.h"
#include "crypto_hw.h"

static uint32_t ce_hash_endian4(uint32_t data)
{
	uint32_t d1, d2, d3, d4;

	d1 = (data & 0xff) << 24;
	d2 = (data & 0xff00) << 8;
	d3 = (data & 0xff0000) >> 8;
	d4 = (data & 0xff000000) >> 24;

	return (d1 | d2 | d3 | d4);
}

static uint32_t ce_hash_blk_size(int type)
{
        if ((type == CE_METHOD_SHA384) || (type == CE_METHOD_SHA512))
                return SHA512_BLOCK_SIZE;
        return SHA1_BLOCK_SIZE;
}

int hash_sw_padding(FAR const void *inputptr, FAR uint32_t last_block_size, FAR uint32_t total_len, FAR uint32_t mode)
{
        uint32_t blk_size = ce_hash_blk_size(mode);
        uint32_t len_threshold = (blk_size == 128) ? 112 : 56;
        uint32_t n = total_len % blk_size;
        uint8_t padding[256] = {0};
        uint32_t padding_len = 0;
        uint8_t *p = padding;
        uint32_t len_l = total_len << 3;  /* total len, in bits. */
        uint32_t len_h = total_len >> 29;
        uint32_t big_endian = (mode == CE_METHOD_MD5) ? 0 : 1;

	if (n) {
		memcpy(padding, inputptr + last_block_size - n, n);
	}

        //CE_DBG("ctx->type = %d, n = %d, ctx->src_length = %d\n", ctx->type, n, ctx->src_length);
        p[n] = 0x80;
        n++;

        if (n > len_threshold) { /* The pad data need two blocks. */
                memset(p+n, 0, blk_size*2 - n);
                p += blk_size*2 - 8;
        } else {
                memset(p+n, 0, blk_size - n);
                p += blk_size - 8;
        }

        if (big_endian == 1) {
#if 0
                /* The length should use bit64 in SHA384/512 case.
                 * The OpenSSL package is always small than 8K,
                 * so we use still bit32.
                 */
                if (blk_size == SHA512_BLOCK_SIZE) {
                        int len_hh = ctx->cnt >> 61;
                        *(int *)(p-4) = ce_hash_endian4(len_hh);
                }
#endif
                *(int *)p = ce_hash_endian4(len_h);
                *(int *)(p+4) = ce_hash_endian4(len_l);
        } else {
                *(int *)p = len_l;
                *(int *)(p+4) = len_h;
        }


	padding_len = (uint32_t)(p + 8 - padding);
  memcpy((void *)inputptr, padding, padding_len);

	/*CE_DBG("After padding %d: %02x %02x %02x %02x   %02x %02x %02x %02x\n",
		ctx->padding_len,
		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);*/

        return padding_len;
}

int hw_hash_crypto(FAR uint8_t *state, FAR const void *inputptr, FAR uint32_t block_size, FAR uint32_t mode, FAR uint32_t hash_size, uint32_t last)
{
	crypto_hash_req_ctx_t *hash_ctx = NULL;
	uint8_t *dst_data = NULL;
  uint8_t *tmp_data = NULL;
	int ret = 0;

	hash_ctx = kmm_memalign(0x40, sizeof(crypto_hash_req_ctx_t));
	if (hash_ctx == NULL) {
		return -ENOBUFS;
	}
	memset(hash_ctx, 0x0, sizeof(crypto_hash_req_ctx_t));

	dst_data = kmm_memalign(0x40, hash_size);
	if (dst_data == NULL) {
		kmm_free(hash_ctx);
		return -ENOBUFS;
	}
  memset(dst_data, 0x0, hash_size);
  tmp_data = kmm_memalign(0x40, block_size);
	if (tmp_data == NULL) {
		syslog(LOG_ERR,"malloc align mem failed: 0x%lx\n", block_size);
		kmm_free(hash_ctx);
		kmm_free(dst_data);
		return -ENOBUFS;
	}
  memset(tmp_data, 0x0, block_size);
  memcpy(tmp_data, inputptr, block_size);

	hash_ctx->type = mode;
	hash_ctx->src_buffer = tmp_data;
    hash_ctx->src_length = block_size;
	hash_ctx->dst_buffer = dst_data;
	hash_ctx->dst_length = hash_size;

  if (last) {
	hash_ctx->md_size = hash_size;
    memcpy(hash_ctx->md, state, hash_size);
  }
  else {
    hash_ctx->md_size = 0;
  }

	ret = ce_hash_update(hash_ctx);
	if (ret < 0) {
		syslog(LOG_INFO,"update fail:%d\n", ret);
		kmm_free(hash_ctx);
		kmm_free(dst_data);
		kmm_free(tmp_data);
		return -EINVAL;
	}
	memcpy(state, dst_data, hash_size);

	kmm_free(hash_ctx);
	kmm_free(dst_data);
	kmm_free(tmp_data);
	return 0;
}

static void aw_md5_init(struct aw_md5_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_MD5;
  ctx->hash_size = MD5_DIGEST_SIZE;
  memset(ctx->state, 0, MD5_DIGEST_SIZE);
}

static int aw_md5_update(struct aw_md5_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  uint32_t caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (MD5_BLOCK_LENGTH - 1));
  need = MD5_BLOCK_LENGTH - have;
  ctx->count += (uint64_t)len;


  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, MD5_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= MD5_BLOCK_LENGTH) {
          caclulate_block= len/MD5_BLOCK_LENGTH;
          ret = hw_hash_crypto(ctx->state, input, caclulate_block * MD5_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * MD5_BLOCK_LENGTH;
          len -= caclulate_block * MD5_BLOCK_LENGTH;
          ctx->iv_input_flags = 1;
      }
    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
    }

    return 0;
}

static void aw_md5_final(FAR unsigned char *digest, struct aw_md5_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % MD5_BLOCK_LENGTH;
  int ret = 0;

  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, MD5_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}


static void aw_sha1_init(struct aw_sha1_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_SHA1;
  ctx->iv_input_flags = 0;
  ctx->hash_size = SHA1_DIGEST_SIZE;
  memset(ctx->state, 0, SHA1_DIGEST_SIZE);
}

static int aw_sha1_update(struct aw_sha1_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  uint32_t caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (SHA1_BLOCK_LENGTH - 1));
  need = SHA1_BLOCK_LENGTH - have;
  ctx->count += (uint64_t)len;


  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, SHA1_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= SHA1_BLOCK_LENGTH) {
          caclulate_block= len/SHA1_BLOCK_LENGTH;
          ret = hw_hash_crypto(ctx->state, input, caclulate_block * SHA1_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * SHA1_BLOCK_LENGTH;
          len -= caclulate_block * SHA1_BLOCK_LENGTH;
          ctx->iv_input_flags = 1;
      }
    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
      //ctx->count += len;
    }

    return 0;
}

static void aw_sha1_final(FAR unsigned char *digest, struct aw_sha1_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % SHA1_BLOCK_LENGTH;
  int ret = 0;

  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, SHA1_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}

static void aw_sha224_init(struct aw_sha224_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_SHA224;
  ctx->iv_input_flags = 0;
  ctx->hash_size = SHA256_DIGEST_SIZE;
  memset(ctx->state, 0, SHA256_DIGEST_SIZE);
}

static int aw_sha224_update(struct aw_sha224_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  uint32_t caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (SHA224_BLOCK_LENGTH - 1));
  need = SHA224_BLOCK_LENGTH - have;
  ctx->count += (uint64_t)len;

  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, SHA224_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= SHA224_BLOCK_LENGTH) {
          caclulate_block= len/SHA224_BLOCK_LENGTH;
          ret = hw_hash_crypto(ctx->state, input, caclulate_block * SHA224_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * SHA224_BLOCK_LENGTH;
          len -= caclulate_block * SHA224_BLOCK_LENGTH;
          ctx->iv_input_flags = 1;
      }
    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
      //ctx->count += len;
    }

    return 0;
}

static void aw_sha224_final(FAR unsigned char *digest, struct aw_sha224_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % SHA224_BLOCK_LENGTH;
  int ret = 0;

  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, SHA224_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}

static void aw_sha256_init(struct aw_sha256_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_SHA256;
  ctx->iv_input_flags = 0;
  ctx->hash_size = SHA256_DIGEST_SIZE;
  memset(ctx->state, 0, SHA256_DIGEST_SIZE);
}

static int aw_sha256_update(struct aw_sha256_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  int caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (SHA256_BLOCK_LENGTH - 1));
  need = SHA256_BLOCK_LENGTH - have;
  ctx->count += len;


  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, SHA256_BLOCK_LENGTH, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= SHA256_BLOCK_LENGTH) {
          caclulate_block= len/SHA256_BLOCK_LENGTH;
          ret = hw_hash_crypto(ctx->state, input, (caclulate_block * SHA256_BLOCK_LENGTH), ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * SHA256_BLOCK_LENGTH;
          len -= caclulate_block * SHA256_BLOCK_LENGTH;
          ctx->iv_input_flags = 1;
      }

    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
      //ctx->count += len;
    }

    return 0;
}

static void aw_sha256_final(FAR unsigned char *digest, struct aw_sha256_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % SHA256_BLOCK_LENGTH;
  int ret = 0;


  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, SHA256_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}

static void aw_sha384_init(struct aw_sha384_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_SHA384;
  ctx->iv_input_flags = 0;
  ctx->hash_size = SHA512_DIGEST_SIZE;
  memset(ctx->state, 0, SHA512_DIGEST_SIZE);
}

static int aw_sha384_update(struct aw_sha384_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  uint32_t caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (SHA384_BLOCK_SIZE - 1));
  need = SHA384_BLOCK_SIZE - have;
  ctx->count += (uint64_t)len;

  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, SHA384_BLOCK_SIZE, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= SHA384_BLOCK_SIZE) {
          caclulate_block= len/SHA384_BLOCK_SIZE;
          ret = hw_hash_crypto(ctx->state, input, caclulate_block * SHA384_BLOCK_SIZE, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * SHA384_BLOCK_SIZE;
          len -= caclulate_block * SHA384_BLOCK_SIZE;
          ctx->iv_input_flags = 1;
      }
    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
      //ctx->count += len;
    }

    return 0;
}

static void aw_sha384_final(FAR unsigned char *digest, struct aw_sha384_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % SHA384_BLOCK_SIZE;
  int ret = 0;

  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, SHA384_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}

static void aw_sha512_init(struct aw_sha512_context_s *ctx)
{
  ctx->count = 0;
  ctx->mode = CE_METHOD_SHA512;
  ctx->iv_input_flags = 0;
  ctx->hash_size = SHA512_DIGEST_SIZE;
  memset(ctx->state, 0, SHA512_DIGEST_SIZE);
}

static int aw_sha512_update(struct aw_sha512_context_s *ctx, const unsigned char *inputptr, uint32_t len)
{
  FAR const uint8_t *input = inputptr;
  uint32_t caclulate_block;
  size_t have;
  size_t need;
  int ret = 0;

  /* Check how many bytes we already have and how many more we need. */

  have = (size_t)((ctx->count) & (SHA512_BLOCK_SIZE - 1));
  need = SHA512_BLOCK_SIZE - have;
  ctx->count += (uint64_t)len;


  /* Update bitcount */
  if (len >= need)
    {
      if (have != 0)
        {
          memcpy(ctx->buffer + have, input, need);
          ret = hw_hash_crypto(ctx->state, ctx->buffer, SHA512_BLOCK_SIZE, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += need;
          len -= need;
          have = 0;
          ctx->iv_input_flags = 1;
        }
      if (len >= SHA512_BLOCK_SIZE) {
          caclulate_block= len/SHA512_BLOCK_SIZE;
          ret = hw_hash_crypto(ctx->state, input, caclulate_block * SHA512_BLOCK_SIZE, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
          if (ret) {
	      syslog(LOG_ERR, "crypto fail: %d\n", ret);
              return ret;
          }
          input += caclulate_block * SHA512_BLOCK_SIZE;
          len -= caclulate_block * SHA512_BLOCK_SIZE;
          ctx->iv_input_flags = 1;
      }
    }

  /* Handle any remaining bytes of data. */

  if (len != 0)
    {
      memcpy(ctx->buffer + have, input, len);
      //ctx->count += len;
    }

    return 0;
}

static void aw_sha512_final(FAR unsigned char *digest, struct aw_sha512_context_s *ctx)
{
  uint32_t last_package_size = ctx->count % SHA512_BLOCK_SIZE;
  int ret = 0;
  last_package_size = hash_sw_padding(ctx->buffer, last_package_size, ctx->count, ctx->mode);
  ret = hw_hash_crypto(ctx->state, ctx->buffer, last_package_size, ctx->mode, ctx->hash_size, ctx->iv_input_flags);
  if (ret)
	  syslog(LOG_ERR, "crypto fail: %d\n", ret);

  memcpy(digest, ctx->state, SHA512_DIGEST_SIZE);
  explicit_bzero(ctx, sizeof(*ctx)); /* in case it's sensitive */
}

void hw_md5_init(void *ctx)
{
	aw_md5_init(ctx);
}

int hw_md5_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_md5_update((struct aw_md5_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_md5_final(uint8_t *out, void *ctx)
{
	aw_md5_final((unsigned char *)out, (struct aw_md5_context_s *)ctx);
}

void hw_sha1_init(void *ctx)
{
	aw_sha1_init(ctx);
}

int hw_sha1_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_sha1_update((struct aw_sha1_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_sha1_final(uint8_t *out, void *ctx)
{
	aw_sha1_final((unsigned char *)out, (struct aw_sha1_context_s *)ctx);
}

void hw_sha224_init(void *ctx)
{
	aw_sha224_init(ctx);
}

int hw_sha224_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_sha224_update((struct aw_sha224_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_sha224_final(uint8_t *out, void *ctx)
{
	aw_sha224_final((unsigned char *)out, (struct aw_sha224_context_s *)ctx);
}

void hw_sha256_init(void *ctx)
{
	aw_sha256_init(ctx);
}

int hw_sha256_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_sha256_update((struct aw_sha256_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_sha256_final(uint8_t *out, void *ctx)
{
	aw_sha256_final((unsigned char *)out, (struct aw_sha256_context_s *)ctx);
}

void hw_sha384_init(void *ctx)
{
	aw_sha384_init(ctx);
}

int hw_sha384_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_sha384_update((struct aw_sha384_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_sha384_final(uint8_t *out, void *ctx)
{
	aw_sha384_final((unsigned char *)out, (struct aw_sha384_context_s *)ctx);
}

void hw_sha512_init(void *ctx)
{
	aw_sha512_init(ctx);
}

int hw_sha512_update(void *ctx, const uint8_t *in, uint32_t len)
{
	return aw_sha512_update((struct aw_sha512_context_s *)ctx,
								(const unsigned char *)in,
								(size_t)len);
}

void hw_sha512_final(uint8_t *out, void *ctx)
{
	aw_sha512_final((unsigned char *)out, (struct aw_sha512_context_s *)ctx);
}

