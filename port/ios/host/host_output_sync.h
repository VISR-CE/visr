/* Output requests can arrive while the game drains its frames. A ticket includes
   a generation so completing an older request cannot erase a newer one, even
   when both requests have the same cap or the newer request cancels it. */
#pragma once
#include <stdatomic.h>
#include <stdint.h>

struct host_output_request { _Atomic uint64_t ticket; };
#define HOST_OUTPUT_REQUEST_INIT { ATOMIC_VAR_INIT(UINT32_MAX) }

static inline void host_output_request_set(struct host_output_request *request, int cap)
{
    uint64_t old=atomic_load(&request->ticket), next;
    do {
        next=((old>>32)+1)<<32 | (uint32_t)(cap<0?-1:cap);
    } while(!atomic_compare_exchange_weak(&request->ticket,&old,next));
}
static inline uint64_t host_output_request_read(struct host_output_request *request)
{
    return atomic_load(&request->ticket);
}
static inline int host_output_request_cap(uint64_t ticket)
{
    return (int32_t)(uint32_t)ticket;
}
static inline void host_output_request_complete(struct host_output_request *request,uint64_t ticket)
{
    atomic_compare_exchange_strong(&request->ticket,&ticket,(ticket & UINT64_C(0xffffffff00000000))|UINT32_MAX);
}

/* The game owns all slots before dispatching the mutation to main. Completion
   handlers release slots without calling main or waiting on the game thread. */
static inline void host_output_fence(void *context,int slots,
    void (*wait)(void *),void (*mutate)(void *),void (*release)(void *))
{
    for(int slot=0;slot<slots;slot++)wait(context);
    mutate(context);
    for(int slot=0;slot<slots;slot++)release(context);
}
