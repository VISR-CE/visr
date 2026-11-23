/* Bounded single-producer collection; canonicalization runs on the worker. */
#include "shader_observations.h"
#include "shader_record.h"
#include <string.h>
#include <stdio.h>

void shader_observations_init(struct shader_observation_collector *c)
{
    memset(c, 0, sizeof(*c));
    atomic_init(&c->read, 0); atomic_init(&c->write, 0); atomic_init(&c->dropped, 0);
}
int shader_observations_append(struct shader_observation_collector *c,
    const struct shader_observation_generation *g, const struct shader_observation *d)
{
    uint32_t write, read;
    if (!g || !g->token || !g->fingerprint[0] || !d->metal || d->suppressed || d->warming) return 0;
    write = atomic_load_explicit(&c->write, memory_order_relaxed);
    read = atomic_load_explicit(&c->read, memory_order_acquire);
    if (write - read >= SHADER_OBSERVATION_CAPACITY) {
        atomic_fetch_add_explicit(&c->dropped, 1, memory_order_relaxed); return 0;
    }
    c->items[write % SHADER_OBSERVATION_CAPACITY].generation = *g;
    c->items[write % SHADER_OBSERVATION_CAPACITY].descriptor = *d;
    atomic_store_explicit(&c->write, write + 1, memory_order_release);
    return 1;
}
int shader_observations_take(struct shader_observation_collector *c, struct shader_observation_item *item)
{
    uint32_t read = atomic_load_explicit(&c->read, memory_order_relaxed);
    if (read == atomic_load_explicit(&c->write, memory_order_acquire)) return 0;
    *item = c->items[read % SHADER_OBSERVATION_CAPACITY];
    atomic_store_explicit(&c->read, read + 1, memory_order_release);
    return 1;
}
int shader_observations_lines(const struct shader_observation *d, char *line, char *vertex, char *pixel)
{
    struct shader_record v = {0}, p = {0}, pipeline = {0};
    uint32_t i;
    line[0] = vertex[0] = pixel[0] = 0;
    if (d->kind < SHADER_OBSERVATION_VERTEX || d->kind > SHADER_OBSERVATION_PIPELINE || d->variant > 1) return 0;
    v.kind = SHADER_RECORD_VS; v.vertex_hash = d->vertex_hash; v.variant = d->variant; v.mask = d->packed_mask;
    if (d->kind != SHADER_OBSERVATION_VERTEX) {
        if (!d->pixel_size || d->pixel_size > SHADER_OBSERVATION_PIXEL_MAX || d->pixel_size > SHADER_RECORD_KEY_MAX ||
            shader_record_hash(d->pixel, d->pixel_size) != d->pixel_hash) return 0;
        p.kind = SHADER_RECORD_PS; p.hash = d->pixel_hash; p.key_size = d->pixel_size;
        memcpy(p.key, d->pixel, d->pixel_size);
        shader_record_format(&p, pixel);
    }
    if (d->kind != SHADER_OBSERVATION_PIXEL) shader_record_format(&v, vertex);
    if (d->kind == SHADER_OBSERVATION_VERTEX) strcpy(line, vertex);
    else if (d->kind == SHADER_OBSERVATION_PIXEL) strcpy(line, pixel);
    else {
        /* the ranges are the contract's (shader_record_admit), written once */
        pipeline.kind = SHADER_RECORD_PIPELINE; pipeline.vertex_hash = d->vertex_hash; pipeline.variant = d->variant;
        pipeline.mask = d->packed_mask; pipeline.hash = d->pixel_hash; pipeline.blend = d->blend;
        pipeline.source = d->source; pipeline.destination = d->destination; pipeline.operation = d->operation;
        pipeline.write_mask = d->write_mask; pipeline.exact_borders = d->exact_borders; pipeline.depth = d->depth;
        pipeline.alpha_source = d->alpha_source; pipeline.alpha_destination = d->alpha_destination;
        for (i = 0; i < SHADER_RECORD_ATTRIBUTES; ++i) pipeline.attribute_kinds[i] = d->attribute_kinds[i];
        if (!shader_record_admit(&pipeline, NULL)) return 0;
        shader_record_format(&pipeline, line);
    }
    return 1;
}
