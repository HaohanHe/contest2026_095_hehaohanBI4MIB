#ifndef DAILY_SUMMARY_H
#define DAILY_SUMMARY_H

#include <stdbool.h>

int  daily_summary_generate(void);
const char *daily_summary_get(void);
bool daily_summary_is_ready(void);

#endif
