#ifndef __INCLUDE_CRYPTO_CRYPTOSOFT_H
#define __INCLUDE_CRYPTO_CRYPTOSOFT_H

#include <crypto/cryptodev.h>
#include <crypto/xform.h>

/* Software session entry */

struct hwcr_data
{
  int hw_alg; /* Algorithm */
  union
    {
      struct
      {
        FAR uint8_t *ictx;
        FAR uint8_t *octx;
        uint32_t klen;
        FAR const struct auth_hash *axf;
        union authctx ctx;
      } HWCR_AUTH;

      struct
      {
        FAR uint8_t *kschedule;
        FAR const struct enc_xform *exf;
      } HWCR_ENC;

      struct
      {
        uint32_t size;
        FAR const struct comp_algo *cxf;
      } HWCR_COMP;
    } HWCR_UN;

#define hw_ictx   HWCR_UN.HWCR_AUTH.ictx
#define hw_octx   HWCR_UN.HWCR_AUTH.octx
#define hw_klen   HWCR_UN.HWCR_AUTH.klen
#define hw_axf    HWCR_UN.HWCR_AUTH.axf
#define hw_ctx    HWCR_UN.HWCR_AUTH.ctx
#define hw_kschedule HWCR_UN.HWCR_ENC.kschedule
#define hw_exf    HWCR_UN.HWCR_ENC.exf
#define hw_size   HWCR_UN.HWCR_COMP.size
#define hw_cxf    HWCR_UN.HWCR_COMP.cxf

  struct hwcr_data *hw_next;
};

int hwcr_process(FAR struct cryptop *);
int hwcr_newsession(FAR uint32_t *, FAR struct cryptoini *);
int hwcr_freesession(uint64_t);
void hwcr_init(void);

#endif /* __INCLUDE_CRYPTO_CRYPTOSOFT_H */
