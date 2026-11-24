/* halo_sim_uninitialized.h: opting one local out of automatic zeroing

The native builds zero every automatic variable declared without an
initializer (configure.py --guest-auto-init, -ftrivial-auto-var-init), so a
read of a local's unwritten bytes gives zero on every run and every build
instead of whatever the stack held. A few large locals on hot paths cost
measurable time to clear and are always written before they are read; such a
local is declared SIM_UNINITIALIZED, with a comment naming the measurement.
tools/test_sim_uninitialized.py holds the reviewed list of them.

In a pattern build (--guest-auto-init=pattern, the determinism check) the
macro is empty, so the check fills the opted-out locals too and a read of one
before it is written shows up there. */

#ifndef HALO_SIM_UNINITIALIZED_H
#define HALO_SIM_UNINITIALIZED_H

#if defined(HALO_AUTO_INIT_PATTERN) || !defined(__has_attribute)
#define SIM_UNINITIALIZED
#elif __has_attribute(uninitialized)
#define SIM_UNINITIALIZED __attribute__((uninitialized))
#else
#define SIM_UNINITIALIZED
#endif

#endif
