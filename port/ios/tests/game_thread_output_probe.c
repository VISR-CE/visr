/* Run the production request mailbox and external snapshot readers without UIKit
   or Metal. The writer stands in for scene callbacks on the main thread. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include "host_external.h"
#include "host_output_sync.h"

#define TARGET_OS_MACCATALYST 0
static struct host_external_snapshot snapshot;
static pthread_mutex_t snapshot_lock=PTHREAD_MUTEX_INITIALIZER;
static _Atomic unsigned long long published_size;
static int companion_on,output_width,output_height;
static int enabled=1;
static int host_game_thread_enabled(void) { return enabled; }
static int host_on_main_thread(void) { return 0; }
#include "game_thread_output.inc"

static _Atomic int done;
static void *scene_writer(void *unused)
{
    (void)unused;
    for(int i=1;i<=100000;i++) {
        companion_on=i%3!=0;output_width=i;output_height=i*2;
        publish_size();
        pthread_mutex_lock(&snapshot_lock);
        snapshot.external=i%2;
        pthread_mutex_unlock(&snapshot_lock);
    }
    atomic_store(&done,1);
    return NULL;
}
static void *cap_writer(void *context)
{
    host_output_request_set(context,720);
    return NULL;
}
struct fence_state { int held,mutations,released; };
static void slot_wait(void *context) {
    struct fence_state *state=context;
    assert(!state->mutations && !state->released);
    state->held++;
}
static void layer_mutate(void *context) {
    struct fence_state *state=context;
    assert(state->held==3 && state->released==0);
    state->mutations++;
}
static void slot_release(void *context) {
    struct fence_state *state=context;
    assert(state->held>0 && state->mutations==1);
    state->held--;state->released++;
}
int main(void)
{
    struct fence_state state={0};
    host_output_fence(&state,3,slot_wait,layer_mutate,slot_release);
    assert(state.held==0 && state.released==3 && state.mutations==1);
    struct host_output_request request=HOST_OUTPUT_REQUEST_INIT;
    assert(host_output_request_cap(host_output_request_read(&request))==-1);
    for(int i=0;i<3;i++) {
        host_output_request_set(&request,720);
        uint64_t drained=host_output_request_read(&request);
        pthread_t writer;
        assert(!pthread_create(&writer,NULL,cap_writer,&request));
        assert(!pthread_join(writer,NULL));
        uint64_t newer=host_output_request_read(&request);
        assert(newer!=drained);
        host_output_request_complete(&request,drained);
        assert(host_output_request_read(&request)==newer);
        host_output_request_set(&request,-1);
        host_output_request_complete(&request,newer);
        assert(host_output_request_cap(host_output_request_read(&request))==-1);
    }
    host_output_request_set(&request,0);
    uint64_t ticket=host_output_request_read(&request);
    assert(host_output_request_cap(ticket)==0);
    host_output_request_complete(&request,ticket);
    assert(host_output_request_cap(host_output_request_read(&request))==-1);
    pthread_t writer;
    assert(!pthread_create(&writer,NULL,scene_writer,NULL));
    unsigned reads=0;
    do {
        int width=-1,height=-1;
        if(host_external_size(&width,&height))assert(height==width*2);
        else assert(width==-1 && height==-1);
        int active=host_external_active();
        assert(active==0 || active==1);
        reads++;
    } while(!atomic_load(&done));
    assert(!pthread_join(writer,NULL));
    enabled=0;companion_on=1;output_width=640;output_height=480;
    int width,height;
    assert(host_external_size(&width,&height) && width==640 && height==480);
    assert(host_external_active()==snapshot.external);
    printf("PASS: drain-before-mutation, output request generations, cancellation, coherent sizes, locked activity (%u reads)\n",reads);
    return 0;
}
