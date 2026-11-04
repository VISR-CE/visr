/*
MEMORY_WATCH_PAGE.H

The granularity of the guest memory write tracker (platform.h's memory_watch_*):
the size of the smallest range whose writes it can tell apart. Everything that
caches by pages of the contiguous window (the vertex mirror, texture caches,
render capture and validation) must use this size, not the Xbox's 4 KB: with a
smaller page, one write raises the generation of every neighbor in the tracker's
page, and the neighbors are rebuilt for nothing.

Apple's arm64 hosts protect memory in 16 KB pages (port/ios/host/host_memory.c).
The other hosts track 4 KB (port/linux/src/memory_watch.c,
port/windows/src/win32_memory_watch.c).
*/

#ifndef __MEMORY_WATCH_PAGE_H
#define __MEMORY_WATCH_PAGE_H

#if (defined(HALO_ILP32) && !defined(HALO_ANDROID)) || defined(__APPLE__)
#define MEMORY_WATCH_PAGE_SHIFT 14
#else
#define MEMORY_WATCH_PAGE_SHIFT 12
#endif
#define MEMORY_WATCH_PAGE_SIZE (1UL << MEMORY_WATCH_PAGE_SHIFT)

#endif
