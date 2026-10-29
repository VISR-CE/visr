/* Closed staging grammar for signed cases and development recordings. Header-only so the host, guest,
   and portable probes use the same parser without adding an engine dependency to the host. */
#ifndef HALO_BENCHMARK_STAGING_H
#define HALO_BENCHMARK_STAGING_H
#include <stdio.h>
#include <string.h>

#define BENCHMARK_STAGING_MAXIMUM 16
#define BENCHMARK_STAGING_JSON_SIZE 1536
enum benchmark_staging_kind { BENCHMARK_STAGING_SWITCH_BSP, BENCHMARK_STAGING_TELEPORT_PLAYER,
    BENCHMARK_STAGING_AI_PLACE, BENCHMARK_STAGING_AI_SEE, BENCHMARK_STAGING_AI_SEE_PLAYER, BENCHMARK_STAGING_DEATHLESS };
struct benchmark_staging_step { int kind, bsp; char a[32], b[32]; };
struct benchmark_staging {
    int count;
    struct benchmark_staging_step steps[BENCHMARK_STAGING_MAXIMUM];
    char json[BENCHMARK_STAGING_JSON_SIZE];
};

static inline int benchmark_staging_name(const char **cursor, char name[32]) {
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
static inline int benchmark_staging_tick(const char *text, long *tick) {
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

static inline int benchmark_staging_parse(const char *text, struct benchmark_staging *staging) {
    static const char *const keys[] = { "switch_bsp", "teleport_player", "ai_place", "ai_see", "ai_see_player", "deathless" };
    const char *at;
    size_t length = 0;
    int quoted = 0;
    memset(staging, 0, sizeof(*staging));
    /* These strings contain only names, so escapes are outside the grammar. */
    for (; *text; text++) {
        if (*text == '"') quoted = !quoted;
        if (!quoted && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
            const char *next = text;
            while (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\n') next++;
            /* Whitespace may separate JSON tokens, never split a number or a literal. */
            if (length && ((staging->json[length - 1] >= '0' && staging->json[length - 1] <= '9') ||
                (staging->json[length - 1] >= 'a' && staging->json[length - 1] <= 'z')) &&
                ((*next >= '0' && *next <= '9') || (*next >= 'a' && *next <= 'z'))) return 0;
            continue;
        }
        if (length + 1 >= sizeof(staging->json)) return 0;
        staging->json[length++] = *text;
    }
    at = staging->json;
    if (*at++ != '[') return 0;
    for (;;) {
        struct benchmark_staging_step *step;
        char key[32];
        int kind;
        if (staging->count == BENCHMARK_STAGING_MAXIMUM || *at++ != '{' || !benchmark_staging_name(&at, key) || *at++ != ':') return 0;
        for (kind = 0; kind < 6 && strcmp(key, keys[kind]); kind++) {}
        if (kind == 6) return 0;
        step = &staging->steps[staging->count++];
        step->kind = kind;
        if (kind == BENCHMARK_STAGING_SWITCH_BSP) {
            if (*at < '0' || *at > '9') return 0;
            step->bsp = *at++ - '0';
            if (step->bsp && *at >= '0' && *at <= '9') step->bsp = step->bsp * 10 + (*at++ - '0');
            if (step->bsp > 31) return 0;
        } else if (kind == BENCHMARK_STAGING_DEATHLESS) {
            if (strncmp(at, "true", 4)) return 0;
            at += 4;
        } else if (kind == BENCHMARK_STAGING_AI_SEE) {
            if (*at++ != '[' || !benchmark_staging_name(&at, step->a) || *at++ != ',' ||
                !benchmark_staging_name(&at, step->b) || *at++ != ']') return 0;
        } else if (!benchmark_staging_name(&at, step->a)) return 0;
        if (*at++ != '}') return 0;
        if (*at == ']') return !at[1];
        if (*at++ != ',') return 0;
    }
}

/* Optional staging fields travel together. Keep the route's existing 4095-byte limit. */
static inline int benchmark_staging_route(const char *text, struct benchmark_staging *staging, long *tick, long *segment) {
    const char *keys[] = { "staging = ", "staging_tick = ", "staging_segment = " };
    char values[3][BENCHMARK_STAGING_JSON_SIZE] = {{0}};
    int seen[3] = {0}, index;
    memset(staging, 0, sizeof(*staging));
    *tick = *segment = 0;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t length = end ? (size_t)(end - text) : strlen(text);
        if (length && text[length - 1] == '\r') length--;
        for (index = 0; index < 3; index++) {
            size_t prefix = strlen(keys[index]);
            if (length < prefix || strncmp(text, keys[index], prefix)) continue;
            if (seen[index]++ || length - prefix >= sizeof(values[index])) return 0;
            memcpy(values[index], text + prefix, length - prefix);
            values[index][length - prefix] = 0;
        }
        if (!end) break;
        text = end + 1;
    }
    if (!seen[0] && !seen[1] && !seen[2]) return 1;
    return seen[0] && seen[1] && seen[2] && benchmark_staging_tick(values[1], tick) &&
        (!strcmp(values[2], "0") || benchmark_staging_tick(values[2], segment)) &&
        benchmark_staging_parse(values[0], staging) && !strcmp(values[0], staging->json);
}

/* lookup returns a flag/encounter index, or the BSP count for SWITCH_BSP. Resolve the whole staging before
   mutating the scenario: even the final reference can invalidate the staging. */
static inline int benchmark_staging_apply(const struct benchmark_staging *staging,
    int (*lookup)(void *, int, const char *), void (*apply)(void *, int, int, int),
    void *context, char *error, size_t size) {
    int a[BENCHMARK_STAGING_MAXIMUM], b[BENCHMARK_STAGING_MAXIMUM];
    int index;
    for (index = 0; index < staging->count; index++) {
        const struct benchmark_staging_step *step = &staging->steps[index];
        a[index] = b[index] = 0;
        if (step->kind == BENCHMARK_STAGING_SWITCH_BSP) {
            a[index] = step->bsp;
            if (step->bsp >= lookup(context, step->kind, NULL)) {
                snprintf(error, size, "staging: missing BSP %d", step->bsp); return 0;
            }
        } else if (step->kind != BENCHMARK_STAGING_DEATHLESS) {
            a[index] = lookup(context, step->kind, step->a);
            if (a[index] < 0) { snprintf(error, size, "staging: missing %s", step->a); return 0; }
            if (step->kind == BENCHMARK_STAGING_AI_SEE) {
                b[index] = lookup(context, step->kind, step->b);
                if (b[index] < 0) { snprintf(error, size, "staging: missing %s", step->b); return 0; }
            }
        }
    }
    for (index = 0; index < staging->count; index++) apply(context, staging->steps[index].kind, a[index], b[index]);
    return 1;
}
#endif
