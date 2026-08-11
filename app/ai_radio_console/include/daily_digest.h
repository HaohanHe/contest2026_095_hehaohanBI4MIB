#ifndef DAILY_DIGEST_H
#define DAILY_DIGEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int  daily_digest_generate(void);
const char *daily_digest_get(void);
bool daily_digest_is_ready(void);
void daily_digest_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
