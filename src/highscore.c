#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "highscore.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/* Restore a highscore file from localStorage into MEMFS before reading. */
static void em_hs_restore(const char *path)
{
    EM_ASM({
        var path = UTF8ToString($0);
        try {
            var data = localStorage.getItem('zezyi_hs:' + path);
            if (data) { FS.writeFile(path, data); }
        } catch(e) {}
    }, path);
}

/* Persist a highscore file from MEMFS into localStorage after writing. */
static void em_hs_persist(const char *path)
{
    EM_ASM({
        var path = UTF8ToString($0);
        try {
            var data = FS.readFile(path, {encoding: 'utf8'});
            localStorage.setItem('zezyi_hs:' + path, data);
        } catch(e) {}
    }, path);
}
#endif

#define HS_FILE_LEGACY "zezyi_hs.txt"

static void hs_sanitize_name(const char *in, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (!in || !in[0]) {
        snprintf(out, out_sz, "%s", "PLAYER");
        return;
    }

    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            out[j++] = (char)c;
        } else if (c >= 'a' && c <= 'z') {
            out[j++] = (char)toupper(c);
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    if (j == 0) snprintf(out, out_sz, "%s", "PLAYER");
}

static void hs_path_for(const GameState *g, char *out, size_t out_sz)
{
    char safe[32];
    hs_sanitize_name(g ? g->player_name : NULL, safe, sizeof(safe));
    snprintf(out, out_sz, "zezyi_hs_%s.txt", safe);
}

void hs_load(GameState *g)
{
    g->hs_marathon  = 0;
    g->hs_sprint_ms = 0;
    g->hs_ultra     = 0;
    g->hs_blitz     = 0;
    g->hs_survival  = 0;
    g->hs_cheese_ms = 0;

    char path[64];
    hs_path_for(g, path, sizeof(path));

#ifdef __EMSCRIPTEN__
    em_hs_restore(path);
#endif

    FILE *f = fopen(path, "r");
    if (!f) {
        /* Backward compat: try legacy file */
#ifdef __EMSCRIPTEN__
        em_hs_restore(HS_FILE_LEGACY);
#endif
        f = fopen(HS_FILE_LEGACY, "r");
        if (!f) return;
    }

    char line[64];
    while (fgets(line, (int)sizeof(line), f)) {
        char key[32];
        int  val;
        if (sscanf(line, " %31[^=] = %d", key, &val) != 2) continue;
        /* Trim trailing whitespace from key */
        int klen = (int)strlen(key);
        while (klen > 0 && (key[klen-1]==' '||key[klen-1]=='\t')) klen--;
        key[klen] = '\0';

        if      (strcmp(key, "marathon")  == 0) g->hs_marathon  = val;
        else if (strcmp(key, "sprint")    == 0) g->hs_sprint_ms = val;
        else if (strcmp(key, "ultra")     == 0) g->hs_ultra     = val;
        else if (strcmp(key, "blitz")     == 0) g->hs_blitz     = val;
        else if (strcmp(key, "survival")  == 0) g->hs_survival  = val;
        else if (strcmp(key, "cheese")    == 0) g->hs_cheese_ms = val;
    }

    fclose(f);
}

void hs_save(const GameState *g)
{
    char path[64];
    hs_path_for(g, path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "marathon  = %d\n", g->hs_marathon);
    fprintf(f, "sprint    = %d\n", g->hs_sprint_ms);
    fprintf(f, "ultra     = %d\n", g->hs_ultra);
    fprintf(f, "blitz     = %d\n", g->hs_blitz);
    fprintf(f, "survival  = %d\n", g->hs_survival);
    fprintf(f, "cheese    = %d\n", g->hs_cheese_ms);
    fclose(f);
#ifdef __EMSCRIPTEN__
    em_hs_persist(path);
#endif
}

int hs_update(GameState *g)
{
    int improved = 0;
    int ms;

    switch (g->mode) {
        case MODE_MARATHON:
            if (g->score > g->hs_marathon) {
                g->hs_marathon = g->score;
                improved = 1;
            }
            break;

        case MODE_SPRINT:
            ms = (int)g->time_elapsed_ms;
            if (ms > 0 && (g->hs_sprint_ms == 0 || ms < g->hs_sprint_ms)) {
                g->hs_sprint_ms = ms;
                improved = 1;
            }
            break;

        case MODE_ULTRA:
            if (g->score > g->hs_ultra) {
                g->hs_ultra = g->score;
                improved = 1;
            }
            break;

        case MODE_BLITZ:
            if (g->score > g->hs_blitz) {
                g->hs_blitz = g->score;
                improved = 1;
            }
            break;

        case MODE_SURVIVAL:
            if (g->score > g->hs_survival) {
                g->hs_survival = g->score;
                improved = 1;
            }
            break;

        case MODE_CHEESE:
            ms = (int)g->time_elapsed_ms;
            if (ms > 0 && (g->hs_cheese_ms == 0 || ms < g->hs_cheese_ms)) {
                g->hs_cheese_ms = ms;
                improved = 1;
            }
            break;

        default:
            break;
    }

    return improved;
}
