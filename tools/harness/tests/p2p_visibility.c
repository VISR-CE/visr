#include "harness.h"
static struct { int public, has_password, listed; } lobby;
#define pthread_mutex_lock(lock) ((void)0)
#define pthread_mutex_unlock(lock) ((void)0)
static void p2p_new_invite_if_listed(void) {}
static int browser_enabled;
static int config_boolean(char const *name)
{
 CHECK(!strcmp(name,"network.public_lobby"),"wrong visibility key");
 return browser_enabled;
}
#include "under_test.inc"
int main(void)
{
 for(browser_enabled=0;browser_enabled<2;browser_enabled++)
  for(int visibility=0;visibility<2;visibility++)
   for(int password=0;password<2;password++)
    for(int transport=0;transport<2;transport++){
     p2p_set_hosting_public(visibility);lobby.has_password=password;lobby.listed=transport;
     CHECK(p2p_hosting_listed()==(visibility&&browser_enabled),"visibility %d/browser %d/password %d/transport %d",visibility,browser_enabled,password,transport);
    }
 return 0;
}
