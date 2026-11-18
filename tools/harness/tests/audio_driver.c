/* Handing the mixer between the SDL device and the silent clock: never two drivers, never none. */
#include "harness.h"
#include <pthread.h>
#include <unistd.h>
#include <time.h>
/* The 32-bit harness build rejects __atomic_* on ints (-Watomic-alignment), so a mutex stands in for them. */
static pthread_mutex_t atomic_shim_lock = PTHREAD_MUTEX_INITIALIZER;
static int shim_add(volatile int *p, int v) { pthread_mutex_lock(&atomic_shim_lock); int r = (*p += v); pthread_mutex_unlock(&atomic_shim_lock); return r; }
static int shim_load(volatile int *p) { pthread_mutex_lock(&atomic_shim_lock); int r = *p; pthread_mutex_unlock(&atomic_shim_lock); return r; }
static void shim_store(volatile int *p, int v) { pthread_mutex_lock(&atomic_shim_lock); *p = v; pthread_mutex_unlock(&atomic_shim_lock); }
#define __atomic_add_fetch(p, v, o) shim_add((p), (v))
#define __atomic_sub_fetch(p, v, o) shim_add((p), -(v))
#define __atomic_load_n(p, o) shim_load((p))
#define __atomic_store_n(p, v, o) shim_store((p), (v))
typedef int BOOL;
#define SDLCALL
#define MIX_CHUNK_FRAMES 512
#define OUTPUT_CHANNELS 2
#define OUTPUT_RATE 48000
#define SDL_AUDIO_F32 1
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0
#define SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES "frames"
typedef struct { int format, channels, freq; } SDL_AudioSpec;
typedef struct SDL_AudioStream { pthread_t thread; int stop, resumed; } SDL_AudioStream;
typedef void (*SDL_AudioStreamCallback)(void *, SDL_AudioStream *, int, int);
static SDL_AudioStreamCallback device_callback;
static int open_fails, closes, close_saw_clock, close_saw_gate, clock_run_probe_valid;
static int inflight, overlap, mixes, device_mixes, dual;
static pthread_key_t device_key;
static pthread_once_t device_key_once = PTHREAD_ONCE_INIT;
static void make_device_key(void) { pthread_key_create(&device_key, NULL); }
static const char *SDL_GetError(void) { return "fake"; }
static void platform_log(const char *format, ...) { (void)format; }
static void SDL_SetHint(const char *name, const char *value) { (void)name; (void)value; }
static void SDL_PutAudioStreamData(SDL_AudioStream *s, const void *b, int n) { (void)s; (void)b; (void)n; }
static int clock_seen;
static int clock_run; /* defined again by the code under test */
static void mix(float *buffer, unsigned long frames) {
 (void)buffer; (void)frames;
 if (__atomic_add_fetch(&inflight, 1, __ATOMIC_SEQ_CST) > 1) overlap = 1;
 usleep(200);
 __atomic_add_fetch(&mixes, 1, __ATOMIC_SEQ_CST);
 if (pthread_getspecific(device_key)) {
  __atomic_add_fetch(&device_mixes, 1, __ATOMIC_SEQ_CST);
  /* the device mixing while the clock is still running means two drivers (double speed) */
  if (!clock_run) clock_seen = 0;
  else if (clock_seen) dual = 1;
 } else clock_seen = 1;
 __atomic_sub_fetch(&inflight, 1, __ATOMIC_SEQ_CST);
}
static void *device_thread(void *parameter) {
 SDL_AudioStream *s = parameter;
 pthread_once(&device_key_once, make_device_key);
 pthread_setspecific(device_key, s);
 while (!__atomic_load_n(&s->stop, __ATOMIC_ACQUIRE)) {
  if (__atomic_load_n(&s->resumed, __ATOMIC_ACQUIRE)) device_callback(NULL, s, 4096, 0);
  usleep(300);
 }
 return NULL;
}
static SDL_AudioStream *SDL_OpenAudioDeviceStream(int device, const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) {
 (void)device; (void)spec; (void)userdata;
 if (open_fails) return NULL;
 SDL_AudioStream *s = calloc(1, sizeof(*s));
 device_callback = callback;
 pthread_create(&s->thread, NULL, device_thread, s);
 return s;
}
static void SDL_ResumeAudioStreamDevice(SDL_AudioStream *s) { __atomic_store_n(&s->resumed, 1, __ATOMIC_RELEASE); }
#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
static int clock_nanosleep(int id, int flags, const struct timespec *t, struct timespec *r) {
 struct timespec now; (void)id; (void)flags; (void)r;
 clock_gettime(CLOCK_MONOTONIC, &now);
 long ns = (t->tv_sec - now.tv_sec) * 1000000000L + (t->tv_nsec - now.tv_nsec);
 if (ns > 0) usleep((unsigned)(ns / 1000));
 return 0;
}
#endif
#define SDL_DestroyAudioStream fake_destroy
static void fake_destroy(SDL_AudioStream *s);
#include "under_test.inc"
static void fake_destroy(SDL_AudioStream *s) {
 /* by now the clock already drives the mixer and the device is gated */
 close_saw_clock = audio_driver == AUDIO_DRIVER_CLOCK && clock_run;
 close_saw_gate = silence_gate;
 closes++;
 usleep(40000); /* a blocking close: the device callback keeps running meanwhile */
 __atomic_store_n(&s->stop, 1, __ATOMIC_RELEASE);
 pthread_join(s->thread, NULL);
 free(s);
}
static void wait_ms(int ms) { usleep(ms * 1000); }
int main(int argc, char **argv) {
 CHECK(argc == 2, "case name");
 const char *c = argv[1];
 if (!strcmp(c, "toggle-orders")) {
  unsigned seed = 7;
  audio_set_device(FALSE);
  for (int i = 0; i < 60; i++) {
   seed = seed * 1103515245u + 12345u;
   BOOL want = (seed >> 16) & 1;
   audio_set_device(want);
   CHECK(audio_driver == (want ? AUDIO_DRIVER_SDL : AUDIO_DRIVER_CLOCK), "driver follows the request");
   wait_ms(3);
  }
  CHECK(!overlap, "two drivers mixed at once");
  CHECK(!dual, "the device mixed while the clock still ran");
  CHECK(device_mixes > 0, "the device drove the mixer at some point");
  return 0;
 }
 if (!strcmp(c, "blocking-close")) {
  audio_set_device(TRUE); wait_ms(5);
  audio_set_device(FALSE);
  CHECK(closes == 1, "closed once");
  CHECK(close_saw_clock && close_saw_gate, "the clock ran and the device was gated before the blocking close");
  CHECK(!overlap, "no overlap across the close");
  return 0;
 }
 if (!strcmp(c, "no-gap")) {
  audio_set_device(FALSE); wait_ms(30);
  for (int i = 0; i < 10; i++) {
   int before = __atomic_load_n(&mixes, __ATOMIC_SEQ_CST);
   audio_set_device(i & 1 ? FALSE : TRUE);
   wait_ms(35);
   CHECK(__atomic_load_n(&mixes, __ATOMIC_SEQ_CST) > before + 2, "the mixer kept running through the toggle");
  }
  CHECK(!overlap, "no overlap");
  return 0;
 }
 if (!strcmp(c, "open-failure")) {
  audio_set_device(FALSE);
  open_fails = 1;
  audio_set_device(TRUE);
  CHECK(audio_driver == AUDIO_DRIVER_CLOCK, "stays on the clock when the device will not open");
  CHECK(!silence_gate, "gate cleared");
  int before = __atomic_load_n(&mixes, __ATOMIC_SEQ_CST);
  wait_ms(50);
  CHECK(__atomic_load_n(&mixes, __ATOMIC_SEQ_CST) > before, "the clock keeps draining voices");
  open_fails = 0;
  audio_set_device(TRUE);
  CHECK(audio_driver == AUDIO_DRIVER_SDL, "the next request opens it");
  CHECK(!overlap, "no overlap");
  return 0;
 }
 CHECK(0, "unknown case");
}
