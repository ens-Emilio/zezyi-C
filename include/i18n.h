#pragma once
#include "config.h"

typedef enum {
    I18N_TITLE = 0,

    /* Generic labels */
    I18N_LABEL_HOLD,
    I18N_LABEL_SCORE,
    I18N_LABEL_LEVEL,
    I18N_LABEL_LINES,
    I18N_LABEL_TIME,
    I18N_LABEL_LEFT,
    I18N_LABEL_NEXT_LV,
    I18N_LABEL_COMBO,
    I18N_LABEL_NEXT,
    I18N_LABEL_TSPIN,
    I18N_LABEL_ALL_CLEAR,

    /* Menu + overlays */
    I18N_MENU_HINT1,
    I18N_MENU_HINT2,
    I18N_HIGH_SCORES,
    I18N_PAUSED,
    I18N_GAME,
    I18N_OVER,
    I18N_HINT_SPACE_RESUME,
    I18N_HINT_Q_MENU,
    I18N_HINT_SPACE_RETRY,
    I18N_HINT_SPACE_PLAY,
    I18N_HINT_Q_BACK,

    /* Name entry */
    I18N_WELCOME,
    I18N_ENTER_YOUR_NAME,
    I18N_NAME_HINT1,
    I18N_NAME_HINT2,

    /* Settings */
    I18N_SETTINGS,
    I18N_SETTINGS_HINT1,
    I18N_SETTINGS_HINT2,
    I18N_SETTING_RESOLUTION,
    I18N_SETTING_GHOST,
    I18N_SETTING_GRID_LINES,
    I18N_SETTING_SFX_VOL,
    I18N_SETTING_BGM_VOL,
    I18N_SETTING_DAS_MS,
    I18N_SETTING_ARR_MS,
    I18N_SETTING_LANGUAGE,
    I18N_ON,
    I18N_OFF,
    I18N_RES_FULL,

    /* Results */
    I18N_RESULTS,
    I18N_NEW_BEST,
    I18N_PIECES,
    I18N_QUAD,
    I18N_TSPIN_SHORT,
    I18N_MAX_COMBO,
    I18N_APM,
    I18N_HINT_RETURN_MENU,

    /* Data / user actions */
    I18N_EDIT_USER,
    I18N_RESET_SCORES,
    I18N_RESET_SETTINGS,
    I18N_RESET_ALL,
    I18N_CONFIRM,
    I18N_CONFIRM_RESET_SCORES,
    I18N_CONFIRM_RESET_SETTINGS,
    I18N_CONFIRM_RESET_ALL,
    I18N_CONFIRM_HINT_OK,
    I18N_CONFIRM_HINT_CANCEL,

    I18N_COUNT
} I18nKey;

void     i18n_set_language(Language lang);
Language i18n_get_language(void);
const char *i18n(I18nKey key);

const char *i18n_language_label(Language lang);

const char *i18n_mode_name(GameMode mode);              /* menu/results */
const char *i18n_mode_info_title(GameMode mode);        /* mode info */
const char *i18n_mode_info_line(GameMode mode, int idx);
const char *i18n_mode_info_objective(GameMode mode);
const char *i18n_mode_info_tip(GameMode mode, int idx); /* 0..1 */

const char *i18n_tip(TipId tip);
