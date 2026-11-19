#ifndef HOST_GEOMETRY_H
#define HOST_GEOMETRY_H
#ifdef __cplusplus
extern "C" {
#endif
/* host_orientation.m publishes after 100 ms without a geometry change.
   Poll on the main thread. Accessors are safe to read from any thread. */
void host_geometry_poll(void);
unsigned host_geometry_settled_generation(void);
/* Last settled size in scene points, and its generation from the same copy.
   Zero size and generation mean that nothing has settled yet. */
unsigned host_geometry_settled_size(double *width, double *height);
#ifdef __cplusplus
}
#endif
#endif
