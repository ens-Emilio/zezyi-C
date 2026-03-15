#include <stddef.h>
#include "i18n.h"

static Language s_lang = LANG_EN;

static Language clamp_lang(Language lang)
{
    if ((int)lang < 0 || (int)lang >= (int)LANG_COUNT) return LANG_EN;
    return lang;
}

void i18n_set_language(Language lang)
{
    s_lang = clamp_lang(lang);
}

Language i18n_get_language(void)
{
    return s_lang;
}

const char *i18n_language_label(Language lang)
{
    static const char *LABELS[LANG_COUNT] = {"EN", "PT-BR", "ES"};
    lang = clamp_lang(lang);
    return LABELS[(int)lang];
}

const char *i18n(I18nKey key)
{
    static const char *TBL[LANG_COUNT][I18N_COUNT] = {
        /* LANG_EN */
        {
            [I18N_TITLE]             = "ZEZYI",
            [I18N_LABEL_HOLD]        = "HOLD",
            [I18N_LABEL_SCORE]       = "SCORE",
            [I18N_LABEL_LEVEL]       = "LEVEL",
            [I18N_LABEL_LINES]       = "LINES",
            [I18N_LABEL_TIME]        = "TIME",
            [I18N_LABEL_LEFT]        = "LEFT",
            [I18N_LABEL_NEXT_LV]     = "NEXT LV",
            [I18N_LABEL_COMBO]       = "COMBO",
            [I18N_LABEL_NEXT]        = "NEXT",
            [I18N_LABEL_TSPIN]       = "T-SPIN",
            [I18N_LABEL_ALL_CLEAR]   = "ALL CLEAR!",

            [I18N_MENU_HINT1]        = "UP DOWN SELECT  SPACE START",
            [I18N_MENU_HINT2]        = "Q QUIT  R RESOLUTION  S SETTINGS",
            [I18N_HIGH_SCORES]       = "HIGH SCORES",
            [I18N_PAUSED]            = "PAUSED",
            [I18N_GAME]              = "GAME",
            [I18N_OVER]              = "OVER",
            [I18N_HINT_SPACE_RESUME] = "SPACE RESUME",
            [I18N_HINT_Q_MENU]       = "Q FOR MENU",
            [I18N_HINT_SPACE_RETRY]  = "SPACE TO RETRY",
            [I18N_HINT_SPACE_PLAY]   = "SPACE TO PLAY",
            [I18N_HINT_Q_BACK]       = "Q TO GO BACK",

            [I18N_WELCOME]           = "WELCOME",
            [I18N_ENTER_YOUR_NAME]   = "ENTER YOUR NAME",
            [I18N_NAME_HINT1]        = "A-Z 0-9  BACKSPACE DELETE",
            [I18N_NAME_HINT2]        = "ENTER TO CONFIRM",

            [I18N_SETTINGS]          = "SETTINGS",
            [I18N_SETTINGS_HINT1]    = "UP DOWN NAVIGATE  LEFT RIGHT CHANGE",
            [I18N_SETTINGS_HINT2]    = "Q SAVE AND RETURN TO MENU",
            [I18N_SETTING_RESOLUTION]= "RESOLUTION",
            [I18N_SETTING_GHOST]     = "GHOST",
            [I18N_SETTING_GRID_LINES]= "GRID LINES",
            [I18N_SETTING_SFX_VOL]   = "SFX VOL",
            [I18N_SETTING_BGM_VOL]   = "BGM VOL",
            [I18N_SETTING_DAS_MS]    = "DAS MS",
            [I18N_SETTING_ARR_MS]    = "ARR MS",
            [I18N_SETTING_LANGUAGE]  = "LANGUAGE",
            [I18N_ON]                = "ON",
            [I18N_OFF]               = "OFF",
            [I18N_RES_FULL]          = "FULL",

            [I18N_RESULTS]           = "RESULTS",
            [I18N_NEW_BEST]          = "NEW BEST!",
            [I18N_PIECES]            = "PIECES",
            [I18N_QUAD]              = "4-LINE",
            [I18N_TSPIN_SHORT]       = "TSPIN",
            [I18N_MAX_COMBO]         = "MAX COMBO",
            [I18N_APM]               = "APM",
            [I18N_HINT_RETURN_MENU]  = "SPACE TO RETURN TO MENU",

            [I18N_EDIT_USER]         = "EDIT NAME",
            [I18N_RESET_SCORES]      = "RESET SCORES",
            [I18N_RESET_SETTINGS]    = "RESET SETTINGS",
            [I18N_RESET_ALL]         = "RESET ALL",
            [I18N_CONFIRM]           = "CONFIRM",
            [I18N_CONFIRM_RESET_SCORES]   = "RESET SCORES",
            [I18N_CONFIRM_RESET_SETTINGS] = "RESET SETTINGS",
            [I18N_CONFIRM_RESET_ALL]      = "RESET ALL",
            [I18N_CONFIRM_HINT_OK]        = "SPACE CONFIRM",
            [I18N_CONFIRM_HINT_CANCEL]    = "Q CANCEL",
        },

        /* LANG_PTBR */
        {
            [I18N_TITLE]             = "ZEZYI",
            [I18N_LABEL_HOLD]        = "HOLD",
            [I18N_LABEL_SCORE]       = "PONTOS",
            [I18N_LABEL_LEVEL]       = "NIVEL",
            [I18N_LABEL_LINES]       = "LINHAS",
            [I18N_LABEL_TIME]        = "TEMPO",
            [I18N_LABEL_LEFT]        = "FALTAM",
            [I18N_LABEL_NEXT_LV]     = "PROX NIV",
            [I18N_LABEL_COMBO]       = "COMBO",
            [I18N_LABEL_NEXT]        = "PROX",
            [I18N_LABEL_TSPIN]       = "T-SPIN",
            [I18N_LABEL_ALL_CLEAR]   = "TUDO LIMPO!",

            [I18N_MENU_HINT1]        = "CIMA BAIXO ESCOLHE  ESPACO INICIA",
            [I18N_MENU_HINT2]        = "Q SAIR  R RESOLUCAO  S AJUSTES",
            [I18N_HIGH_SCORES]       = "RECORDES",
            [I18N_PAUSED]            = "PAUSADO",
            [I18N_GAME]              = "FIM",
            [I18N_OVER]              = "JOGO",
            [I18N_HINT_SPACE_RESUME] = "ESPACO CONTINUA",
            [I18N_HINT_Q_MENU]       = "Q MENU",
            [I18N_HINT_SPACE_RETRY]  = "ESPACO REINICIA",
            [I18N_HINT_SPACE_PLAY]   = "ESPACO JOGAR",
            [I18N_HINT_Q_BACK]       = "Q VOLTAR",

            [I18N_WELCOME]           = "BEM VINDO",
            [I18N_ENTER_YOUR_NAME]   = "DIGITE SEU NOME",
            [I18N_NAME_HINT1]        = "A-Z 0-9  BACKSPACE APAGA",
            [I18N_NAME_HINT2]        = "ENTER CONFIRMA",

            [I18N_SETTINGS]          = "AJUSTES",
            [I18N_SETTINGS_HINT1]    = "CIMA BAIXO NAVEGA  ESQ DIR MUDA",
            [I18N_SETTINGS_HINT2]    = "Q SALVA E MENU",
            [I18N_SETTING_RESOLUTION]= "RESOLUCAO",
            [I18N_SETTING_GHOST]     = "FANTASMA",
            [I18N_SETTING_GRID_LINES]= "GRADE",
            [I18N_SETTING_SFX_VOL]   = "VOL SFX",
            [I18N_SETTING_BGM_VOL]   = "VOL BGM",
            [I18N_SETTING_DAS_MS]    = "DAS MS",
            [I18N_SETTING_ARR_MS]    = "ARR MS",
            [I18N_SETTING_LANGUAGE]  = "IDIOMA",
            [I18N_ON]                = "SIM",
            [I18N_OFF]               = "NAO",
            [I18N_RES_FULL]          = "TELA CHEIA",

            [I18N_RESULTS]           = "RESULTADOS",
            [I18N_NEW_BEST]          = "NOVO RECORDE!",
            [I18N_PIECES]            = "PECAS",
            [I18N_QUAD]              = "4-LINHAS",
            [I18N_TSPIN_SHORT]       = "TSPIN",
            [I18N_MAX_COMBO]         = "COMBO MAX",
            [I18N_APM]               = "APM",
            [I18N_HINT_RETURN_MENU]  = "ESPACO MENU",

            [I18N_EDIT_USER]         = "EDITAR NOME",
            [I18N_RESET_SCORES]      = "RESET RECORDES",
            [I18N_RESET_SETTINGS]    = "RESET AJUSTES",
            [I18N_RESET_ALL]         = "RESET TUDO",
            [I18N_CONFIRM]           = "CONFIRMA",
            [I18N_CONFIRM_RESET_SCORES]   = "RESET RECORDES",
            [I18N_CONFIRM_RESET_SETTINGS] = "RESET AJUSTES",
            [I18N_CONFIRM_RESET_ALL]      = "RESET TUDO",
            [I18N_CONFIRM_HINT_OK]        = "ESPACO CONFIRMA",
            [I18N_CONFIRM_HINT_CANCEL]    = "Q CANCELA",
        },

        /* LANG_ES */
        {
            [I18N_TITLE]             = "ZEZYI",
            [I18N_LABEL_HOLD]        = "HOLD",
            [I18N_LABEL_SCORE]       = "PUNTOS",
            [I18N_LABEL_LEVEL]       = "NIVEL",
            [I18N_LABEL_LINES]       = "LINEAS",
            [I18N_LABEL_TIME]        = "TIEMPO",
            [I18N_LABEL_LEFT]        = "FALTAN",
            [I18N_LABEL_NEXT_LV]     = "SIG NIV",
            [I18N_LABEL_COMBO]       = "COMBO",
            [I18N_LABEL_NEXT]        = "SIG",
            [I18N_LABEL_TSPIN]       = "T-SPIN",
            [I18N_LABEL_ALL_CLEAR]   = "TODO LIMPIO!",

            [I18N_MENU_HINT1]        = "ARRIBA ABAJO ELIGE  ESPACIO INICIA",
            [I18N_MENU_HINT2]        = "Q SALIR  R RESOLUCION  S AJUSTES",
            [I18N_HIGH_SCORES]       = "RECORDS",
            [I18N_PAUSED]            = "PAUSA",
            [I18N_GAME]              = "FIN",
            [I18N_OVER]              = "JUEGO",
            [I18N_HINT_SPACE_RESUME] = "ESPACIO SIGUE",
            [I18N_HINT_Q_MENU]       = "Q MENU",
            [I18N_HINT_SPACE_RETRY]  = "ESPACIO REINTENTA",
            [I18N_HINT_SPACE_PLAY]   = "ESPACIO JUGAR",
            [I18N_HINT_Q_BACK]       = "Q VOLVER",

            [I18N_WELCOME]           = "BIENVENIDO",
            [I18N_ENTER_YOUR_NAME]   = "ESCRIBE TU NOMBRE",
            [I18N_NAME_HINT1]        = "A-Z 0-9  BACKSPACE BORRA",
            [I18N_NAME_HINT2]        = "ENTER CONFIRMA",

            [I18N_SETTINGS]          = "AJUSTES",
            [I18N_SETTINGS_HINT1]    = "ARRIBA ABAJO NAVEGA  IZQ DER CAMBIA",
            [I18N_SETTINGS_HINT2]    = "Q GUARDA Y MENU",
            [I18N_SETTING_RESOLUTION]= "RESOLUCION",
            [I18N_SETTING_GHOST]     = "FANTASMA",
            [I18N_SETTING_GRID_LINES]= "REJILLA",
            [I18N_SETTING_SFX_VOL]   = "VOL SFX",
            [I18N_SETTING_BGM_VOL]   = "VOL BGM",
            [I18N_SETTING_DAS_MS]    = "DAS MS",
            [I18N_SETTING_ARR_MS]    = "ARR MS",
            [I18N_SETTING_LANGUAGE]  = "IDIOMA",
            [I18N_ON]                = "SI",
            [I18N_OFF]               = "NO",
            [I18N_RES_FULL]          = "PANTALLA",

            [I18N_RESULTS]           = "RESULTADOS",
            [I18N_NEW_BEST]          = "NUEVO RECORD!",
            [I18N_PIECES]            = "PIEZAS",
            [I18N_QUAD]              = "4-LINEAS",
            [I18N_TSPIN_SHORT]       = "TSPIN",
            [I18N_MAX_COMBO]         = "COMBO MAX",
            [I18N_APM]               = "APM",
            [I18N_HINT_RETURN_MENU]  = "ESPACIO MENU",

            [I18N_EDIT_USER]         = "EDITAR NOMBRE",
            [I18N_RESET_SCORES]      = "RESET RECORDS",
            [I18N_RESET_SETTINGS]    = "RESET AJUSTES",
            [I18N_RESET_ALL]         = "RESET TODO",
            [I18N_CONFIRM]           = "CONFIRMA",
            [I18N_CONFIRM_RESET_SCORES]   = "RESET RECORDS",
            [I18N_CONFIRM_RESET_SETTINGS] = "RESET AJUSTES",
            [I18N_CONFIRM_RESET_ALL]      = "RESET TODO",
            [I18N_CONFIRM_HINT_OK]        = "ESPACIO CONFIRMA",
            [I18N_CONFIRM_HINT_CANCEL]    = "Q CANCELA",
        }
    };

    Language lang = clamp_lang(s_lang);
    if ((int)key < 0 || (int)key >= (int)I18N_COUNT) key = I18N_TITLE;
    const char *s = TBL[(int)lang][(int)key];
    if (!s) s = TBL[(int)LANG_EN][(int)key];
    return s ? s : "";
}

const char *i18n_tip(TipId tip)
{
    static const char *TIPS[LANG_COUNT][TIP_COUNT] = {
        /* LANG_EN */
        {
            [TIP_NONE]        = "",
            [TIP_TRY_HOLD]    = "TIP - PRESS C TO SAVE A PIECE - HOLD!",
            [TIP_DANGER]      = "DANGER - STACK TOO HIGH - USE HOLD!",
            [TIP_COMBO_FIRE]  = "COMBO ON FIRE - KEEP IT GOING!",
            [TIP_FIRST_QUAD]  = "4-LINE - 4 LINES AT ONCE - AMAZING!",
        },
        /* LANG_PTBR */
        {
            [TIP_NONE]        = "",
            [TIP_TRY_HOLD]    = "DICA - APERTE C PARA HOLD!",
            [TIP_DANGER]      = "PERIGO - PILHA ALTA - USE HOLD!",
            [TIP_COMBO_FIRE]  = "COMBO PEGANDO FOGO - CONTINUA!",
            [TIP_FIRST_QUAD]  = "4-LINHAS - 4 LINHAS - INCRIVEL!",
        },
        /* LANG_ES */
        {
            [TIP_NONE]        = "",
            [TIP_TRY_HOLD]    = "TIP - PULSA C PARA HOLD!",
            [TIP_DANGER]      = "PELIGRO - PILA ALTA - USA HOLD!",
            [TIP_COMBO_FIRE]  = "COMBO EN FUEGO - SIGUE!",
            [TIP_FIRST_QUAD]  = "4-LINEAS - 4 LINEAS - INCREIBLE!",
        }
    };

    if ((int)tip < 0 || (int)tip >= (int)TIP_COUNT) tip = TIP_NONE;
    Language lang = clamp_lang(s_lang);
    const char *s = TIPS[(int)lang][(int)tip];
    if (!s) s = TIPS[(int)LANG_EN][(int)tip];
    return s ? s : "";
}

const char *i18n_mode_name(GameMode mode)
{
    static const char *NAMES[LANG_COUNT][MODE_COUNT] = {
        /* LANG_EN */
        {"MARATHON","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CHEESE"},
        /* LANG_PTBR */
        {"MARATONA","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CHEESE"},
        /* LANG_ES */
        {"MARATON","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CHEESE"},
    };

    if ((int)mode < 0 || (int)mode >= (int)MODE_COUNT) return "-";
    Language lang = clamp_lang(s_lang);
    const char *s = NAMES[(int)lang][(int)mode];
    if (!s) s = NAMES[(int)LANG_EN][(int)mode];
    return s ? s : "-";
}

const char *i18n_mode_info_title(GameMode mode)
{
    static const char *TITLES[LANG_COUNT][MODE_COUNT] = {
        /* LANG_EN */
        {"MARATHON","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CHEESE RACE"},
        /* LANG_PTBR */
        {"MARATONA","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CORRIDA CHEESE"},
        /* LANG_ES */
        {"MARATON","SPRINT","ULTRA","BLITZ","ZEN","SURVIVAL","CARRERA CHEESE"},
    };

    if ((int)mode < 0 || (int)mode >= (int)MODE_COUNT) return "-";
    Language lang = clamp_lang(s_lang);
    const char *s = TITLES[(int)lang][(int)mode];
    if (!s) s = TITLES[(int)LANG_EN][(int)mode];
    return s ? s : "-";
}

const char *i18n_mode_info_line(GameMode mode, int idx)
{
    static const char *LINES[LANG_COUNT][MODE_COUNT][4] = {
        /* LANG_EN */
        {
            {"CLEAR LINES AND ADVANCE LEVELS.",
             "SPEED INCREASES EVERY 10 LINES.", NULL, NULL},
            {"CLEAR 40 LINES AS FAST AS YOU CAN.",
             "EVERY SECOND COUNTS.", NULL, NULL},
            {"120 SECONDS ON THE CLOCK.",
             "SCORE AS HIGH AS POSSIBLE BEFORE TIME.", NULL, NULL},
            {"40 SECONDS - PURE SPEED.",
             "CHAIN COMBOS FOR MASSIVE POINTS.", NULL, NULL},
            {"NO GAME OVER. NO SCORE.",
             "BOARD RESETS WHEN FULL.",
             "JUST FLOW.", NULL},
            {"SPEED RAMPS UP EVERY 3 LINES.",
             "NO LEVEL CAP - HOW LONG CAN YOU LAST.", NULL, NULL},
            {"10 GARBAGE ROWS WITH HOLES.",
             "CLEAR THEM ALL TO WIN.", NULL, NULL},
        },
        /* LANG_PTBR */
        {
            {"LIMPE LINHAS E SUBA DE NIVEL.",
             "VELOCIDADE SOBE A CADA 10 LINHAS.", NULL, NULL},
            {"LIMPE 40 LINHAS O MAIS RAPIDO.",
             "CADA SEGUNDO CONTA.", NULL, NULL},
            {"120 SEGUNDOS NO RELOGIO.",
             "FACA O MAXIMO DE PONTOS NO TEMPO.", NULL, NULL},
            {"40 SEGUNDOS - PURA VELOCIDADE.",
             "EMENDE COMBOS PARA MUITOS PONTOS.", NULL, NULL},
            {"SEM GAME OVER. SEM PONTOS.",
             "TABULEIRO RESETA QUANDO CHEIO.",
             "APENAS FLUA.", NULL},
            {"VELOCIDADE SOBE A CADA 3 LINHAS.",
             "SEM LIMITE DE NIVEL - ATE QUANDO VOCE AGUENTA.", NULL, NULL},
            {"10 LINHAS LIXO COM BURACOS.",
             "LIMPE TODAS PARA VENCER.", NULL, NULL},
        },
        /* LANG_ES */
        {
            {"LIMPIA LINEAS Y SUBE NIVEL.",
             "VELOCIDAD SUBE CADA 10 LINEAS.", NULL, NULL},
            {"LIMPIA 40 LINEAS LO MAS RAPIDO.",
             "CADA SEGUNDO CUENTA.", NULL, NULL},
            {"120 SEGUNDOS EN RELOJ.",
             "HAZ EL MAXIMO DE PUNTOS A TIEMPO.", NULL, NULL},
            {"40 SEGUNDOS - PURA VELOCIDAD.",
             "ENCADENA COMBOS PARA MUCHOS PUNTOS.", NULL, NULL},
            {"SIN GAME OVER. SIN PUNTOS.",
             "TABLERO RESETEA CUANDO LLENO.",
             "SOLO FLUYE.", NULL},
            {"VELOCIDAD SUBE CADA 3 LINEAS.",
             "SIN LIMITE DE NIVEL - CUANTO DURAS.", NULL, NULL},
            {"10 LINEAS BASURA CON HUECOS.",
             "LIMPIA TODAS PARA GANAR.", NULL, NULL},
        }
    };

    if ((int)mode < 0 || (int)mode >= (int)MODE_COUNT) return NULL;
    if (idx < 0 || idx >= 4) return NULL;
    Language lang = clamp_lang(s_lang);
    const char *s = LINES[(int)lang][(int)mode][idx];
    if (!s) s = LINES[(int)LANG_EN][(int)mode][idx];
    return s;
}

const char *i18n_mode_info_objective(GameMode mode)
{
    static const char *OBJ[LANG_COUNT][MODE_COUNT] = {
        /* LANG_EN */
        {
            "SURVIVE AS LONG AS POSSIBLE!",
            "AIM FOR UNDER 1 MINUTE!",
            "MAXIMIZE 4-LINES AND COMBOS!",
            "MAXIMIZE YOUR SCORE IN 40 SECONDS!",
            "PRACTICE TECHNIQUES AND RELAX.",
            "SURVIVE THE EVER-INCREASING SPEED!",
            "FASTEST TIME WINS!",
        },
        /* LANG_PTBR */
        {
            "SOBREVIVA O MAXIMO POSSIVEL!",
            "MIRE MENOS DE 1 MINUTO!",
            "MAXIMIZE 4-LINHAS E COMBOS!",
            "MAXIMIZE PONTOS EM 40 SEGUNDOS!",
            "PRATIQUE E RELAXE.",
            "SOBREVIVA A VELOCIDADE CRESCENTE!",
            "MENOR TEMPO VENCE!",
        },
        /* LANG_ES */
        {
            "SOBREVIVE LO MAS POSIBLE!",
            "APUNTA MENOS DE 1 MINUTO!",
            "MAXIMIZA 4-LINEAS Y COMBOS!",
            "MAXIMIZA PUNTOS EN 40 SEGUNDOS!",
            "PRACTICA Y RELAJA.",
            "SOBREVIVE A LA VELOCIDAD CRECIENTE!",
            "MENOR TIEMPO GANA!",
        }
    };

    if ((int)mode < 0 || (int)mode >= (int)MODE_COUNT) return "";
    Language lang = clamp_lang(s_lang);
    const char *s = OBJ[(int)lang][(int)mode];
    if (!s) s = OBJ[(int)LANG_EN][(int)mode];
    return s ? s : "";
}

const char *i18n_mode_info_tip(GameMode mode, int idx)
{
    static const char *TIPS[LANG_COUNT][MODE_COUNT][2] = {
        /* LANG_EN */
        {
            {"4-LINE - 4 LINES AT ONCE - SCORES 4X.",
             "BACK-TO-BACK 4-LINE GIVES 1.5X BONUS."},
            {"HARD DROP - SPACE - IS ESSENTIAL HERE.",
             "SAVE THE I PIECE WITH HOLD."},
            {"4-LINES SCORE MUCH MORE THAN SINGLES.",
             "WATCH THE TIMER - SPEED UP NEAR THE END."},
            {"COMBOS GIVE 50 X COMBO X LEVEL BONUS.",
             "T-SPIN DOUBLE BEATS 4-LINE PER SECOND."},
            {"PERFECT FOR TRAINING T-SPIN SETUPS.",
             "TRY NEW STACKING PATTERNS WITH NO RISK."},
            {"PLAN AHEAD - SPEED BECOMES BRUTAL.",
             "HOLD IS YOUR BEST FRIEND HERE."},
            {"I AND L PIECES REACH THE HOLES BEST.",
             "KEEP YOUR STACK LOW AT ALL TIMES."},
        },
        /* LANG_PTBR */
        {
            {"4-LINHAS - 4 LINHAS - VALE 4X.",
             "B2B 4-LINHAS DA BONUS 1.5X."},
            {"HARD DROP - ESPACO - E ESSENCIAL.",
             "GUARDE O I COM HOLD."},
            {"4-LINHAS VALEM MUITO MAIS QUE 1 LINHA.",
             "OLHO NO TEMPO - ACELERE NO FINAL."},
            {"COMBO DA 50 X COMBO X NIVEL.",
             "T-SPIN DOUBLE BATE 4-LINHAS POR SEGUNDO."},
            {"PERFEITO PARA TREINAR T-SPIN.",
             "TENTE NOVAS PILHAS SEM RISCO."},
            {"PLANEJE - A VELOCIDADE FICA BRUTAL.",
             "HOLD E SEU MELHOR AMIGO."},
            {"PECAS I E L ALCANCAM OS BURACOS.",
             "MANTENHA A PILHA BAIXA SEMPRE."},
        },
        /* LANG_ES */
        {
            {"4-LINEAS - 4 LINEAS - VALE 4X.",
             "B2B 4-LINEAS DA BONUS 1.5X."},
            {"HARD DROP - ESPACIO - ES CLAVE.",
             "GUARDA LA I CON HOLD."},
            {"4-LINEAS VALEN MAS QUE 1 LINEA.",
             "MIRA EL TIEMPO - ACELERA AL FINAL."},
            {"COMBO DA 50 X COMBO X NIVEL.",
             "T-SPIN DOUBLE GANA A 4-LINEA POR SEGUNDO."},
            {"PERFECTO PARA ENTRENAR T-SPIN.",
             "PRUEBA NUEVAS PILAS SIN RIESGO."},
            {"PLANEA - VELOCIDAD SE PONE BRUTAL.",
             "HOLD ES TU MEJOR AMIGO."},
            {"PIEZAS I Y L LLEGAN A HUECOS.",
             "MANTEN LA PILA BAJA SIEMPRE."},
        }
    };

    if ((int)mode < 0 || (int)mode >= (int)MODE_COUNT) return "";
    if (idx < 0 || idx > 1) return "";
    Language lang = clamp_lang(s_lang);
    const char *s = TIPS[(int)lang][(int)mode][idx];
    if (!s) s = TIPS[(int)LANG_EN][(int)mode][idx];
    return s ? s : "";
}
