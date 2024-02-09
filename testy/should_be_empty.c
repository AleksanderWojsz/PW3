#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <assert.h>

#include "../SimpleQueue.h"
#include "../RingsQueue.h"
#include "../BLQueue.h"
#include "../LLQueue.h"
#include "../HazardPointer.h"

// A structure holding function pointers to methods of some queue type.
struct QueueVTable {
    const char* name;
    void* (*new)(void);
    void (*push)(void* queue, Value item);
    Value (*pop)(void* queue);
    bool (*is_empty)(void* queue);
    void (*delete)(void* queue);
};
typedef struct QueueVTable QueueVTable;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

const QueueVTable queueVTables[] = {
//        { "SimpleQueue", SimpleQueue_new, SimpleQueue_push, SimpleQueue_pop, SimpleQueue_is_empty, SimpleQueue_delete },
//        { "RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete },
//        { "LLQueue", LLQueue_new, LLQueue_push, LLQueue_pop, LLQueue_is_empty, LLQueue_delete },
        { "BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete }
};

#pragma GCC diagnostic pop

#define THREADS 16
#define DATA_SIZE 100

void* queue;
QueueVTable Q;
pthread_t threads[THREADS];
Value results[THREADS][DATA_SIZE];


void* basic_test(void* thread_id)
{
    int id = *(int*)thread_id;
    free(thread_id);
    HazardPointer_register(id, THREADS);



    for (int i = 0; i < DATA_SIZE; i++) {
        assert(Q.is_empty(queue) == true);
    }


    return NULL;
}

int main(void)
{
    for (int i = 0; i < sizeof(queueVTables) / sizeof(QueueVTable); ++i) {
        Q = queueVTables[i];
        queue = Q.new();

        for (int j = 0; j < THREADS; j++) {
            int* thread_id = malloc(sizeof(int));
            *thread_id = j;
            pthread_create(&threads[j], NULL, basic_test, thread_id);
        }

        for (int j = 0; j < THREADS; j++) {
            pthread_join(threads[j], NULL);
        }

        Q.delete(queue);

        printf("Queue type: %s\n", Q.name);
    }

    return 0;
}

