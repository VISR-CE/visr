#include "host_main_thread.h"
#include <pthread.h>

int host_on_main_thread(void) {
    return pthread_main_np();
}
void host_main_async(dispatch_block_t block) {
    if (host_on_main_thread()) block();
    else dispatch_async(dispatch_get_main_queue(), block);
}
void host_main_sync(dispatch_block_t block) {
    if (host_on_main_thread()) block();
    else dispatch_sync(dispatch_get_main_queue(), block);
}
void host_main_async_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else dispatch_async_f(dispatch_get_main_queue(), context, fn);
}
void host_main_sync_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else dispatch_sync_f(dispatch_get_main_queue(), context, fn);
}
