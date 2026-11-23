#pragma once
#include <stddef.h>
/* Deactivates the audio session and notifies other apps (see host_audio_session.m). Call only with no sound device open. */
void host_audio_session_release(void);
/* Music off: when set, host_audio_session_apply makes the session Playback + MixWithOthers without ducking other apps. */
void host_audio_session_set_mix_only(int enabled);
/* Call right after SDL opens an audio device; a no-op unless mix-only is set and on platforms without a session. */
void host_audio_session_apply(void);
/* Writes what the system grants the sound device (IO buffer, output latency, rate, route port types on the session
   platforms; the default output device's buffer, latency and safety offset in frames on macOS) into out, for the logs. */
void host_audio_session_describe(char *out,size_t size);
