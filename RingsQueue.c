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
    _Atomic(int) push_idx; // TODO trzeba korzystac z atomic funkcji
    _Atomic(int) pop_idx;
};

RingsQueueNode* RingsQueueNode_new(void) {
    RingsQueueNode* node = (RingsQueueNode*)malloc(sizeof(RingsQueueNode));
    atomic_init(&node->next, NULL);
    atomic_init(&node->push_idx, 0);
    atomic_init(&node->pop_idx, 0);

    for (int i = 0; i < RING_SIZE; i++) {
        node->buffer[i] = EMPTY_VALUE;
    }

    return node;
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
    // Nie potrzeba strażnika, bo po prostu będzie pusty bufor
    RingsQueueNode* new_node = RingsQueueNode_new();
    queue->head = new_node;
    queue->tail = new_node;
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

/*
->Jeśli węzeł wskazany przez tail nie jest pełny,
dodaje do jego bufora cyklicznego nową wartość,
zwiększając przy tym push_idx.
->Jeśli zaś jest pełny, to tworzy nowy węzeł i aktualizuje
 odpowiednie wskaźniki.
 */
void RingsQueue_push(RingsQueue* queue, Value item)
{
    pthread_mutex_lock(&queue->push_mtx);

    // Jeśli nie ma miejsca, to tworzymy nowy węzeł i aktualizujemy wskaźniki
    assert(queue->tail->push_idx - queue->tail->pop_idx <= RING_SIZE);
    if (queue->tail->push_idx - queue->tail->pop_idx == RING_SIZE) {
        RingsQueueNode* new_node = RingsQueueNode_new();
        atomic_store(&queue->tail->next, new_node);
        queue->tail = new_node;
    }

    queue->tail->buffer[(queue->tail->push_idx) % RING_SIZE] = item;
    atomic_fetch_add(&queue->tail->push_idx, 1);

    pthread_mutex_unlock(&queue->push_mtx);
}

/*
->jeśli węzeł wskazany przez head nie jest pusty,
 to zwraca wartość z jego bufora cyklicznego, zwiększając przy tym pop_idx.
->jeśli węzeł ten jest pusty i ma następnika:
 aktualizuje head na następny węzeł i zwraca wartość z jego bufora cyklicznego.
->jeśli węzeł ten jest pusty i nie ma następnika: zwraca EMPTY_VALUE.
 */
Value RingsQueue_pop(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);

    assert(queue->head->push_idx - queue->head->pop_idx >= 0);

    // Jak w head nic nie ma, to przesuwamy się dalej, o ile się da
    if (queue->head->push_idx - queue->head->pop_idx == 0) {
        if (atomic_load(&queue->head->next) != NULL) {
            RingsQueueNode *old_head = queue->head;
            queue->head = atomic_load(&queue->head->next);
            free(old_head);
        } else {
            pthread_mutex_unlock(&queue->pop_mtx);
            return EMPTY_VALUE;
        }
    }

    // W tym miejscu w buforze jest coś do odczytania
    Value value = queue->head->buffer[(queue->head->pop_idx) % RING_SIZE];
    atomic_fetch_add(&queue->head->pop_idx, 1);
    pthread_mutex_unlock(&queue->pop_mtx);
    return value;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{
    bool result = false;
    pthread_mutex_lock(&queue->pop_mtx);

    // Head jest pusty i nie ma następnika
    if (queue->head->push_idx - queue->head->pop_idx == 0 &&
            atomic_load(&queue->head->next) == NULL) {
        result = true;
    }

    pthread_mutex_unlock(&queue->pop_mtx);

    return result;
}
