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
@interface TestCounterDevice : NSObject
@end
@implementation TestCounterDevice
@end
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
            double base = 100.0 + trial, starts[12], ends[12], separate = 0.0;
            for (unsigned i = 0; i < n; ++i) {
                unsigned group = (seed = seed * 1664525u + 1013904223u) % 3;
                double start = base + ((seed = seed * 1664525u + 1013904223u) % 8000) * 1e-6;
                double end = start + ((seed = seed * 1664525u + 1013904223u) % 4000) * 1e-6;
                starts[i] = start; ends[i] = end; separate += (end - start) * 1e6;
                if (group == HOST_PERF_UPSCALE && (seed >> 16) % 2) {
                    host_perf_metal_upscale_expected();
                    host_perf_metal_upscale_time(test_frame, start, end, 1);
                }
                else entry->ranges[entry->count++] = (gpu_interval){ start, end, group };
            }
            /* the union, by merging the intervals */
            for (unsigned i = 1; i < n; ++i)
                for (unsigned j = i; j && starts[j - 1] > starts[j]; --j) {
                    double t = starts[j]; starts[j] = starts[j - 1]; starts[j - 1] = t;
                    t = ends[j]; ends[j] = ends[j - 1]; ends[j - 1] = t;
                }
            double merged = 0.0, from = starts[0], to = ends[0];
            for (unsigned i = 1; i < n; ++i) {
                if (starts[i] > to) { merged += to - from; from = starts[i]; to = ends[i]; }
                else if (ends[i] > to) to = ends[i];
            }
            merged = (merged + to - from) * 1e6;
            frame = (game_perf_frame){.frame_id = test_frame};
            host_perf_metal_join(&frame);
            assert(frame.drs.available & 1);
            double input = (double)(frame.drs.scene_us + frame.drs.upscale_us + frame.drs.hud_us);
            assert(input <= merged + 2.0 && input >= merged - 2.0 && input <= separate + 2.0);
        }
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
