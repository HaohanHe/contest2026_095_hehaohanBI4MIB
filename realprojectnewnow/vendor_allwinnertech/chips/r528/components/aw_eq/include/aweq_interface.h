#ifndef _AWEQ_INTERFACE_H_
#define _AWEQ_INTERFACE_H_
#include "eq.h"

#define LINE_STR_BUF_LEN_MAX 128

int parse_config_to_eq_prms(const char *config_file,
				eq_prms_t *prms, int *enabled);
void print_eq_prms(const eq_prms_t *prms);

int init_eq_prms(int bin_num, eq_prms_t *prms,
				const char *config_file, int *file_en);
int deinit_eq_prms(const eq_prms_t *prms);

#endif
