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
static MTLTimestamp test_cpu, test_gpu;
@interface TestCounterDevice : NSObject
@end
@implementation TestCounterDevice
- (void)sampleTimestamps:(MTLTimestamp *)cpu gpuTimestamp:(MTLTimestamp *)gpu { *cpu = test_cpu; *gpu = test_gpu; }
@end
@interface TestSampleBuffer : NSObject
@property NSData *data;
@end
@implementation TestSampleBuffer
- (NSData *)resolveCounterRange:(NSRange)range { (void)range; return self.data; }
@end
@interface TestCommands : NSObject
@property CFTimeInterval GPUStartTime, GPUEndTime;
@end
@implementation TestCommands
- (MTLCommandBufferStatus)status { return MTLCommandBufferStatusCompleted; }
@end
/* One draw-boundary interval through host_perf_metal_complete: its start and end ticks, and its command buffer's span
 * relative to the host time just before the call. NULL when the bracketed clock pair was dropped (a preemption). */
static typeof(grouped[0]) *complete_one(TestSampleBuffer *samples, uint64_t start_tick, uint64_t end_tick,
    double buffer_from, double buffer_to, double *before, double *after) {
    test_frame++;
    TestCommands *commands = [TestCommands new];
    uint32_t token = host_perf_metal_prepare((id)commands, nil, 0, HOST_PERF_SCENE);
    assert(token);
    buffers[(token - 1) / INTERVAL_COUNT].buffer = (id)samples;
    MTLCounterResultTimestamp ticks[4] = { { start_tick }, { end_tick }, { 0 }, { 0 } };
    samples.data = [NSData dataWithBytes:ticks length:sizeof(ticks)];
    *before = mach_seconds(mach_absolute_time());
    commands.GPUStartTime = *before + buffer_from; commands.GPUEndTime = *before + buffer_to;
    unsigned long dropped_pairs = containment.dropped_pairs;
    host_perf_metal_complete((id)commands);
    *after = mach_seconds(mach_absolute_time());
    return containment.dropped_pairs != dropped_pairs ? NULL : &grouped[test_frame % FRAME_COUNT];
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
        assert(fabs(gpu_seconds(900, 1000, 5.0, 0.001) - (5.0 - 1e-7)) < 1e-12);
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
            assert(frame.drs.available & 1);
            double scene = (double)frame.drs.scene_us, upscaled = scene + (double)frame.drs.upscale_us;
            double input = upscaled + (double)frame.drs.hud_us;
            assert(fabs(scene - merged[0]) <= 1.0 && fabs(upscaled - merged[1]) <= 2.0 && fabs(input - merged[2]) <= 3.0);
        }
        /* The conversion through host_perf_metal_complete: a fake clock pair 2,000,000 ticks after the reference at
         * 1 ns a tick, so a tick 1,500,000 before the pair lands 1.5 ms before the pair's bracketed host time. */
        for (unsigned slot = 0; slot < BUFFER_COUNT; ++slot) atomic_store(&buffers[slot].commands, 0);
        TestSampleBuffer *samples = [TestSampleBuffer new];
        reference_cpu = reference_gpu = 1000;
        test_cpu = test_gpu = 1000 + 2000000;
        memset(&containment, 0, sizeof(containment));
        double before, after;
        typeof(grouped[0]) *converted = NULL;
        for (unsigned attempt = 0; attempt < 8 && !converted; ++attempt)
            converted = complete_one(samples, test_gpu - 1500000, test_gpu - 500000, -0.002, 0.001, &before, &after);
        assert(converted && converted->count == 1 && !converted->invalid && converted->ranges[0].group == HOST_PERF_SCENE);
        assert(converted->ranges[0].start >= before - 1.5e-3 - 1e-9 && converted->ranges[0].start <= after - 1.5e-3 + 1e-9);
        assert(converted->ranges[0].end >= before - 0.5e-3 - 1e-9 && converted->ranges[0].end <= after - 0.5e-3 + 1e-9);
        assert(containment.checked == 1 && containment.outside == 0);
        /* a tick after the pair lands after its host time; a range past its buffer's span counts as outside */
        converted = NULL;
        for (unsigned attempt = 0; attempt < 8 && !converted; ++attempt)
            converted = complete_one(samples, test_gpu - 500000, test_gpu + 100000, -0.002, -0.001, &before, &after);
        assert(converted && converted->count == 1);
        assert(converted->ranges[0].end >= before + 1e-4 - 1e-9 && converted->ranges[0].end <= after + 1e-4 + 1e-9);
        assert(containment.checked == 2 && containment.outside == 1 && containment.worst >= 1.1e-3 - 1e-6);
        /* no rate yet (the pair is the reference): no range, and the frame has no reading */
        test_cpu = reference_cpu; test_gpu = reference_gpu;
        converted = complete_one(samples, 500, 900, -0.002, 0.001, &before, &after);
        assert(converted && converted->count == 0 && (converted->invalid & (1u << HOST_PERF_SCENE)));
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
