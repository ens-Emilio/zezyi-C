#pragma once
#include "config.h"

void        input_set_timing(int das_ms, int arr_ms);
void        input_poll(InputSnapshot *snap);

/* Text-input mode — call before entering APP_NAME_ENTRY */
void        input_start_text(const char *initial);
void        input_stop_text(void);
const char *input_text_get(void);
