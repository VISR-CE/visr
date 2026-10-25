/*
GL_FUNCTIONS.C

Run-time resolution of the OpenGL entry points listed in gl.h.
*/

/* the platform layer's (xbox_kernel.c; host_gpu.c in the iOS host) */
void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define GL_FUNCTIONS_DEFINE
#include "gl.h"

#include <SDL3/SDL.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#include <dlfcn.h>
/* Under Mac Catalyst desktop OpenGL (libGL.dylib) can be loaded too, and its
   glGetString comes first by name (macOS 26); take every entry point from the
   OpenGLES framework EAGL draws with. */
static void *gles_proc(const char *name)
{
	static void *gles;
	if (!gles)
		gles = dlopen("/System/iOSSupport/System/Library/Frameworks/OpenGLES.framework/OpenGLES", RTLD_LAZY | RTLD_NOLOAD | RTLD_FIRST);
	return gles ? dlsym(gles, name) : NULL;
}
#define HALO_GL_PROC(name) gles_proc(name)
#else
#define HALO_GL_PROC(name) SDL_GL_GetProcAddress(name)
#endif

#ifdef GPU_GL_HOST
/* port/ios/host/host.h */
void host_fatal(const char *format, ...) __attribute__((format(printf, 1, 2), noreturn));

/* the ES 3.1 and 3.2 entry points, which gpu_initialize's capability probe leaves
unused when the context lacks them (Apple's ES 3.0 does); without any other,
the backend cannot run, so it stops here with the name rather than crashing
at the first call */
static int gl_function_optional(const char *name)
{
	return !SDL_strcmp(name, "glCopyImageSubData") || !SDL_strcmp(name, "glDrawElementsBaseVertex") ||
		!SDL_strcmp(name, "glMemoryBarrier");
}

#define GL_FUNCTION_MISSING(name) \
	if (!gl_function_optional(name)) \
		host_fatal("OpenGL ES entry point unavailable: %s", name)
#else
#define GL_FUNCTION_MISSING(name) success = 0
#endif

#define GL_DEFINE_FUNCTION(name) __typeof__(&name) halo_##name;
GL_FUNCTIONS(GL_DEFINE_FUNCTION)
unsigned long halo_gl_call_count;

void halo_gl_count_call(void)
{
	halo_gl_call_count++;
}

int gl_functions_load(void)
{
	int success = 1;

#define GL_LOAD_FUNCTION(name) \
	halo_##name = (__typeof__(halo_##name))HALO_GL_PROC(#name); \
	if (!halo_##name) \
	{ \
		platform_log("OpenGL function %s is unavailable", #name); \
		GL_FUNCTION_MISSING(#name); \
	}
	GL_FUNCTIONS(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION
	return success;
}
