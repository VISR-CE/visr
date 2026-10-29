/* Closed staging grammar for signed cases and development recordings. Header-only so the host, guest,
   and portable probes use the same parser without adding an engine dependency to the host. */
#ifndef HALO_BENCHMARK_STAGE_H
#define HALO_BENCHMARK_STAGE_H
#include <stdio.h>
#include <string.h>

#define BENCHMARK_STAGE_MAXIMUM 16
#define BENCHMARK_STAGE_JSON_SIZE 1536
enum benchmark_stage_kind { BENCHMARK_STAGE_SWITCH_BSP, BENCHMARK_STAGE_TELEPORT_PLAYER,
    BENCHMARK_STAGE_AI_PLACE, BENCHMARK_STAGE_AI_SEE, BENCHMARK_STAGE_AI_SEE_PLAYER, BENCHMARK_STAGE_DEATHLESS };
struct benchmark_stage_step { int kind, bsp; char a[32], b[32]; };
struct benchmark_stage {
    int count;
    struct benchmark_stage_step steps[BENCHMARK_STAGE_MAXIMUM];
    char json[BENCHMARK_STAGE_JSON_SIZE];
};

static inline int benchmark_stage_name(const char **cursor, char name[32]) {
    const char *at = *cursor;
    int count = 0;
    if (*at++ != '"') return 0;
    while ((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_') {
        if (count == 31) return 0;
        name[count++] = *at++;
    }
    if (!count || *at++ != '"') return 0;
    name[count] = 0;
    *cursor = at;
    return 1;
}

/* Only positive 32-bit ticks cross the host/guest boundary. */
static inline int benchmark_stage_tick(const char *text, long *tick) {
    unsigned long value = 0;
    if (*text < '1' || *text > '9') return 0;
    do {
        unsigned digit = (unsigned)(*text++ - '0');
        if (value > (2147483647UL - digit) / 10) return 0;
        value = value * 10 + digit;
    } while (*text >= '0' && *text <= '9');
    if (*text) return 0;
    *tick = (long)value;
    return 1;
}

static inline int benchmark_stage_parse(const char *text, struct benchmark_stage *stage) {
    static const char *const keys[] = { "switch_bsp", "teleport_player", "ai_place", "ai_see", "ai_see_player", "deathless" };
    const char *at;
    size_t length = 0;
    int quoted = 0;
    memset(stage, 0, sizeof(*stage));
    /* These strings contain only names, so escapes are outside the grammar. */
    for (; *text; text++) {
        if (*text == '"') quoted = !quoted;
        if (!quoted && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
            const char *next = text;
            while (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\n') next++;
            /* Whitespace may separate JSON tokens, never split a number or a literal. */
            if (length && ((stage->json[length - 1] >= '0' && stage->json[length - 1] <= '9') ||
                (stage->json[length - 1] >= 'a' && stage->json[length - 1] <= 'z')) &&
                ((*next >= '0' && *next <= '9') || (*next >= 'a' && *next <= 'z'))) return 0;
            continue;
        }
        if (length + 1 >= sizeof(stage->json)) return 0;
        stage->json[length++] = *text;
    }
    at = stage->json;
    if (*at++ != '[') return 0;
    for (;;) {
        struct benchmark_stage_step *step;
        char key[32];
        int kind;
        if (stage->count == BENCHMARK_STAGE_MAXIMUM || *at++ != '{' || !benchmark_stage_name(&at, key) || *at++ != ':') return 0;
        for (kind = 0; kind < 6 && strcmp(key, keys[kind]); kind++) {}
        if (kind == 6) return 0;
        step = &stage->steps[stage->count++];
        step->kind = kind;
        if (kind == BENCHMARK_STAGE_SWITCH_BSP) {
            if (*at < '0' || *at > '9') return 0;
            step->bsp = *at++ - '0';
            if (step->bsp && *at >= '0' && *at <= '9') step->bsp = step->bsp * 10 + (*at++ - '0');
            if (step->bsp > 31) return 0;
        } else if (kind == BENCHMARK_STAGE_DEATHLESS) {
            if (strncmp(at, "true", 4)) return 0;
            at += 4;
        } else if (kind == BENCHMARK_STAGE_AI_SEE) {
            if (*at++ != '[' || !benchmark_stage_name(&at, step->a) || *at++ != ',' ||
                !benchmark_stage_name(&at, step->b) || *at++ != ']') return 0;
        } else if (!benchmark_stage_name(&at, step->a)) return 0;
        if (*at++ != '}') return 0;
        if (*at == ']') return !at[1];
        if (*at++ != ',') return 0;
    }
}

/* Optional route fields are a pair, unique and canonical. Keep the route's existing 4095-byte limit. */
static inline int benchmark_stage_route(const char *text, struct benchmark_stage *stage, long *tick) {
    char json[BENCHMARK_STAGE_JSON_SIZE] = "", number[32] = "";
    int seen_stage = 0, seen_tick = 0;
    memset(stage, 0, sizeof(*stage));
    *tick = 0;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t length = end ? (size_t)(end - text) : strlen(text);
        if (length && text[length - 1] == '\r') length--;
        if (length >= 8 && !strncmp(text, "stage = ", 8)) {
            if (seen_stage++ || length - 8 >= sizeof(json)) return 0;
            memcpy(json, text + 8, length - 8); json[length - 8] = 0;
        } else if (length >= 13 && !strncmp(text, "stage_tick = ", 13)) {
            if (seen_tick++ || length - 13 >= sizeof(number)) return 0;
            memcpy(number, text + 13, length - 13); number[length - 13] = 0;
        }
        if (!end) break;
        text = end + 1;
    }
    if (!seen_stage && !seen_tick) return 1;
    return seen_stage && seen_tick && benchmark_stage_tick(number, tick) &&
        benchmark_stage_parse(json, stage) && !strcmp(json, stage->json);
}

/* lookup returns a flag/encounter index, or the BSP count for SWITCH_BSP. Resolve the whole stage before
   mutating the scenario: even the final reference can invalidate the stage. */
static inline int benchmark_stage_apply(const struct benchmark_stage *stage,
    int (*lookup)(void *, int, const char *), void (*apply)(void *, int, int, int),
    void *context, char *error, size_t size) {
    int a[BENCHMARK_STAGE_MAXIMUM], b[BENCHMARK_STAGE_MAXIMUM];
    int index;
    for (index = 0; index < stage->count; index++) {
        const struct benchmark_stage_step *step = &stage->steps[index];
        a[index] = b[index] = 0;
        if (step->kind == BENCHMARK_STAGE_SWITCH_BSP) {
            a[index] = step->bsp;
            if (step->bsp >= lookup(context, step->kind, NULL)) {
                snprintf(error, size, "stage: missing BSP %d", step->bsp); return 0;
            }
        } else if (step->kind != BENCHMARK_STAGE_DEATHLESS) {
            a[index] = lookup(context, step->kind, step->a);
            if (a[index] < 0) { snprintf(error, size, "stage: missing %s", step->a); return 0; }
            if (step->kind == BENCHMARK_STAGE_AI_SEE) {
                b[index] = lookup(context, step->kind, step->b);
                if (b[index] < 0) { snprintf(error, size, "stage: missing %s", step->b); return 0; }
            }
        }
    }
    for (index = 0; index < stage->count; index++) apply(context, stage->steps[index].kind, a[index], b[index]);
    return 1;
}
#endif
