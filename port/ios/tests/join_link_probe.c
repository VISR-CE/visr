/* halo://join links the app is opened with (host_join_link.h): only an invite's
   64 hex digits get through, written back as the game makes its links */
#include "host_join_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DIGITS "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

static void accepted(const char *url, const char *expected) {
    char invite[HOST_JOIN_LINK_SIZE];
    memset(invite, 'x', sizeof(invite));
    assert(host_join_link_parse(url, invite) == 1);
    assert(!strcmp(invite, expected));
}
static void refused(const char *url) {
    char invite[HOST_JOIN_LINK_SIZE] = "untouched";
    assert(host_join_link_parse(url, invite) == -1);
    assert(!strcmp(invite, "untouched"));
}
static void ignored(const char *url) {
    char invite[HOST_JOIN_LINK_SIZE] = "untouched";
    assert(host_join_link_parse(url, invite) == 0);
    assert(!strcmp(invite, "untouched"));
}
int main(void) {
    assert(HOST_JOIN_LINK_SIZE == 12 + 64 + 1);
    /* what p2p.c's make_invite writes, a trailing slash, and any case */
    accepted("halo://join/" DIGITS, "halo://join/" DIGITS);
    accepted("halo://join/" DIGITS "/", "halo://join/" DIGITS);
    accepted("HALO://Join/0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef",
        "halo://join/" DIGITS);
    accepted("HALO://JOIN/0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
        "halo://join/" DIGITS);
    /* Scanner text follows the same strict rules as pasted links. */
    refused("HALO://JOIN/" DIGITS "\n");
    refused("halo://join/" DIGITS "\r\n");
    ignored("ABCD-EFGH");
    /* other URLs and files: none of this code's business, and not logged */
    ignored(NULL);
    ignored("");
    ignored("hal");
    ignored("https://join/" DIGITS);
    ignored("/private/var/mobile/Documents/halo.iso");
    ignored("file:///halo://join/" DIGITS);
    ignored("halox://join/" DIGITS);
    /* halo: links that are not join links */
    refused("halo:");
    refused("halo://");
    refused("halo://join");
    refused("halo://join/");
    refused("halo:join/" DIGITS);
    refused("halo://host/" DIGITS);
    refused("halo://joinx/" DIGITS);
    refused("halo://user@join/" DIGITS);
    /* an older version's invite (the host's identifier alone: 44 digits), one digit short or long */
    refused("halo://join/0123456789abcdef0123456789abcdef0123456789ab");
    refused("halo://join/" "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde");
    refused("halo://join/" DIGITS "0");
    /* nothing after the digits but one slash: no query, fragment, path or second link */
    refused("halo://join/" DIGITS "?exec=1");
    refused("halo://join/" DIGITS "#x");
    refused("halo://join/" DIGITS "//");
    refused("halo://join/" DIGITS "/more");
    refused("halo://join/" DIGITS " ");
    refused("halo://join/" DIGITS "%0A");
    refused("halo://join/%30123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    refused("halo://join/0123456789abcdeg0123456789abcdef0123456789abcdef0123456789abcdef");
    refused("halo://join/ " DIGITS);
    /* no byte other than an ASCII letter folds onto the prefix ('\x0f' | 0x20 is '/') */
    refused("halo:\x0f/join/" DIGITS);
    ignored("halo\x1a//join/" DIGITS);
    puts("PASS: join links parse, and anything else in a halo: link is refused");
    return 0;
}
