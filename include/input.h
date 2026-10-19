#pragma once
#include "config.h"

void        input_set_timing(int das_ms, int arr_ms);
void        input_poll(InputSnapshot *snap);

/* Text-input mode — call before entering APP_NAME_ENTRY */
void        input_start_text(const char *initial);
void        input_stop_text(void);
const char *input_text_get(void);

#ifdef ZEZYI_HEADLESS
/* In headless (test) builds, inject a raw key-state array bypassing SDL. */
void        input_set_mock_state(const uint8_t *keys, int nkeys);
void        input_clear_mock_state(void);
#endif
