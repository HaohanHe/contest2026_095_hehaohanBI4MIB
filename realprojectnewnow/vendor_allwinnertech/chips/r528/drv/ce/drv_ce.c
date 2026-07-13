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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "crypto_hw.h"
#include "hw_hash.h"
#include "hw_aes.h"
#include <stddef.h>

FAR struct hwcr_data **hwcr_sessions = NULL;
uint32_t hwcr_sesnum = 0;
int hwcr_id = -1;

void ce_init(void)
{
	sunxi_ce_init();
}

void ce_uninit(void)
{
	sunxi_ce_uninit();
}

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
	cryptinfo("\n\t");
}

const struct auth_hash aw_auth_hash_md5 =
{
	CRYPTO_MD5, "MD5",
	0, 16, 16, 128, MD5_BLOCK_SIZE,
	(void (*) (FAR void *)) hw_md5_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_md5_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_md5_final
};

const struct auth_hash aw_auth_hash_sha1 =
{
	CRYPTO_SHA1, "SHA1",
	0, 20, 20, 128, SHA1_BLOCK_SIZE,
	(void (*) (FAR void *)) hw_sha1_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_sha1_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_sha1_final
};

const struct auth_hash aw_auth_hash_sha2_224 =
{
	CRYPTO_SHA2_224, "SHA2-224",
	0, 28, 16, 128, SHA224_BLOCK_LENGTH,
	(void (*) (FAR void *)) hw_sha224_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_sha224_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_sha224_final
};

const struct auth_hash aw_auth_hash_sha2_256 =
{
	CRYPTO_SHA2_256, "SHA2-256",
	0, 32, 16, 128, HMAC_SHA2_256_BLOCK_LEN,
	(void (*) (FAR void *)) hw_sha256_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_sha256_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_sha256_final
};

const struct auth_hash aw_auth_hash_sha2_384 =
{
	CRYPTO_SHA2_384, "SHA2-384",
	0, 48, 24, 128, HMAC_SHA2_384_BLOCK_LEN,
	(void (*) (FAR void *)) hw_sha384_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_sha384_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_sha384_final
};

const struct auth_hash aw_auth_hash_sha2_512 =
{
	CRYPTO_SHA2_512, "SHA2-512",
	0, 64, 32, 128, HMAC_SHA2_512_BLOCK_LEN,
	(void (*) (FAR void *)) hw_sha512_init, NULL, NULL,
	(int (*) (void *, const uint8_t *, size_t))hw_sha512_update,
	(void (*) (FAR uint8_t *, FAR void *))hw_sha512_final
};

/* Generate a new software session. */

int hwcr_newsession(FAR uint32_t *sid, FAR struct cryptoini *cri)
{
//cryptinfo("-------newssion---------------\n");
  FAR struct hwcr_data **hwd;
  FAR const struct auth_hash *axf;
  //FAR const struct enc_xform *txf;
  uint32_t i;

  if (sid == NULL || cri == NULL)
    {
      return -EINVAL;
    }

  if (hwcr_sessions)
    {
      for (i = 1; i < hwcr_sesnum; i++)
        {
          if (hwcr_sessions[i] == NULL)
            {
              break;
            }
        }
    }

  if (hwcr_sessions == NULL || i == hwcr_sesnum)
    {
      if (hwcr_sessions == NULL)
        {
          i = 1; /* We leave swcr_sessions[0] empty */
          hwcr_sesnum = CRYPTO_SW_SESSIONS;
        }
      else
        {
          hwcr_sesnum *= 2;
        }

      hwd = kmm_calloc(hwcr_sesnum, sizeof(struct hwcr_data *));
      if (hwd == NULL)
        {
          /* Reset session number */

          if (hwcr_sesnum == CRYPTO_SW_SESSIONS)
            {
              hwcr_sesnum = 0;
            }
          else
            {
              hwcr_sesnum /= 2;
            }

          return -ENOBUFS;
        }

      /* Copy existing sessions */

      if (hwcr_sessions)
        {
          bcopy(hwcr_sessions, hwd,
              (hwcr_sesnum / 2) * sizeof(struct hwcr_data *));
          kmm_free(hwcr_sessions);
        }

      hwcr_sessions = hwd;
    }

  hwd = &hwcr_sessions[i];
  *sid = i;

  while (cri)
    {
      *hwd = kmm_zalloc(sizeof(struct hwcr_data));
      if (*hwd == NULL)
        {
          hwcr_freesession(i);
          return -ENOBUFS;
        }
      switch (cri->cri_alg)
        {
          case CRYPTO_MD5:
            axf = &aw_auth_hash_md5;
            goto hash_common;
          case CRYPTO_SHA1:
            axf = &aw_auth_hash_sha1;
            goto hash_common;
          case CRYPTO_SHA2_224:
            axf = &aw_auth_hash_sha2_224;
            goto hash_common;
          case CRYPTO_SHA2_256:
            axf = &aw_auth_hash_sha2_256;
            goto hash_common;
          case CRYPTO_SHA2_384:
            axf = &aw_auth_hash_sha2_384;
            goto hash_common;
          case CRYPTO_SHA2_512:
            axf = &aw_auth_hash_sha2_512;
          hash_common:
            (*hwd)->hw_ictx = kmm_zalloc(axf->ctxsize);
            if ((*hwd)->hw_ictx == NULL)
              {
                hwcr_freesession(i);
                return -ENOBUFS;
              }

            axf->init((*hwd)->hw_ictx);
            //ce_init();
            (*hwd)->hw_axf = axf;
            bcopy((*hwd)->hw_ictx, &(*hwd)->hw_ctx, axf->ctxsize);
            break;

          case CRYPTO_AES_CBC:
            goto aescommon;
          case CRYPTO_AES_CTR:
          //case CRYPTO_AES_XTS:
          //  goto aescommon;
          case CRYPTO_AES_OFB:
            goto aescommon;
          case CRYPTO_AES_CFB_8:
            goto aescommon;
          case CRYPTO_AES_CFB_128:
            goto aescommon;
          aescommon:
			//ce_init();
			break;

          default:
            hwcr_freesession(i);
            return -EINVAL;
        }

      (*hwd)->hw_alg = cri->cri_alg;
      cri = cri->cri_next;
      hwd = &((*hwd)->hw_next);
    }

  return 0;
}

/* Free a session. */

int hwcr_freesession(uint64_t tid)
{
  FAR struct hwcr_data *hwd;
  //FAR const struct enc_xform *txf;
  FAR const struct auth_hash *axf;
  uint32_t sid = ((uint32_t) tid) & 0xffffffff;

  if (sid > hwcr_sesnum || hwcr_sessions == NULL ||
      hwcr_sessions[sid] == NULL)
    {
      return -EINVAL;
    }

  /* Silently accept and return */

  if (sid == 0)
    {
      return 0;
    }

  while ((hwd = hwcr_sessions[sid]) != NULL)
    {
      hwcr_sessions[sid] = hwd->hw_next;

      switch (hwd->hw_alg)
        {
          case CRYPTO_MD5:
          case CRYPTO_SHA1:
          case CRYPTO_SHA2_224:
          case CRYPTO_SHA2_256:
          case CRYPTO_SHA2_384:
          case CRYPTO_SHA2_512:
            axf = hwd->hw_axf;
            if (hwd->hw_ictx)
              {
                explicit_bzero(hwd->hw_ictx, axf->ctxsize);
                kmm_free(hwd->hw_ictx);
              }
            break;
          case CRYPTO_AES_CBC:
          case CRYPTO_AES_CTR:
          case CRYPTO_AES_XTS:
          case CRYPTO_AES_OFB:
          case CRYPTO_AES_CFB_8:
          case CRYPTO_AES_CFB_128:
            if (hwd->hw_kschedule)
            {
              kmm_free(hwd->hw_kschedule);
            }
            break;
		}
			//ce_uninit();
      kmm_free(hwd);
    }

  return 0;
}

int hwcr_hash(FAR struct cryptop *crp,
              FAR struct cryptodesc *crd,
              FAR struct hwcr_data *hw,
              caddr_t buf)
{
  FAR const struct auth_hash *axf = hw->hw_axf;

  if (crd->crd_flags & CRD_F_UPDATE)
    {
      return axf->update(&hw->hw_ctx, (FAR uint8_t *)buf + crd->crd_skip,
                         crd->crd_len);
    }
  else
    {
      axf->final((FAR uint8_t *)crp->crp_mac, &hw->hw_ctx);
    }

  return 0;
}

/* Process a hardware request. */

int hwcr_process(struct cryptop *crp)
{

  //cryptinfo("-------process---------------\n");
  FAR struct cryptodesc *crd;
  FAR struct hwcr_data *hw;
  uint32_t lid;
  int err = -1;

  /* Sanity check */

  if (crp == NULL)
    {
      return -EINVAL;
    }

  if (crp->crp_desc == NULL || crp->crp_buf == NULL)
    {
      crp->crp_etype = -EINVAL;
      goto done;
    }

  lid = crp->crp_sid & 0xffffffff;
  if (lid >= hwcr_sesnum || lid == 0 || hwcr_sessions[lid] == NULL)
    {
      crp->crp_etype = -ENOENT;
      goto done;
    }

  /* Go through crypto descriptors, processing as we go */

  for (crd = crp->crp_desc; crd; crd = crd->crd_next)
    {
      for (hw = hwcr_sessions[lid];
           hw && hw->hw_alg != crd->crd_alg;
           hw = hw->hw_next);

      /* No such context ? */

      if (hw == NULL)
        {
          crp->crp_etype = -EINVAL;
          goto done;
        }

      switch (hw->hw_alg)
        {
          case CRYPTO_NULL:
            {
              break;
            }

          case CRYPTO_MD5:
          case CRYPTO_SHA1:
          case CRYPTO_SHA2_224:
          case CRYPTO_SHA2_256:
          case CRYPTO_SHA2_384:
          case CRYPTO_SHA2_512:
			    if ((crp->crp_etype = hwcr_hash(crp, crd, hw,
                crp->crp_buf)) != 0)
              {
                goto done;
              }
            break;

          case CRYPTO_AES_CTR:
            err = hw_aes_crypto(crp->crp_dst, crp->crp_buf, crd->crd_len,
                             crp->crp_iv, crd->crd_key, crd->crd_klen / 8 - 4, hw->hw_alg,
                             crd->crd_flags & CRD_F_ENCRYPT);
            if (err < 0)
              {
                return err;
              }
            break;
          case CRYPTO_AES_CBC:
          case CRYPTO_AES_XTS:
          case CRYPTO_AES_OFB:
          case CRYPTO_AES_CFB_8:
          case CRYPTO_AES_CFB_128:
            err = hw_aes_crypto(crp->crp_dst, crp->crp_buf, crd->crd_len,
                             crp->crp_iv, crd->crd_key, crd->crd_klen / 8, hw->hw_alg,
                             crd->crd_flags & CRD_F_ENCRYPT);
            if (err < 0)
              {
                return err;
              }
            break;

          default:

            /* Unknown/unsupported algorithm */

            crp->crp_etype = -EINVAL;
            goto done;
        }
    }

  return 0;

done:
  return -1;
}

int hwcr_rsa_verify(struct cryptkop *krp)
{
  uint8_t *exp = (uint8_t *)krp->krp_param[0].crp_p;
  uint8_t *modulus = (uint8_t *)krp->krp_param[1].crp_p;
  uint8_t *sig = (uint8_t *)krp->krp_param[2].crp_p;
  uint8_t *hash = (uint8_t *)krp->krp_param[3].crp_p;
  uint8_t *padding = (uint8_t *)krp->krp_param[4].crp_p;
  int exp_len = krp->krp_param[0].crp_nbits / 8;
  int modulus_len = krp->krp_param[1].crp_nbits / 8;
  int sig_len = krp->krp_param[2].crp_nbits / 8;
  int hash_len = krp->krp_param[3].crp_nbits / 8;
  int padding_len = krp->krp_param[4].crp_nbits / 8;
  uint8_t key_e[256] = {0};
  int ret = -1;

  __attribute__((aligned(CACHELINE_LEN))) uint8_t dst_buffer[256] = {0};
	crypto_rsa_req_ctx_t *rsa_ctx = NULL;

	rsa_ctx = (crypto_rsa_req_ctx_t *)kmm_memalign(CACHELINE_LEN, sizeof(crypto_rsa_req_ctx_t));
	if (rsa_ctx == NULL) {
		_err (" malloc rsa ctx fail\n");
		return -1;
	}

  memset(dst_buffer, 0, 256);
	memset(rsa_ctx, 0, sizeof(crypto_rsa_req_ctx_t));
  memcpy(key_e, exp, exp_len);

	rsa_ctx->key_n = modulus;
	rsa_ctx->n_len = modulus_len;
	rsa_ctx->key_e = key_e;
	rsa_ctx->e_len = modulus_len;
	rsa_ctx->key_d = 0;
	rsa_ctx->d_len = 0;

	rsa_ctx->src_buffer = sig;
	rsa_ctx->src_length = sig_len;
	rsa_ctx->dst_buffer = dst_buffer;
	rsa_ctx->dst_length = sig_len;

	rsa_ctx->dir = 0;
	rsa_ctx->type = 0x20; /*CE_METHOD_RSA*/
	rsa_ctx->bitwidth = sig_len * 8;

	ret = do_rsa_pkcs15_verity(rsa_ctx);
	if (ret < 0) {
		_err ("do rsa crypto failed: %d\n", ret);
		return ret;
	}

  if (memcmp(dst_buffer, hash, hash_len)) {
    crypterr("rsa encrypt failed \n");
	ce_dump("want data: ", hash, hash_len, 16);
	ce_dump("calc data: ", dst_buffer, hash_len, 16);
  }
  if (memcmp(dst_buffer + hash_len, padding, padding_len)) {
    crypterr("rsa encrypt failed\n");
	ce_dump("want data: ", padding, padding_len, 16);
	ce_dump("calc data: ", dst_buffer + hash_len, padding_len, 16);
  }

  return !!memcmp(dst_buffer, hash, hash_len) +
       !!memcmp(dst_buffer + hash_len, padding, padding_len);
}

int hwcr_kprocess(struct cryptkop *krp)
{
  /* Sanity check */
  //cryptinfo("----------start rsa verity-----------\n");
  if (krp == NULL)
    {
      return -EINVAL;
    }

  /* Go through crypto descriptors, processing as we go */

  switch (krp->krp_op)
    {
      case CRK_RSA_PKCS15_VERIFY:
        ce_init();
        if ((krp->krp_status = hwcr_rsa_verify(krp)) != 0)
          {
            goto done;
          }
        ce_uninit();
        break;
      default:

        /* Unknown/unsupported algorithm */

        krp->krp_status = -EINVAL;
        goto done;
    }

done:
  return 0;
}

/* Initialize the driver, called from the kernel main(). */

void hwcr_init(void)
{
  int algs[CRYPTO_ALGORITHM_MAX + 1];
  int kalgs[CRK_ALGORITHM_MAX + 1];
  int flags = 0;

  hwcr_id = crypto_get_driverid(flags);
  if (hwcr_id < 0)
  {
      /* This should never happen */

   PANIC();
  }

  //algs[CRYPTO_3DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_BLF_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_CAST_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_MD5_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_RIPEMD160_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_RIJNDAEL128_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_AES_CTR] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_AES_XTS] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_AES_GCM_16] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_AES_GMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_NULL] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_AES_OFB] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_AES_CFB_8] = CRYPTO_ALG_FLAG_SUPPORTED;
  //algs[CRYPTO_AES_CFB_128] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_MD5] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_SHA1] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_SHA2_224] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_SHA2_256] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_SHA2_384] = CRYPTO_ALG_FLAG_SUPPORTED;
  algs[CRYPTO_SHA2_512] = CRYPTO_ALG_FLAG_SUPPORTED;

  crypto_register(hwcr_id, algs, hwcr_newsession,
                  hwcr_freesession, hwcr_process);

  kalgs[CRK_RSA_PKCS15_VERIFY] = CRYPTO_ALG_FLAG_SUPPORTED;
  crypto_kregister(hwcr_id, kalgs, hwcr_kprocess);
}
