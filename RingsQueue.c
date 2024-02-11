#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>

#include <stdio.h>
#include <stdlib.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

struct RingsQueueNode {
    _Atomic(RingsQueueNode*) next;
    Value buffer[RING_SIZE];
    _Atomic(int64_t) push_idx;
    _Atomic(int64_t) pop_idx;
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

RingsQueue* RingsQueue_new(void)
{
    RingsQueue* queue = (RingsQueue*)malloc(sizeof(RingsQueue));
    RingsQueueNode* first_node = RingsQueueNode_new();
    queue->head = first_node;
    queue->tail = first_node;
    pthread_mutex_init(&queue->pop_mtx, NULL);
    pthread_mutex_init(&queue->push_mtx, NULL);

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

    if (atomic_load(&tail->push_idx) - atomic_load(&tail->pop_idx) >= RING_SIZE) { // Nie ma miejsca
        RingsQueueNode* new_node = RingsQueueNode_new();
        atomic_fetch_add(&new_node->push_idx, 1);
        new_node->buffer[0] = item;
        atomic_store(&tail->next, new_node);
        queue->tail = new_node; // Nie trzeba martwić się, że drugi wątek nagle odczyta cały bufor i zrobi free node'a w którym jesteśmy, bo może to zrobić, dopiero jak będzie istniał następny node, a wtedy już w nim będziemy
    }
    else {
        tail->buffer[atomic_load(&tail->push_idx) % RING_SIZE] = item;
        atomic_fetch_add(&tail->push_idx, 1);
    }

    pthread_mutex_unlock(&queue->push_mtx);
}

Value RingsQueue_pop(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);

    RingsQueueNode* head = queue->head;
    if (atomic_load(&head->push_idx) - atomic_load(&head->pop_idx) <= 0){ // Węzeł jest pusty
        if (atomic_load(&head->next) != NULL && head != queue->tail && atomic_load(&head->push_idx) - atomic_load(&head->pop_idx) <= 0) { // Ma następnika. // Przechodzimy dalej, tylko jak tail jest już w następnym węźle i u nas jest pusto
            queue->head = atomic_load(&head->next);
            free(head);
        }
        else {
            pthread_mutex_unlock(&queue->pop_mtx);
            return EMPTY_VALUE;
        }
    }

    head = queue->head;
    Value v = head->buffer[atomic_load(&head->pop_idx) % RING_SIZE];

    atomic_fetch_add(&head->pop_idx, 1);
    pthread_mutex_unlock(&queue->pop_mtx);

    return v;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    RingsQueueNode* head = queue->head;
    bool result = atomic_load(&head->push_idx) - atomic_load(&head->pop_idx) <= 0 && atomic_load(&head->next) == NULL;
    pthread_mutex_unlock(&queue->pop_mtx);

    return result;
}
