#include "config_store.h"
#include "radio_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>

void config_set_defaults(ai_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->provider = ASR_PROVIDER_SILICONFLOW;
    strncpy(config->asr_model, DEFAULT_ASR_MODEL, MAX_MODEL_NAME_LEN - 1);
    strncpy(config->llm_model, DEFAULT_LLM_MODEL, MAX_MODEL_NAME_LEN - 1);
    strncpy(config->asr_endpoint, SILICONFLOW_ASR_ENDPOINT, MAX_ENDPOINT_LEN - 1);
    strncpy(config->chat_endpoint, SILICONFLOW_CHAT_ENDPOINT, MAX_ENDPOINT_LEN - 1);
    config->asr_enabled = false;
    config->llm_enabled = false;
    config->mayday_detection_via_llm = true;
    config->violation_detection = true;
    config->auto_logging = true;
    config->realtime_translation = false;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *e = s + strlen(s) - 1;
    while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) {
        *e = '\0';
        e--;
    }
    return s;
}

static void set_bool_field(ai_config_t *c, const char *key, bool val)
{
    if (strcmp(key, "asr_enabled") == 0) c->asr_enabled = val;
    else if (strcmp(key, "llm_enabled") == 0) c->llm_enabled = val;
    else if (strcmp(key, "mayday_detection_via_llm") == 0) c->mayday_detection_via_llm = val;
    else if (strcmp(key, "violation_detection") == 0) c->violation_detection = val;
    else if (strcmp(key, "auto_logging") == 0) c->auto_logging = val;
    else if (strcmp(key, "realtime_translation") == 0) c->realtime_translation = val;
}

static void set_str_field(ai_config_t *c, const char *key, const char *val)
{
    if (strcmp(key, "api_key") == 0) strncpy(c->api_key, val, MAX_API_KEY_LEN - 1);
    else if (strcmp(key, "asr_model") == 0) strncpy(c->asr_model, val, MAX_MODEL_NAME_LEN - 1);
    else if (strcmp(key, "llm_model") == 0) strncpy(c->llm_model, val, MAX_MODEL_NAME_LEN - 1);
    else if (strcmp(key, "asr_endpoint") == 0) strncpy(c->asr_endpoint, val, MAX_ENDPOINT_LEN - 1);
    else if (strcmp(key, "chat_endpoint") == 0) strncpy(c->chat_endpoint, val, MAX_ENDPOINT_LEN - 1);
}

int config_store_load(ai_config_t *config)
{
    if (!config) return -1;
    config_set_defaults(config);

    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(line);
        char *val = trim(eq + 1);

        if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) {
            set_bool_field(config, key, true);
        } else if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0) {
            set_bool_field(config, key, false);
        } else {
            set_str_field(config, key, val);
        }
    }
    fclose(f);
    return 0;
}

static void ensure_dir(const char *path)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
}

int config_store_init(void)
{
    ensure_dir(CONFIG_PATH);
    return 0;
}

int config_store_save(const ai_config_t *config)
{
    if (!config) return -1;
    ensure_dir(CONFIG_PATH);
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return -1;

    fprintf(f, "# AI Radio Console Configuration\n");
    fprintf(f, "asr_enabled=%s\n", config->asr_enabled ? "true" : "false");
    fprintf(f, "llm_enabled=%s\n", config->llm_enabled ? "true" : "false");
    fprintf(f, "provider=%d\n", config->provider);
    fprintf(f, "api_key=%s\n", config->api_key);
    fprintf(f, "asr_model=%s\n", config->asr_model);
    fprintf(f, "llm_model=%s\n", config->llm_model);
    fprintf(f, "asr_endpoint=%s\n", config->asr_endpoint);
    fprintf(f, "chat_endpoint=%s\n", config->chat_endpoint);
    fprintf(f, "mayday_detection_via_llm=%s\n", config->mayday_detection_via_llm ? "true" : "false");
    fprintf(f, "violation_detection=%s\n", config->violation_detection ? "true" : "false");
    fprintf(f, "auto_logging=%s\n", config->auto_logging ? "true" : "false");
    fprintf(f, "realtime_translation=%s\n", config->realtime_translation ? "true" : "false");

    fclose(f);
    return 0;
}
