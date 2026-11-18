#pragma once
/* Deactivates the audio session and notifies other apps (see host_audio_session.m). Call only with no sound device open. */
void host_audio_session_release(void);
