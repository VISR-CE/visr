"""Exercise host hit visibility using production decisions and a synthetic ray world."""
from pathlib import Path
import subprocess

from test_network_damage_rules import function

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "port/linux/game/network_damage.c"


def test_hit_visibility(tmp_path):
    actual = "\n".join(function(SOURCE, name) for name in (
        "distributed_sight_clear", "distributed_player_saw_hit", "distributed_reach_combine"))
    source = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define FLAG(bit) (1u << (bit))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define PLAYER_TRAIL_TICKS 384
#define PLAYER_TRAIL_POINTS 128
#define PLAYER_TRAIL_INTERVAL_TICKS 3
#define TARGET_HISTORY_SLACK_TICKS 3
#define PLAYER_TRAIL_REPORT_SLACK_TICKS 30
/* Collisions behind the x=5 wall; scenery at x=3 is deliberately ignored. */
enum { _collision_test_front_facing_surfaces_bit, _collision_test_back_facing_surfaces_bit,
    _collision_test_structure_bit, _collision_test_objects_bit, _collision_test_ignore_invisible_surfaces_bit };
typedef int boolean;
typedef float real;
typedef struct { float x,y,z; } real_point3d;
typedef struct { float i,j,k; } real_vector3d;
struct collision_result { int unused; };
struct object_datum { struct { real_point3d bounding_sphere_center; } object; } target;
struct distributed_hit_report { int object_index; long host_time; real_point3d target_position;
    struct { real_point3d origin; } damage; };
struct damage_reach { real distance, ticks, carried_ticks; };
struct damage_trail_point { long time; real_point3d position; real speed, radius;
    real_point3d eye; short structure_bsp_index; } damage_trails[1][PLAYER_TRAIL_POINTS];
static long now = 120;
static int bsp = 1, wall, scenery, invisible, back, calls;
static float wall_x = 5;
static long game_time_get(void) { return now; }
static int global_structure_bsp_index_get(void) { return bsp; }
static float distributed_machine_round_trip_ticks(long machine) { (void)machine;return 6; }
static struct object_datum *object_get(long index) { (void)index;return &target; }
static void object_get_origin(long index, real_point3d *p) { (void)index;*p=(real_point3d){10,0,0}; }
static void vector_from_points3d(real_point3d const *a, real_point3d const *b, real_vector3d *v) {
    *v=(real_vector3d){b->x-a->x,b->y-a->y,b->z-a->z};
}
static float normalize3d(real_vector3d *v) {
    float length=sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);
    if(length) {v->i/=length;v->j/=length;v->k/=length;}return length;
}
static void scale_vector3d(real_vector3d const *v, float scale, real_vector3d *out) {
    *out=(real_vector3d){v->i*scale,v->j*scale,v->k*scale};
}
static int crosses(real_point3d const *p, real_vector3d const *v, float x) {
    if (!v->i) return 0;
    float t=(x-p->x)/v->i;
    return t>=0 && t<=1 && fabsf(p->y+t*v->j)<2;
}
static int collision_test_vector(unsigned flags, real_point3d const *p, real_vector3d const *v,
    long ignored, struct collision_result *hit) {
    (void)ignored;(void)hit;calls++;
    assert(flags & FLAG(_collision_test_front_facing_surfaces_bit));
    assert(!(flags & FLAG(_collision_test_back_facing_surfaces_bit)));
    return ((flags & FLAG(_collision_test_structure_bit)) && wall &&
            !(invisible && (flags & FLAG(_collision_test_ignore_invisible_surfaces_bit))) &&
            (!back || (flags & FLAG(_collision_test_back_facing_surfaces_bit))) && crosses(p,v,wall_x)) ||
        ((flags & FLAG(_collision_test_objects_bit)) && scenery && crosses(p,v,3));
}
''' + actual + r'''
static void reset(void) {
    for(int i=0;i<PLAYER_TRAIL_POINTS;i++) damage_trails[0][i].time=NONE;
    damage_trails[0][0]=(struct damage_trail_point){.time=114,.eye={0,0,1},.structure_bsp_index=1};
    target.object.bounding_sphere_center=(real_point3d){10,0,1};wall=0;scenery=0;invisible=0;back=0;wall_x=5;calls=0;
}
int main(void) {
    struct distributed_hit_report report={.host_time=114,.target_position={10,0,0},.damage={{10,0,1}}};
    struct damage_reach reach={100,30,0};
    reset();assert(distributed_player_saw_hit(0,0,&report,&reach)); /* clear */
    wall=1;assert(!distributed_player_saw_hit(0,0,&report,&reach)); /* BSP */
    invisible=1;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* projectile-passable BSP */
    invisible=0;back=1;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* projectile-passable back face */
    back=0;wall=0;scenery=1;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* mutable scenery */
    wall=1;damage_trails[0][1]=damage_trails[0][0];damage_trails[0][1].time=93;
    damage_trails[0][1].eye.y=8;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* flight/lock-on */
    damage_trails[0][1].time=60;assert(!distributed_player_saw_hit(0,0,&report,&reach)); /* outside window */
    reach.ticks=0;damage_trails[0][1].time=100;
    assert(!distributed_player_saw_hit(0,0,&report,&reach)); /* hitscan's shorter window */
    reach.ticks=384;reach.distance=-1;
    assert(distributed_player_saw_hit(0,0,&report,&reach)); /* unbounded flight still checked */
    damage_trails[0][1].time=NONE;assert(!distributed_player_saw_hit(0,0,&report,&reach));
    reset();wall=1;wall_x=0.2f;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* tracked muzzle */
    wall_x=0.26f;assert(!distributed_player_saw_hit(0,0,&report,&reach)); /* tolerance bound */
    wall_x=1;damage_trails[0][0].radius=1.5f;
    assert(distributed_player_saw_hit(0,0,&report,&reach)); /* covered vehicle seat */
    wall_x=2;assert(!distributed_player_saw_hit(0,0,&report,&reach)); /* vehicle cannot clear far wall */
    reset();wall=1;wall_x=9.995f;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* rounded impact */
    wall_x=9.98f;assert(!distributed_player_saw_hit(0,0,&report,&reach));
    real_point3d a={0,0,0};assert(distributed_sight_clear(&a,&a));
    reset();wall=1;report.damage.origin.y=5;assert(distributed_player_saw_hit(0,0,&report,&reach)); /* edge impact */
    report.damage.origin.y=0;report.host_time=70;
    assert(distributed_player_saw_hit(0,0,&report,&reach)); /* report predates retained history */
    report.host_time=114;damage_trails[0][0].structure_bsp_index=2;calls=0;
    assert(distributed_player_saw_hit(0,0,&report,&reach) && calls==0); /* another BSP */
    damage_trails[0][0].time=NONE;
    assert(distributed_player_saw_hit(0,0,&report,&reach)); /* startup/history gap */
    struct damage_reach first={0,0,0}, unbounded={-1,384,0};
    distributed_reach_combine(&first,&unbounded);assert(first.distance<0 && first.ticks==384);
    puts("host hit visibility probes passed");
}
'''
    path, binary = tmp_path / "hit.c", tmp_path / "hit"
    path.write_text(source)
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(path), "-lm", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_host_gate_and_observability():
    valid = function(SOURCE, "distributed_report_valid")
    start = valid.index("if (!area && !melee && !collision && game_connection()")
    end = valid.index("/* the damage as the host deals it:", start)
    gate = valid[start:end]
    for text in ("_game_connection_network_server", "!input_replay_playing()",
                 "!mp_benchmark_probe_active()", "!distributed_player_saw_hit(",
                 "damage_sight_rejected_tick++", "#ifndef HALO_RELEASE", "return FALSE;"):
        assert text in gate
    handler = function(SOURCE, "network_damage_handle_reports")
    assert handler.index("damage_rejected_reports++") < handler.index("object_cause_damage(")
    assert "host_frame_cpu_phase(24," in function(SOURCE, "network_damage_host_tick")
    assert "unit_get_head_position(unit_index, &point->eye)" in function(SOURCE, "distributed_note_targets")
    deals = function(SOURCE, "distributed_projectile_deals")
    assert deals.index("from_here.ticks += flight.ticks") > deals.index("from_here.distance += flight.distance")
