#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode*) next;
    Value buffer[RING_SIZE];
    _Atomic(long long) push_idx;
    _Atomic(long long) pop_idx;
};

RingsQueueNode* RingsQueueNode_new(void) {
    RingsQueueNode* new_node = (RingsQueueNode*)malloc(sizeof(RingsQueueNode));
    atomic_init(&new_node->next, NULL);
    for (int i = 0; i < RING_SIZE; i++) {
        new_node->buffer[i] = EMPTY_VALUE;
    }
    atomic_init(&new_node->push_idx, 0);
    atomic_init(&new_node->pop_idx, 0);

    return new_node;
}

struct RingsQueue {
    RingsQueueNode* head;
    RingsQueueNode* tail;
    pthread_mutex_t pop_mtx;
    pthread_mutex_t push_mtx;
};

//int tab[1000000][3];
//int expected = 1;
//int licznik = 0;
RingsQueue* RingsQueue_new(void)
{
    RingsQueue* queue = (RingsQueue*)malloc(sizeof(RingsQueue));
    RingsQueueNode* first_node = RingsQueueNode_new();
    queue->head = first_node;
    queue->tail = first_node;
    pthread_mutex_init(&queue->pop_mtx, NULL);
    pthread_mutex_init(&queue->push_mtx, NULL);

//    for (int i = 0; i < 1000000; i++) {
//        tab[i][0] = 0;
//        tab[i][1] = 0;
//        tab[i][2] = 0;
//    }

    return queue;
}

void RingsQueue_delete(RingsQueue* queue)
{
    // Przechodzimy po wszystkich node'ach i je usuwamy
    while (queue->head != NULL) {
        RingsQueueNode* next = atomic_load(&queue->head->next);
        free(queue->head);
        queue->head = next;
    }
    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);

    free(queue);
}

void RingsQueue_push(RingsQueue* queue, Value item)
{
    pthread_mutex_lock(&queue->push_mtx);
    RingsQueueNode* tail = queue->tail;
    assert(tail->push_idx - tail->pop_idx <= RING_SIZE);
    assert(tail->push_idx - tail->pop_idx >= 0);

    // nie ma miejsca
    if (atomic_load(&tail->push_idx) - atomic_load(&tail->pop_idx) == RING_SIZE) {
        RingsQueueNode* new_node = RingsQueueNode_new();
        atomic_store(&tail->next, new_node);
        queue->tail = new_node; // Nie trzeba martwić się, że drugi wątek nagle odczyta cały bufor i zrobi free node'a w którym jesteśmy, bo może to zrobić, dopiero jak będzie istniał następny node, a wtedy już w nim będziemy
    }

    tail = queue->tail;
    long long int push_idx = atomic_load(&tail->push_idx);
    tail->buffer[push_idx % RING_SIZE] = item;
    atomic_fetch_add(&tail->push_idx, 1);

//    tab[licznik][0] = item;
//    tab[licznik][1] = push_idx % RING_SIZE;
//    tab[licznik][2] = push_idx - tail->pop_idx;
//    licznik++;

    pthread_mutex_unlock(&queue->push_mtx);
}

Value RingsQueue_pop(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    assert(queue->head->push_idx - queue->head->pop_idx <= RING_SIZE); // TODO usunąć
    assert(queue->head->push_idx - queue->head->pop_idx >= 0); // TODO czasami assertion failed

    if (atomic_load(&queue->head->push_idx) - atomic_load(&queue->head->pop_idx) == 0){ // Węzeł jest pusty
        if (atomic_load(&queue->head->next) != NULL && queue->head != queue->tail && atomic_load(&queue->head->push_idx) - atomic_load(&queue->head->pop_idx) == 0) { // Ma następnika. // Przechodzimy dalej, tylko jak tail jest już w następnym węźle i u nas jest pusto
            RingsQueueNode* old_head = queue->head;
            queue->head = atomic_load(&queue->head->next);
            free(old_head);
        }
        else {
            pthread_mutex_unlock(&queue->pop_mtx);
            return EMPTY_VALUE;
        }
    }

    Value v = queue->head->buffer[atomic_load(&queue->head->pop_idx) % RING_SIZE];

//    if (v != EMPTY_VALUE && v != expected) {
//        printf("Zle jest %d, powinno byc %d; %d\n", v, expected, queue->head->pop_idx);
//        for (int i = 0; i < RING_SIZE; i++) {
//            printf("%d ", queue->head->buffer[i]);
//        }
//        printf("\n");
//        printf("\n");
//        printf("\n");
//        for (int i = 0; i < 1000000; i++) {
//            printf("item: %d, index: %d, zajęte miejsca: %d\n", tab[i][0], tab[i][1], tab[i][2]);
//        }
//
//        exit(1);
//    }
//    if (v != EMPTY_VALUE) {
//        expected++;
//    }

    atomic_fetch_add(&queue->head->pop_idx, 1); // TODO mutexy dookoła tego rozwiązują problem
    pthread_mutex_unlock(&queue->pop_mtx);

    return v;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    bool result = atomic_load(&queue->head->push_idx) - atomic_load(&queue->head->pop_idx) == 0 && atomic_load(&queue->head->next) == NULL;
    pthread_mutex_unlock(&queue->pop_mtx);

    return result;
}
