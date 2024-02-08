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
        { "SimpleQueue", SimpleQueue_new, SimpleQueue_push, SimpleQueue_pop, SimpleQueue_is_empty, SimpleQueue_delete },
        { "RingsQueue", RingsQueue_new, RingsQueue_push, RingsQueue_pop, RingsQueue_is_empty, RingsQueue_delete },
        { "LLQueue", LLQueue_new, LLQueue_push, LLQueue_pop, LLQueue_is_empty, LLQueue_delete },
//        { "BLQueue", BLQueue_new, BLQueue_push, BLQueue_pop, BLQueue_is_empty, BLQueue_delete }
};

#pragma GCC diagnostic pop

#define THREADS 5
#define DATA_SIZE 100000

void* queue1;
void* queue2;
QueueVTable Q1;
QueueVTable Q2;
pthread_t threads[THREADS];
Value results[DATA_SIZE + 1];

bool reader_not_finished = true;
void* basic_test(void* thread_id)
{
    int id = *(int*)thread_id;
    free(thread_id);

    HazardPointer_register(id, THREADS);

    if (id == 3 || id == 4) {
        for (int i = 1; i <= DATA_SIZE; i++) {
            Q1.push(queue1, i);
        }
    }

    if (id == 1 || id == 2) {
        while (reader_not_finished) {
            Value v = Q1.pop(queue1);
            if (v != EMPTY_VALUE) {
                Q2.push(queue2, v);
            }
        }
    }

    if (id == 0) {
        for (int i = 0; i < DATA_SIZE * 2; i++) {
            Value v = Q2.pop(queue2);
            while (v == EMPTY_VALUE) {
                v = Q2.pop(queue2);
            }
            results[v]++;
        }
        reader_not_finished = false;

        for (int i = 1; i <= DATA_SIZE; i++) {
            printf("%d ", results[i]);
            assert(results[i] == 2);
        }
    }



    return NULL;
}

int main(void)
{
    for (int i = 0; i < sizeof(queueVTables) / sizeof(QueueVTable); ++i) {
        Q1 = queueVTables[i];
        Q2 = queueVTables[i];
        queue1 = Q1.new();
        queue2 = Q2.new();
        reader_not_finished = true;

        for (int j = 0; j < DATA_SIZE + 1; j++) {
            results[j] = 0;
        }

        for (int j = 0; j < THREADS; j++) {
            int* thread_id = malloc(sizeof(int));
            *thread_id = j;
            pthread_create(&threads[j], NULL, basic_test, thread_id);
        }

        for (int j = 0; j < THREADS; j++) {
            pthread_join(threads[j], NULL);
        }

        Q1.delete(queue1);
        Q2.delete(queue2);

        printf("Queue type: %s\n", Q1.name);
    }

    return 0;
}
