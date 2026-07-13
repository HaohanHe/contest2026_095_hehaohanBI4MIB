#include <stdio.h>
#include "aweq_interface.h"

int parse_config_to_eq_prms(const char *config_file, eq_prms_t *prms, int *enabled)
{
    int ret;
    FILE *fp = NULL;
    char line_str[LINE_STR_BUF_LEN_MAX];
    int temp_int;
    int type;
    int frequency;
    int gain;
    float Q;
    int index = 0;

    if (!config_file || !prms) {
        printf("Invalid config_file or prms\n");
        ret = -1;
        goto out;
    }

    fp = fopen(config_file, "r");
    if (!fp) {
        printf("Failed to open %s\n", config_file);
        ret = -1;
        goto out;
    }

    memset(line_str, 0, sizeof(line_str));

    while (fgets(line_str, sizeof(line_str), fp)) {
        if (sscanf(line_str, "channels=%d", &temp_int) == 1) {
            prms->chan = temp_int;
        } else if (sscanf(line_str, "enabled=%d", &temp_int) == 1) {
            *enabled = temp_int;
        } else if (sscanf(line_str, "bin_num=%d", &temp_int) == 1) {
            prms->biq_num = temp_int;
        } else if (sscanf(line_str, "samplerate=%d", &temp_int) == 1) {
            prms->sampling_rate = temp_int;
        } else if (sscanf(line_str, "params=%d %d %d %f",
                    &type, &frequency, &gain, &Q) == 4) {
            prms->core_prms[index].type = type;
            prms->core_prms[index].fc = frequency;
            prms->core_prms[index].G = gain;
            prms->core_prms[index].Q = Q;
            ++index;
        }

        memset(line_str, 0, sizeof(line_str));
    }

    ret = 0;

    fclose(fp);
out:
    return ret;
}

void print_eq_prms(const eq_prms_t *prms)
{
    int i;
    for (i = 0; i < prms->biq_num; ++i) {
        const eq_core_prms_t *core_prms = &prms->core_prms[i];
        printf("----------------- printf eq parameter ---------------------\n");
        printf(" [Biquad%02i] type: %i, freq: %d, gain: %d, Q: %.2f\n",
                i + 1, core_prms->type, core_prms->fc, core_prms->G, core_prms->Q);
        printf("-----------------------------------------------------------\n");
    }
}

int init_eq_prms(int bin_num, eq_prms_t *prms,
                    const char *config_file, int *file_en)
{
    memset(prms, 0x00, sizeof(eq_prms_t));
    prms->core_prms = calloc(sizeof(eq_core_prms_t), bin_num);
    if (!prms->core_prms) {
        printf("init_eq_prms err, calloc failed!");
        return -1;
    }

    if (parse_config_to_eq_prms(config_file, prms, file_en)) {
	free(prms->core_prms);
	prms->core_prms = NULL;
	printf("init_eq_prms err, parse_config_to_eq_prms failed!");
	return -1;
    }
//    print_eq_prms(prms);

    return 0;
}

int deinit_eq_prms(const eq_prms_t *prms)
{
    if (!prms || !prms->core_prms) {
        printf("deinit_eq_prms err, pointer is NULL\n");
        return -1;
    }

    free(prms->core_prms);
    memset((void*)prms, 0x00, sizeof(eq_prms_t));

    return 0;
}

