#include <assert.h>
#include <errno.h>
#include <nuttx/kmalloc.h>
#include <sys/param.h>
#include <sunxi_hal_ce.h>

#define CE_METHOD_AES				0
#define CE_METHOD_DES				1
#define CE_METHOD_3DES				2
#define CE_METHOD_MD5				16
#define CE_METHOD_SHA1				17
#define CE_METHOD_SHA224			18
#define CE_METHOD_SHA256			19
#define CE_METHOD_SHA384			20
#define CE_METHOD_SHA512			21
#define CE_METHOD_HMAC_SHA1			22
#define CE_METHOD_HMAC_SHA256		23
#define CE_METHOD_RSA				32


struct aw_md5_context_s
{
  uint8_t state[MD5_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[MD5_BLOCK_SIZE];   /* data block being processed */
};

struct aw_sha1_context_s
{
  uint8_t state[SHA1_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[SHA1_BLOCK_SIZE];   /* data block being processed */
};

struct aw_sha224_context_s
{
  uint8_t state[SHA256_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[SHA224_BLOCK_SIZE];   /* data block being processed */
};

struct aw_sha256_context_s
{
  uint8_t state[SHA256_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[SHA256_BLOCK_SIZE];   /* data block being processed */
};

struct aw_sha384_context_s
{
  uint8_t state[SHA512_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[SHA384_BLOCK_SIZE];   /* data block being processed */
};

struct aw_sha512_context_s
{
  uint8_t state[SHA512_DIGEST_SIZE];          /* intermediate digest state  */
  uint32_t mode;
  uint32_t hash_size;
  uint32_t count;
  uint32_t iv_input_flags;
  unsigned char buffer[SHA512_BLOCK_SIZE];   /* data block being processed */
};

void hw_md5_init(void *ctx);
int hw_md5_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_md5_final(uint8_t *out, void *ctx);
void hw_sha1_init(void *ctx);
int hw_sha1_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_sha1_final(uint8_t *out, void *ctx);
void hw_sha224_init(void *ctx);
int hw_sha224_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_sha224_final(uint8_t *out, void *ctx);
void hw_sha256_init(void *ctx);
int hw_sha256_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_sha256_final(uint8_t *out, void *ctx);
void hw_sha384_init(void *ctx);
int hw_sha384_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_sha384_final(uint8_t *out, void *ctx);
void hw_sha512_init(void *ctx);
int hw_sha512_update(void *ctx, const uint8_t *in, uint32_t len);
void hw_sha512_final(uint8_t *out, void *ctx);

int hash_sw_padding(FAR const void *inputptr, FAR uint32_t last_block_size, FAR uint32_t total_len, FAR uint32_t mode);
int hw_hash_crypto(FAR uint8_t *state, FAR const void *inputptr, FAR uint32_t block_size, FAR uint32_t mode, FAR uint32_t hash_size, uint32_t last);
