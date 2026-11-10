/* Process-constant platform values captured before the guest starts. Display state
   used during play (rotation, screen migration and pacing) must remain live. */
#pragma once

struct host_startup_info {
    double screen_scale;
    double pixel_width, pixel_height;
    double maximum_frames_per_second;
    int phone;
    char device_model[256];
    char documents_root[4096];
    char caches_root[4096];
    char application_support_root[4096];
};

/* Call on main before starting the guest. Repeated calls preserve the first capture. */
void host_startup_capture(void);
/* Before capture, reads live on main into caller-thread storage, valid until that
   thread's next call. After capture the returned process snapshot is immutable. */
const struct host_startup_info *host_startup_get(void);
/* Platform boundary, implemented in host_startup_platform.m. */
void host_startup_read_live(struct host_startup_info *info);
