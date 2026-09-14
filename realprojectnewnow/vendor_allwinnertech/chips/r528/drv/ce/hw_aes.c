#include "hw_aes.h"
#include "crypto_hw.h"

#if 0
static void ce_dump(char *str,unsigned char *data, int len, int align)
{
	int i = 0;
	if(str)
		cryptinfo("\n%s: ",str);
	for(i = 0; i<len; i++)
	{
		if((i%align) == 0)
		{
			cryptinfo("\n");
			cryptinfo("0x08%x: ", (unsigned int)data + i * align);
		}
		cryptinfo("%02x ",*(data++));
	}
	cryptinfo("\n");
}
#endif

int hw_aes_crypto(void *out, const void *in, size_t size,
               const void *iv, const void *key, size_t keysize,
               int mode, int encrypt)
{
  int ret = -1;
  uint8_t *tmp_data = NULL;
  uint8_t ctr_iv[16] = {0};
  uint8_t rand[4] = {0x00, 0x00, 0x00, 0x01};
	__attribute__((aligned(CACHELINE_LEN))) uint8_t iv_next[AES_BLOCK_SIZE] = {0};
	crypto_aes_req_ctx_t *aes_ctx = NULL;

  aes_ctx = kmm_memalign(CACHELINE_LEN, sizeof(crypto_aes_req_ctx_t));
	if (aes_ctx == NULL) {
		_err (" malloc data buffer fail\n");
		return -1;
	}
	memset(aes_ctx, 0x0, sizeof(crypto_aes_req_ctx_t));
  tmp_data = kmm_memalign(CACHELINE_LEN, size);
  memset(tmp_data, 0x0, size);
  memcpy(tmp_data, in, size);

  aes_ctx->dst_buffer = kmm_memalign(CACHELINE_LEN, 512);
	if (aes_ctx->dst_buffer == NULL) {
		_err (" malloc dest buffer fail\n");
    kmm_free(aes_ctx);
		ret = -1;
	}

  switch (mode) {
    case CRYPTO_AES_CBC:
	    aes_ctx->mode = AES_MODE_CBC;
      break;
    case CRYPTO_AES_CTR:
      aes_ctx->mode = AES_MODE_CTR;
      memcpy(ctr_iv, key + keysize, 4);
      memcpy(ctr_iv + 4, iv, 8);
	  memcpy(ctr_iv + 12, rand, 4);
      break;
    //case CRYPTO_AES_XTS:
      //aes_ctx->mode = AES_MODE_XTS;
      //break;
    //case CRYPTO_AES_CTS:
      //aes_ctx->mode = AES_MODE_CTS;
      //break;
    case CRYPTO_AES_OFB:
      aes_ctx->mode = AES_MODE_OFB;
      break;
    case CRYPTO_AES_CFB_128:
      aes_ctx->mode = AES_MODE_CFB;
      break;
    default:
        _err("input error mode\n");
        goto out;
  }
	aes_ctx->src_buffer = tmp_data;
	aes_ctx->src_length = size;
	aes_ctx->key = (uint8_t *)key;
	aes_ctx->key_length = keysize;
	if (aes_ctx->mode == AES_MODE_ECB)
		aes_ctx->iv = NULL;
	else if(aes_ctx->mode == AES_MODE_CTR)
        aes_ctx->iv = ctr_iv;
    else
	  aes_ctx->iv = (uint8_t *)iv;
	if (aes_ctx->mode == AES_MODE_CTR) {
		memset(iv_next, 0, AES_BLOCK_SIZE);
		//memcpy(iv_next, key + keysize, 4);
		aes_ctx->iv_next = iv_next;
	} else
		aes_ctx->iv_next = NULL;
	if (aes_ctx->mode == AES_MODE_CFB)
		aes_ctx->bitwidth = 128;
	else
		aes_ctx->bitwidth = 0;
  if(encrypt)
	  aes_ctx->dir = 0;
  else
    aes_ctx->dir = 1;
	aes_ctx->dst_length = CE_ROUND_UP(aes_ctx->src_length, AES_BLOCK_SIZE);

	/* debuge */
#if 0
  ce_dump("src:", aes_ctx->src_buffer, aes_ctx->src_length, 16);
  ce_dump("iv:", aes_ctx->iv, 16, 16);
  ce_dump("iv_next:", aes_ctx->iv_next, AES_BLOCK_SIZE, 16);
  ce_dump("key:", aes_ctx->key, aes_ctx->key_length, 16);
#endif

  ret = do_aes_crypto(aes_ctx);
		if (ret < 0) {
	  	_err ("aes crypto fail %d\n", ret);
      goto out;
	}

  if (aes_ctx->dir == 0)
	memcpy((void *)iv, aes_ctx->dst_buffer, aes_ctx->dst_length);
  else
	memcpy((void *)iv, aes_ctx->src_buffer, aes_ctx->src_length);
  memcpy(out, aes_ctx->dst_buffer, aes_ctx->dst_length);

out:
  kmm_free(aes_ctx->dst_buffer);
  kmm_free(aes_ctx);
  return ret;
}
