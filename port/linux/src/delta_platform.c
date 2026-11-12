#include "delta_platform.h"

int delta_platform_current(int mac, int tv, int vision)
{
	(void)mac; (void)tv; (void)vision;
#if defined(HALO_IOS)
	return mac ? _delta_platform_pc_macos : tv ? _delta_platform_tvos :
		vision ? _delta_platform_visionos : _delta_platform_ios;
#elif defined(__APPLE__)
	return _delta_platform_pc_macos;
#elif defined(__linux__) && !defined(HALO_ANDROID)
	return _delta_platform_pc_linux;
#else
	return _delta_platform_unknown;
#endif
}

void delta_platform_key(unsigned char key[DELTA_PLATFORM_KEY_SIZE], int platform,
	int opted_in, int dedicated, unsigned long memory_mb)
{
	unsigned char memory = 0;

	/* These are this port's policy rows, including the native visionOS row. */
	if (platform != _delta_platform_pc_macos && platform != _delta_platform_pc_linux &&
		platform != _delta_platform_ios && platform != _delta_platform_tvos && platform != _delta_platform_visionos)
		platform = _delta_platform_unknown;
	if (memory_mb)
	{
		while (memory_mb >>= 1)
			memory++;
	}
	else if (platform == _delta_platform_ios || platform == _delta_platform_tvos || platform == _delta_platform_visionos)
		memory = 11;
	else if (platform != _delta_platform_unknown)
		memory = 12;
	key[0] = (unsigned char)platform;
	key[1] = DELTA_PLATFORM_KEY_VERSION;
	key[2] = (unsigned char)((opted_in ? DELTA_PLATFORM_OPTED_IN : 0) |
		(dedicated ? DELTA_PLATFORM_DEDICATED : 0));
	key[3] = 128; key[4] = 128; key[5] = 128;
	key[6] = memory;
	key[DELTA_PLATFORM_CLIENT_ID_RESERVED_OFFSET] = 0;
}

const char *delta_platform_arch(void)
{
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
	return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
	return "x64";
#elif defined(__i386__) || defined(_M_IX86)
	return "x86";
#elif defined(__arm__) || defined(_M_ARM)
	return "arm";
#else
	return "";
#endif
}
