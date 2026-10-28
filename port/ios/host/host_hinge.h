#ifndef HOST_HINGE_H
#define HOST_HINGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum host_hinge_status {
    HOST_HINGE_UNKNOWN,
    HOST_HINGE_CLOSED,
    HOST_HINGE_PARTIALLY_OPEN,
    HOST_HINGE_FULLY_OPEN
};

struct host_hinge_snapshot {
    int available;
    enum host_hinge_status status;
    double angle; /* Radians. UIKit controls update frequency and precision. */
    int division_active;
    /* Points in the game's root view, including UIKit's interactive margins. */
    double division_x, division_y, division_width, division_height;
    unsigned generation;
};

/* Attach after SDL creates the game's root view. Attach and poll on the main
   thread; poll also catches division-frame changes from rotation and resizing. */
void host_hinge_attach(void *root_view);
void host_hinge_poll(void);
/* A coherent copy for the touch overlay, safe to read from any thread.
   Unsupported SDKs, platforms and devices report an unavailable zero state. */
void host_hinge_snapshot(struct host_hinge_snapshot *out);

#ifdef __cplusplus
}
#endif
#endif
