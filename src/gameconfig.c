#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "gameconfig.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

static void em_cfg_restore(const char *path)
{
    EM_ASM({
        var path = UTF8ToString($0);
        try {
            var data = localStorage.getItem('zezyi_cfg:' + path);
            if (data) { FS.writeFile(path, data); }
        } catch(e) {}
    }, path);
}

static void em_cfg_persist(const char *path)
{
    EM_ASM({
        var path = UTF8ToString($0);
        try {
            var data = FS.readFile(path, {encoding: 'utf8'});
            localStorage.setItem('zezyi_cfg:' + path, data);
        } catch(e) {}
    }, path);
}
#endif

static int streq_ci(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static Language parse_language_str(const char *s, Language fallback)
{
    if (!s || !s[0]) return fallback;
    if (streq_ci(s, "en") || streq_ci(s, "eng") || streq_ci(s, "english"))
        return LANG_EN;
    if (streq_ci(s, "ptbr") || streq_ci(s, "pt-br") || streq_ci(s, "pt_br") ||
        streq_ci(s, "pt")   || streq_ci(s, "portuguese"))
        return LANG_PTBR;
    if (streq_ci(s, "es") || streq_ci(s, "spa") || streq_ci(s, "spanish"))
        return LANG_ES;
    return fallback;
}

static const char *language_to_str(Language lang)
{
    switch (lang) {
        case LANG_PTBR: return "ptbr";
        case LANG_ES:   return "es";
        case LANG_EN:
        default:        return "en";
    }
}
void gameconfig_defaults(GameConfig *cfg)
{
    cfg->das_ms      = 140;
    cfg->arr_ms      = 30;
    cfg->ghost       = 1;
    cfg->grid_lines  = 0;
    cfg->sfx_volume  = 100;
    cfg->bgm_volume  = 70;
    cfg->resolution  = 1; /* RES_720P */
    cfg->language    = LANG_EN;
    cfg->player_name[0] = '\0';
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void gameconfig_clamp(GameConfig *cfg)
{
    cfg->das_ms     = clamp_int(cfg->das_ms, 0, 500);
    cfg->arr_ms     = clamp_int(cfg->arr_ms, 0, 100);
    cfg->ghost      = clamp_int(cfg->ghost, 0, 1);
    cfg->grid_lines = clamp_int(cfg->grid_lines, 0, 1);
    cfg->sfx_volume = clamp_int(cfg->sfx_volume, 0, 128);
    cfg->bgm_volume = clamp_int(cfg->bgm_volume, 0, 128);
    cfg->resolution = clamp_int(cfg->resolution, 0, (int)RES_COUNT - 1);
    if (cfg->language < 0 || cfg->language >= LANG_COUNT)
        cfg->language = LANG_EN;
}

/*
 * Parse a minimal INI-style config file.
 * Accepted format per line:  key = value   or   key = string value
 * Lines starting with '#', ';', or '[' are ignored.
 * Unknown keys are silently skipped.
 */
void gameconfig_load(GameConfig *cfg, const char *path)
{
    gameconfig_defaults(cfg);

#ifdef __EMSCRIPTEN__
    em_cfg_restore(path);
#endif

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, (int)sizeof(line), f)) {
        char key[64];
        char strval[64];
        int  val;

        /* Skip blank lines and comments */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '[' ||
            *p == '\n' || *p == '\0') continue;

        /* Trim trailing newline / comment */
        char *hash = strpbrk(p, "#;\n\r");
        if (hash) *hash = '\0';

        /* Try integer key=value */
        if (sscanf(p, " %63[^=] = %d", key, &val) == 2) {
            int klen = (int)strlen(key);
            while (klen > 0 && (key[klen-1]==' '||key[klen-1]=='\t')) klen--;
            key[klen] = '\0';

            if      (strcmp(key, "das_ms")     == 0) cfg->das_ms     = val;
            else if (strcmp(key, "arr_ms")     == 0) cfg->arr_ms     = val;
            else if (strcmp(key, "ghost")      == 0) cfg->ghost      = val;
            else if (strcmp(key, "grid_lines") == 0) cfg->grid_lines = val;
            else if (strcmp(key, "sfx_volume") == 0) cfg->sfx_volume = val;
            else if (strcmp(key, "bgm_volume") == 0) cfg->bgm_volume = val;
            else if (strcmp(key, "resolution") == 0) cfg->resolution = val;
            else if (strcmp(key, "language")   == 0) {
                if (val < 0) val = 0;
                if (val >= (int)LANG_COUNT) val = (int)LANG_EN;
                cfg->language = (Language)val;
            }
            continue;
        }

        /* Try string key=value (player_name, language) — allow spaces in value after '=' */
        if (sscanf(p, " %63[^=] = %63[^\n]", key, strval) == 2) {
            int klen = (int)strlen(key);
            while (klen > 0 && (key[klen-1]==' '||key[klen-1]=='\t')) klen--;
            key[klen] = '\0';
            /* trim leading spaces from strval */
            char *sv = strval;
            while (*sv == ' ' || *sv == '\t') sv++;
            /* trim trailing spaces */
            int slen = (int)strlen(sv);
            while (slen > 0 && (sv[slen-1]==' '||sv[slen-1]=='\t')) slen--;
            sv[slen] = '\0';
            /* cut at first space if value should be single token (player_name) */
            char *sp = strchr(sv, ' ');
            if (sp) *sp = '\0';
            sp = strchr(sv, '\t');
            if (sp) *sp = '\0';

            if (strcmp(key, "player_name") == 0) {
                strncpy(cfg->player_name, sv,
                        sizeof(cfg->player_name) - 1);
                cfg->player_name[sizeof(cfg->player_name) - 1] = '\0';
            } else if (strcmp(key, "language") == 0) {
                cfg->language = parse_language_str(sv, cfg->language);
            }
        }
    }
    fclose(f);
    gameconfig_clamp(cfg);
}

int gameconfig_save(const GameConfig *cfg, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "# zezyi runtime config\n");
    fprintf(f, "# Lines starting with # are ignored.\n\n");
    fprintf(f, "[player]\n");
    fprintf(f, "player_name = %s\n\n",
            cfg->player_name[0] ? cfg->player_name : "PLAYER");
    fprintf(f, "[ui]\n");
    fprintf(f, "language   = %s\n\n", language_to_str(cfg->language));
    fprintf(f, "[input]\n");
    fprintf(f, "das_ms     = %d\n", cfg->das_ms);
    fprintf(f, "arr_ms     = %d\n\n", cfg->arr_ms);
    fprintf(f, "[display]\n");
    fprintf(f, "ghost      = %d\n", cfg->ghost);
    fprintf(f, "grid_lines = %d\n", cfg->grid_lines);
    fprintf(f, "resolution = %d\n\n", cfg->resolution);
    fprintf(f, "[audio]\n");
    fprintf(f, "sfx_volume = %d\n", cfg->sfx_volume);
    fprintf(f, "bgm_volume = %d\n", cfg->bgm_volume);

    fclose(f);
#ifdef __EMSCRIPTEN__
    em_cfg_persist(path);
#endif
    return 1;
}
