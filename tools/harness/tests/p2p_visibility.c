#include "harness.h"
static struct { int public, has_password, listed; } lobby;
#define pthread_mutex_lock(lock) ((void)0)
#define pthread_mutex_unlock(lock) ((void)0)
static void p2p_new_invite_if_listed(void) {}
/* Browser disablement must not override the selected visibility. */
static int config_boolean(char const *name) {(void)name;return FALSE;}
#include "under_test.inc"
int main(void)
{
 for(int visibility=0;visibility<2;visibility++)
  for(int password=0;password<2;password++)
   for(int transport=0;transport<2;transport++){
    p2p_set_hosting_public(visibility);lobby.has_password=password;lobby.listed=transport;
    CHECK(p2p_hosting_listed()==visibility,"chosen visibility changed with password or transport");
   }
 return 0;
}
