#include "halo_direct_ip.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct halo_direct_target target;
	const unsigned char expected[] = { 192, 0, 2, 7 };
	assert(halo_direct_ip_parse("192.0.2.7", "2400", &target));
	assert(!memcmp(&target.address, expected, 4) && target.port == 2400);
	assert(halo_direct_ip_parse("192.0.2.7:2500", "2400", &target) && target.port == 2500);
	assert(halo_direct_ip_parse("192.0.2.7", "", &target) && target.port == 2302);
	assert(halo_direct_ip_parse("192.0.2.7", "invalid", &target) && target.port == 2302);
	assert(halo_direct_ip_parse(" \t192.0.2.7:65535\r\n", "", &target) && target.port == 65535);
	assert(halo_direct_ip_parse("192.0.2.7", " \t1\r\n", &target) && target.port == 1);
	assert(!halo_direct_ip_parse("192.0.2.256", "2302", &target));
	assert(!halo_direct_ip_parse("", "2302", &target));
	assert(!halo_direct_ip_parse(" \t\r\n", "2302", &target));
	assert(!halo_direct_ip_parse("192.0.2.7", "0", &target));
	assert(!halo_direct_ip_parse("192.0.2.7", "70000", &target));
	assert(!halo_direct_ip_parse("192.0.2.7:0", "2302", &target));
	assert(!halo_direct_ip_parse("192.0.2.7:70000", "2302", &target));
	assert(!halo_direct_ip_parse("192.0.2.7x", "2302", &target));
	assert(!halo_direct_ip_parse("192.0.2", "2302", &target));
	assert(!halo_direct_ip_parse("0.0.0.0", "2302", &target));
	assert(!halo_direct_ip_parse("255.255.255.255", "2302", &target));
	assert(!halo_direct_ip_parse("224.0.0.1", "2302", &target));
	puts("PASS: direct IP address, port, defaults, bounds, and whitespace");
	return 0;
}
