/* Apache-2.0. Deterministic sampler metadata checks; does not submit GPU work. */
#define HALO_PERF_REPORTS 1
static int test_eligible = 1, test_collecting = 1, test_shadow;
int host_perf_hooks_dynamic_enabled(void) { return test_shadow; }
int host_perf_reports_enabled(void) { return test_eligible; }
int host_perf_reports_collecting(void) { return test_eligible && test_collecting; }
#include "../host/host_perf_metal.m"
#include <assert.h>
static uint64_t test_frame = 1;
uint64_t host_perf_hooks_frame_id(void) { return test_frame; }
#include <unistd.h>
static MTLTimestamp test_cpu, test_gpu;
static int test_slow_pair; /* the pair's call takes 100 us, as when the completion thread is preempted */
@interface TestCounterDevice : NSObject
@end
@implementation TestCounterDevice
- (void)sampleTimestamps:(MTLTimestamp *)cpu gpuTimestamp:(MTLTimestamp *)gpu {
    if (test_slow_pair) usleep(100);
    *cpu = test_cpu; *gpu = test_gpu;
}
@end
@interface TestSampleBuffer : NSObject
@property NSData *data;
@end
@implementation TestSampleBuffer
- (NSData *)resolveCounterRange:(NSRange)range { (void)range; return self.data; }
@end
@interface TestCommands : NSObject
@property CFTimeInterval GPUStartTime, GPUEndTime;
@property int errored;
@end
@implementation TestCommands
- (MTLCommandBufferStatus)status { return self.errored ? MTLCommandBufferStatusError : MTLCommandBufferStatusCompleted; }
@end
/* One interval through host_perf_metal_complete: its four samples (draw boundary uses the first two), whether its
 * command buffer errored, and the buffer's span relative to the host time just before the call (no span when both
 * are 0). Returns the slot it used through *slot when asked. */
static typeof(grouped[0]) *complete_ticks(TestSampleBuffer *samples, const uint64_t tick[4], int errored,
    double buffer_from, double buffer_to, double *before, double *after, unsigned *slot) {
    test_frame++;
    TestCommands *commands = [TestCommands new];
    commands.errored = errored;
    uint32_t token = host_perf_metal_prepare((id)commands, nil, 0, HOST_PERF_SCENE);
    assert(token);
    buffers[(token - 1) / INTERVAL_COUNT].buffer = (id)samples;
    if (slot) *slot = (token - 1) / INTERVAL_COUNT;
    MTLCounterResultTimestamp ticks[4] = { { tick[0] }, { tick[1] }, { tick[2] }, { tick[3] } };
    samples.data = [NSData dataWithBytes:ticks length:sizeof(ticks)];
    *before = mach_seconds(mach_absolute_time());
    if (buffer_from || buffer_to) { commands.GPUStartTime = *before + buffer_from; commands.GPUEndTime = *before + buffer_to; }
    host_perf_metal_complete((id)commands);
    *after = mach_seconds(mach_absolute_time());
    return &grouped[test_frame % FRAME_COUNT];
}
static typeof(grouped[0]) *complete_one(TestSampleBuffer *samples, uint64_t start_tick, uint64_t end_tick,
    double buffer_from, double buffer_to, double *before, double *after) {
    return complete_ticks(samples, (const uint64_t[4]){ start_tick, end_tick, 0, 0 }, 0, buffer_from, buffer_to,
        before, after, NULL);
}
/* every slot's samples count as fresh again (as before any completion) */
static void forget_completions(void) {
    for (unsigned slot = 0; slot < BUFFER_COUNT; ++slot) buffers[slot].fresh_after = 0;
}
int main(void) {
    @autoreleasepool {
        /* a GPU that samples counters at both boundaries, whatever this Mac's does (metal_gate.h) */
        metal_gate_adopt_facts(&(struct metal_gate_facts){ .apple_families = 0xfe /* Apple1 to Apple7 */,
            .queries = (UINT32_C(1) << METAL_QUERY_COUNTERS_STAGE) | (UINT32_C(1) << METAL_QUERY_COUNTERS_DRAW) });
        NSObject *commands[BUFFER_COUNT + 1];
        for (unsigned i = 0; i <= BUFFER_COUNT; ++i) commands[i] = [NSObject new];
        assert(host_perf_metal_prepare((id)commands[0], nil, 0, HOST_PERF_SCENE) == 0);
        mode = 1; /* Metadata needs no GPU capability or counter buffer. */
        test_eligible = 0;
        host_perf_metal_initialize(nil);
        assert(mode == 1 && counter_device == nil); /* disabled initialization is inert */
        assert(!host_perf_metal_prepare((id)commands[0], nil, 0, HOST_PERF_SCENE));
        test_eligible = 1;
        test_collecting = 0; /* eligible tester build, suppressed automated run */
        assert(!host_perf_metal_prepare((id)commands[0], nil, 0, HOST_PERF_SCENE));
        assert(!last_frame && !buffers[0].count && !atomic_load(&buffers[0].commands));
        assert(!host_perf_metal_dropped());
        mode = 2;
        /* Stage sampling: upscale and HUD share one encoder without spoiling
         * the frame; a switch involving the scene cannot be told apart. */
        test_shadow = 1; test_frame = 300;
        buffers[0].groups[0] = HOST_PERF_UPSCALE;
        assert(host_perf_metal_switch(1, nil, HOST_PERF_HUD) == 1);
        assert(!grouped[300 % FRAME_COUNT].invalid);
        buffers[0].groups[0] = HOST_PERF_SCENE;
        assert(host_perf_metal_switch(1, nil, HOST_PERF_HUD) == 1);
        assert(grouped[300 % FRAME_COUNT].invalid == ((1u << HOST_PERF_SCENE) | (1u << HOST_PERF_HUD)));
        buffers[0].groups[0] = 0;
        test_shadow = 0; test_frame = 1;
        mode = 1;
        test_collecting = 1;
        for (unsigned i = 0; i < INTERVAL_COUNT; ++i)
            assert(host_perf_metal_prepare((id)commands[0], nil, 0, HOST_PERF_SCENE));
        assert(!host_perf_metal_prepare((id)commands[0], nil, 0, HOST_PERF_SCENE));
        assert(host_perf_metal_dropped() == 1);
        game_perf_frame frame = {.frame_id = 1};
        host_perf_metal_join(&frame);
        assert(!(frame.detail.available & (1u << 8))); /* partial category refused */
        add(1, 1, 0, 1); /* a measured submicrosecond interval can round to zero */
        host_perf_metal_join(&frame);
        assert(frame.detail.available & (1u << 9));
        frame.flags = GAME_PERF_GPU_UNAVAILABLE;
        frame.detail.available = 0;
        host_perf_metal_join(&frame);
        assert(!frame.detail.available); /* unresolved overall frame */
        test_frame += FRAME_COUNT;
        assert(host_perf_metal_prepare((id)commands[1], nil, 1, HOST_PERF_SCENE));
        add(1, 1, 999, 1); /* late callback cannot enrich the reused generation */
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(!frame.detail.available);
        add(test_frame, 1, 25, 1);
        add(test_frame, 1, 15, 1);
        host_perf_metal_join(&frame);
        assert(frame.detail.gpu_pass_us[1] == 40 && frame.detail.available == (1u << 9));
        for (unsigned i = 2; i < BUFFER_COUNT; ++i)
            assert(host_perf_metal_prepare((id)commands[i], nil, 2, HOST_PERF_SCENE));
        assert(!host_perf_metal_prepare((id)commands[BUFFER_COUNT], nil, 2, HOST_PERF_SCENE));
        assert(host_perf_metal_dropped() == 2);
        /* Shadow off: reports touch no semantic-group state or lock. */
        test_frame = 400; frame_prepare(test_frame);
        host_perf_metal_unavailable(HOST_PERF_SCENE);
        assert(grouped[400 % FRAME_COUNT].frame != 400 && !grouped[400 % FRAME_COUNT].invalid);
        /* Presentation callbacks repair frame-ID attribution when reordered. */
        test_shadow = 1;
        test_frame = 500;
        frame_prepare(test_frame);
        host_perf_metal_presented(500, 1000000, 16667);
        test_frame = 501; frame_prepare(test_frame);
        test_frame = 502; frame_prepare(test_frame);
        host_perf_metal_presented(502, 1050000, 16667);
        host_perf_metal_presented(501, 1016667, 16667);
        frame = (game_perf_frame){.frame_id = 502};
        host_perf_metal_join(&frame);
        assert(frame.drs.presentation_us == 33333 && frame.drs.late && (frame.drs.available & 2));
        frame = (game_perf_frame){.frame_id = 501};
        host_perf_metal_join(&frame);
        assert(frame.drs.presentation_us == 16667 && !frame.drs.late);
        test_frame = 502 + FRAME_COUNT; frame_prepare(test_frame);
        host_perf_metal_presented(502, 9999999, 16667); /* stale callback */
        assert(!grouped[test_frame % FRAME_COUNT].presented);
        /* Stage-envelope union charges overlapping ticks exactly once. */
        counter_device = (id)[TestCounterDevice new];
        typeof(grouped[0]) *entry = &grouped[test_frame % FRAME_COUNT];
        entry->count = 3;
        entry->ranges[0] = (gpu_interval){ 200e-6, 400e-6, HOST_PERF_HUD };
        entry->ranges[1] = (gpu_interval){ 100e-6, 250e-6, HOST_PERF_SCENE };
        entry->ranges[2] = (gpu_interval){ 150e-6, 300e-6, HOST_PERF_UPSCALE };
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(frame.drs.available & 1);
        assert(frame.drs.scene_us == 150 && frame.drs.upscale_us == 50 && frame.drs.hud_us == 100);
        entry->invalid = 1u << HOST_PERF_UPSCALE;
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(!(frame.drs.available & 1));
        /* A GPU tick moves to the CPU timebase from the pair sampled after it. */
        assert(fabs(gpu_seconds(900, 1000, 5.0, 125.0 / 3.0 / 1000.0) - (5.0 - 100 * 125.0 / 3.0 * 1e-9)) < 1e-12);
        /* Bug class: the planner's S+U+H counted overlapping GPU intervals twice, so under MetalFX it read more GPU
         * time than the frame took (M1 iPad Pro, 120 Hz: 9.97 ms against 5.57) and held DRS at its floor. MetalFX's
         * command buffer starts while the scene drains; the HUD's vertex stage starts while MetalFX runs. */
        test_frame++; frame_prepare(test_frame);
        entry = &grouped[test_frame % FRAME_COUNT];
        host_perf_metal_upscale_expected();
        entry->ranges[entry->count++] = (gpu_interval){ 10.0, 10.0022, HOST_PERF_SCENE };
        host_perf_metal_upscale_time(test_frame, 10.0011, 10.0054, 1);
        entry->ranges[entry->count++] = (gpu_interval){ 10.0012, 10.0055, HOST_PERF_HUD };
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(frame.drs.available & 1);
        assert(frame.drs.scene_us == 2200 && frame.drs.upscale_us == 3200 && frame.drs.hud_us == 100);
        /* Dependency order, not start order: a scene encoder that starts after MetalFX's buffer began (which waits
         * on it) is still scene work, so it stays in S, the planner's scalable term. */
        test_frame++; frame_prepare(test_frame);
        entry = &grouped[test_frame % FRAME_COUNT];
        host_perf_metal_upscale_expected();
        entry->ranges[entry->count++] = (gpu_interval){ 20.0, 20.0010, HOST_PERF_SCENE };
        host_perf_metal_upscale_time(test_frame, 20.0011, 20.0054, 1);
        entry->ranges[entry->count++] = (gpu_interval){ 20.0012, 20.0022, HOST_PERF_SCENE };
        entry->ranges[entry->count++] = (gpu_interval){ 20.0012, 20.0055, HOST_PERF_HUD };
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(frame.drs.available & 1);
        assert(frame.drs.scene_us == 2000 && frame.drs.upscale_us == 3300 && frame.drs.hud_us == 100);
        /* a MetalFX span still on its way leaves the frame without a reading */
        test_frame++; frame_prepare(test_frame);
        entry = &grouped[test_frame % FRAME_COUNT];
        host_perf_metal_upscale_expected();
        entry->ranges[entry->count++] = (gpu_interval){ 11.0, 11.002, HOST_PERF_SCENE };
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(!(frame.drs.available & 1));
        /* The class, over random overlapping intervals of every part: the planner's input equals the union of the
         * frame's GPU intervals, so it never exceeds the frame's measured GPU time (which covers that union). */
        uint32_t seed = 12345;
        for (unsigned trial = 0; trial < 4000; ++trial) {
            test_frame++; frame_prepare(test_frame);
            entry = &grouped[test_frame % FRAME_COUNT];
            unsigned n = 1 + (seed = seed * 1664525u + 1013904223u) % 12;
            double base = 100.0 + trial, starts[12], ends[12];
            unsigned groups[12];
            for (unsigned i = 0; i < n; ++i) {
                unsigned group = (seed = seed * 1664525u + 1013904223u) % 3;
                double start = base + ((seed = seed * 1664525u + 1013904223u) % 8000) * 1e-6;
                double end = start + ((seed = seed * 1664525u + 1013904223u) % 4000) * 1e-6;
                starts[i] = start; ends[i] = end; groups[i] = group;
                if (group == HOST_PERF_UPSCALE && (seed >> 16) % 2) {
                    host_perf_metal_upscale_expected();
                    host_perf_metal_upscale_time(test_frame, start, end, 1);
                }
                else entry->ranges[entry->count++] = (gpu_interval){ start, end, group };
            }
            /* oracles: the union of the scene's intervals, of scene and upscale, and of all, by merging */
            double merged[3];
            for (unsigned last = 0; last < 3; ++last) {
                double from = 0.0, to = -1.0, total = 0.0;
                unsigned order[12], m = 0;
                for (unsigned i = 0; i < n; ++i) if (groups[i] <= last) {
                    unsigned j = m++;
                    while (j && starts[order[j - 1]] > starts[i]) { order[j] = order[j - 1]; --j; }
                    order[j] = i;
                }
                for (unsigned k = 0; k < m; ++k) {
                    unsigned i = order[k];
                    if (to < from || starts[i] > to) { if (to >= from) total += to - from; from = starts[i]; to = ends[i]; }
                    else if (ends[i] > to) to = ends[i];
                }
                if (m && to >= from) total += to - from;
                merged[last] = total * 1e6;
            }
            frame = (game_perf_frame){.frame_id = test_frame};
            host_perf_metal_join(&frame);
            unsigned scenes = 0;
            for (unsigned i = 0; i < n; ++i) scenes += groups[i] == HOST_PERF_SCENE;
            /* (no scene interval: no reading, rather than S = 0) */
            assert(!(frame.drs.available & 1) == !scenes);
            if (!scenes) continue;
            double scene = (double)frame.drs.scene_us, upscaled = scene + (double)frame.drs.upscale_us;
            double input = upscaled + (double)frame.drs.hud_us;
            assert(fabs(scene - merged[0]) <= 1.0 && fabs(upscaled - merged[1]) <= 2.0 && fabs(input - merged[2]) <= 3.0);
        }
        /* The conversion through host_perf_metal_complete, at Apple silicon's 125/3 ns a tick (an inverted rate would
         * put the ranges seconds away): the pair is 3,000,000 ticks and 125 ms after the reference, so a tick 36,000
         * before the pair lands 1.5 ms before the pair's bracketed host time. */
        for (unsigned slot = 0; slot < BUFFER_COUNT; ++slot) atomic_store(&buffers[slot].commands, 0);
        TestSampleBuffer *samples = [TestSampleBuffer new];
        reference_cpu = reference_gpu = 1000;
        test_gpu = 1000 + 3000000; test_cpu = 1000 + 125000000;
        memset(&containment, 0, sizeof(containment));
        double before, after;
        clock_pair.kept = 0;
        typeof(grouped[0]) *converted = complete_one(samples, test_gpu - 36000, test_gpu - 12000, -0.002, 0.001, &before, &after);
        assert(converted->count == 1 && !converted->invalid && converted->ranges[0].group == HOST_PERF_SCENE);
        assert(converted->ranges[0].start >= before - 1.5e-3 - 1e-9 && converted->ranges[0].start <= after - 1.5e-3 + 1e-9);
        assert(converted->ranges[0].end >= before - 0.5e-3 - 1e-9 && converted->ranges[0].end <= after - 0.5e-3 + 1e-9);
        assert(containment.checked == 1 && containment.outside == 0);
        /* a range ending 0.1 ms before the pair, past its buffer's span (which ends 1 ms before the call), counts as
         * outside and is clipped to the buffer */
        clock_pair.kept = 0; forget_completions();
        converted = complete_one(samples, test_gpu - 12000, test_gpu - 2400, -0.002, -0.001, &before, &after);
        assert(converted->count == 1);
        assert(containment.checked == 2 && containment.outside == 1 && containment.worst >= 0.9e-3 - 1e-6);
        assert(fabs(converted->ranges[0].end - (before - 0.001)) < 1e-9);
        assert(converted->ranges[0].start >= before - 0.5e-3 - 1e-9 - 0.001 && converted->ranges[0].start <= converted->ranges[0].end);
        /* a tick after the pair cannot belong to this buffer (its work finished before the pair was taken): no range */
        clock_pair.kept = 0; forget_completions();
        unsigned long stale = stale_samples;
        converted = complete_one(samples, test_gpu - 12000, test_gpu + 2400, -0.002, -0.001, &before, &after);
        assert(converted->count == 0 && (converted->invalid & (1u << HOST_PERF_SCENE)) && stale_samples == stale + 1);
        /* A start sample 2 s before the buffer (2 s = 48,000,000 ticks) that the freshness test cannot catch (no
         * earlier completion of its slot): the range is clipped to the buffer's 3 ms span, so the frame's S is at
         * most that, never seconds. */
        clock_pair.kept = 0; forget_completions();
        test_gpu = 1000 + 300000000; test_cpu = 1000 + 12500000000; /* (the same rate, far enough from the reference) */
        converted = complete_one(samples, test_gpu - 48000000, test_gpu - 12000, -0.003, 0.001, &before, &after);
        assert(converted->count == 1 && containment.outside == 2 && containment.worst >= 1.99);
        assert(fabs(converted->ranges[0].start - (before - 0.003)) < 1e-9);
        {
            game_perf_frame clipped = {.frame_id = test_frame};
            host_perf_metal_join(&clipped);
            assert((clipped.drs.available & 1) && clipped.drs.scene_us <= 3001);
        }
        /* A preempted pair (a 100 us bracket) is not adopted while a tighter one that agrees with it is kept, and the
         * frame keeps its reading: its ranges convert through the kept pair. */
        clock_pair = (typeof(clock_pair)){ 1, test_gpu, mach_seconds(mach_absolute_time()), 0.0 };
        forget_completions();
        double kept_host = clock_pair.host;
        unsigned long wide = clock_stats.wide, steps = clock_stats.steps;
        test_slow_pair = 1;
        converted = complete_one(samples, test_gpu - 36000, test_gpu - 12000, -0.1, 0.1, &before, &after);
        test_slow_pair = 0;
        assert(converted->count == 1 && !converted->invalid);
        assert(clock_pair.host == kept_host && clock_stats.wide == wide + 1 && clock_stats.widest >= 100e-6);
        assert(clock_stats.steps == steps);
        assert(fabs(converted->ranges[0].start - (kept_host - 1.5e-3)) < 1e-9);
        /* The GPU's clock steps 10 ms (240,000 ticks) while the host's barely moves: the kept pair mispredicts the new
         * one, so the new one is adopted at once and the ranges land against this call's host time. */
        test_gpu += 240000; test_cpu += 10000000;
        converted = complete_one(samples, test_gpu - 36000, test_gpu - 12000, -0.1, 0.1, &before, &after);
        assert(clock_stats.steps == steps + 1 && clock_pair.gpu == test_gpu && converted->count == 1);
        assert(converted->ranges[0].start >= before - 1.5e-3 - 1e-9 && converted->ranges[0].start <= after - 1.5e-3 + 1e-9);
        /* Stage boundary, every slot last completed at G1 (10 ms before this pair): a pass that wrote only its fragment
         * samples leaves the vertex pair from the slot's last use, which is dropped; the interval keeps the fragment
         * pair, so its range starts 239,000 ticks (9.958 ms) before the pair. */
        mode = 2;
        MTLTimestamp g1 = test_gpu;
        test_gpu += 240000; test_cpu += 10000000;
        clock_pair.kept = 0;
        for (unsigned slot = 0; slot < BUFFER_COUNT; ++slot) buffers[slot].fresh_after = g1;
        stale = stale_samples;
        converted = complete_ticks(samples, (const uint64_t[4]){ g1 - 1000, g1 - 500, g1 + 1000, test_gpu - 12000 }, 0,
            -0.02, 0.001, &before, &after, NULL);
        assert(converted->count == 1 && !converted->invalid && stale_samples == stale + 1);
        assert(converted->ranges[0].start >= before - 239000 * 125e-9 / 3 - 1e-9);
        assert(converted->ranges[0].start <= after - 239000 * 125e-9 / 3 + 1e-9);
        /* all four samples from the slot's last use: no range, with or without the buffer's span to clip against */
        for (int spanned = 1; spanned >= 0; --spanned) {
            clock_pair.kept = 0;
            for (unsigned slot = 0; slot < BUFFER_COUNT; ++slot) buffers[slot].fresh_after = g1;
            stale = stale_samples;
            converted = complete_ticks(samples, (const uint64_t[4]){ g1 - 4000, g1 - 3000, g1 - 2000, g1 - 1000 }, 0,
                spanned ? -0.02 : 0.0, spanned ? 0.001 : 0.0, &before, &after, NULL);
            assert(converted->count == 0 && (converted->invalid & (1u << HOST_PERF_SCENE)) && stale_samples == stale + 2);
        }
        /* an errored buffer still marks its slot: samples from before its completion are stale at the next use */
        {
            unsigned slot;
            test_gpu += 24000; test_cpu += 1000000;
            clock_pair.kept = 0;
            complete_ticks(samples, (const uint64_t[4]){ 0, 0, 0, 0 }, 1, -0.002, 0.001, &before, &after, &slot);
            assert(buffers[slot].fresh_after == test_gpu);
        }
        mode = 1;
        /* no rate yet (the pair is the reference): no range, and the frame has no reading */
        clock_pair.kept = 0; forget_completions();
        test_cpu = reference_cpu; test_gpu = reference_gpu;
        converted = complete_one(samples, 500, 900, -0.002, 0.001, &before, &after);
        assert(converted->count == 0 && (converted->invalid & (1u << HOST_PERF_SCENE)));
        /* a frame with only MetalFX's span (no scene range) has no reading, rather than S = 0 */
        test_frame++; frame_prepare(test_frame);
        host_perf_metal_upscale_expected();
        host_perf_metal_upscale_time(test_frame, 30.0, 30.001, 1);
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(!(frame.drs.available & 1));
        test_collecting = 0; test_shadow = 1;
        test_frame++; frame_prepare(test_frame);
        atomic_store(&buffers[1].commands, 0);
        assert(host_perf_metal_prepare((id)commands[BUFFER_COUNT], nil, 1, HOST_PERF_SCENE));
        /* No stage-boundary sampling: the scene's command-buffer spans (a union, overlap counted once) are S,
         * the least of the rest's last 32 spans is U+H, and the frame is marked coarse */
        counter_device = nil; mode = 0; metal_seen = 1;
        assert(host_perf_metal_timing_supported() && !host_perf_metal_stage_supported());
        for (unsigned f = 0; f < 3; ++f) {
            test_frame++;
            host_perf_metal_frame_touch(test_frame);
            host_perf_metal_command_span(test_frame, HOST_PERF_SCENE, 1.000, 1.004);
            host_perf_metal_command_span(test_frame, HOST_PERF_SCENE, 1.003, 1.006); /* overlaps by 1 ms */
            host_perf_metal_command_span(test_frame, HOST_PERF_HUD, 1.006, 1.007 + (f == 2 ? 0.003 : 0.0));
            frame = (game_perf_frame){.frame_id = test_frame};
            host_perf_metal_join(&frame);
            assert((frame.drs.available & (1 | 8 | 16)) == (1 | 8 | 16));
            assert(frame.drs.scene_us >= 5999 && frame.drs.scene_us <= 6001);
            /* (the third frame's 4 ms present span is a drawable wait; the estimate stays near 1 ms) */
            assert(frame.drs.upscale_us == 0 && frame.drs.hud_us >= 999 && frame.drs.hud_us <= 1001);
        }
        /* The fallback's rest overlapping the scene's last buffer is charged to the scene, not counted twice. */
        test_frame++; host_perf_metal_frame_touch(test_frame);
        host_perf_metal_command_span(test_frame, HOST_PERF_SCENE, 3.000, 3.004);
        host_perf_metal_command_span(test_frame, HOST_PERF_UPSCALE, 3.003, 3.005);
        host_perf_metal_command_span(test_frame, HOST_PERF_HUD, 3.0045, 3.006);
        {
            double scene, rest;
            span_charge(&grouped[test_frame % FRAME_COUNT], &scene, &rest);
            assert(fabs(scene - 4000.0) < 1.0 && fabs(rest - 2000.0) < 1.0);
        }
        /* a refused scene (several views) gives no reading */
        test_frame++; host_perf_metal_frame_touch(test_frame);
        host_perf_metal_command_span(test_frame, HOST_PERF_SCENE, 2.0, 2.005);
        host_perf_metal_unavailable(HOST_PERF_SCENE);
        frame = (game_perf_frame){.frame_id = test_frame};
        host_perf_metal_join(&frame);
        assert(!(frame.drs.available & 1));
        assert(sizeof(buffers) + sizeof(sums) + sizeof(grouped) + BUFFER_COUNT * INTERVAL_COUNT * 4 * sizeof(MTLCounterResultTimestamp) < 1024 * 1024);
    }
    return 0;
}
