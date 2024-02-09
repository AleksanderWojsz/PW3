#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode*) AtomicBLNodePtr;

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffer[BUFFER_SIZE];
    _Atomic(long long int) push_idx;
    _Atomic(long long int) pop_idx;
};

BLNode* BLNode_new(void) {
    BLNode* new_node = (BLNode*)malloc(sizeof(BLNode));
    atomic_store(&new_node->next, NULL);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        atomic_store(&new_node->buffer[i], EMPTY_VALUE);
    }
    atomic_store(&new_node->push_idx, 0);
    atomic_store(&new_node->pop_idx, 0);

    return new_node;
}

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
};

BLQueue* BLQueue_new(void)
{
    BLQueue* queue = (BLQueue*)malloc(sizeof(BLQueue));
    BLNode* first_node = BLNode_new();
    atomic_store(&queue->head, first_node);
    atomic_store(&queue->tail, first_node);
    HazardPointer_initialize(&queue->hp);

    return queue;
}

void BLQueue_delete(BLQueue* queue)
{
    while (queue->head != NULL) {
        BLNode* next = atomic_load(&queue->head->next);
        free(queue->head); // Tutaj free, bo już nie ma wielowątkowości
        queue->head = next;
    }

    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void BLQueue_push(BLQueue* queue, Value item)
{
    while (true) {
        BLNode* tail = atomic_load(&queue->tail);
        _Atomic(long long int) push_idx_before_inc = atomic_fetch_add(&tail->push_idx, 1);

        if (push_idx_before_inc < BUFFER_SIZE) { // jest miejsca w buforze
            Value expected_empty = EMPTY_VALUE;
            if (atomic_compare_exchange_strong(&tail->buffer[push_idx_before_inc], &expected_empty, item)) { // spodziewamy się EMPTY_VALUE
                return;
            }
        }
        else { // bufor jest pełny
            if (tail->next != NULL) { // następny węzeł już istnieje
                atomic_compare_exchange_strong(&queue->tail, &tail, tail->next);
            }
            else { // trzeba utworzyć nowy węzeł
                BLNode* new_node = BLNode_new();
                new_node->buffer[0] = item;
                atomic_fetch_add(&new_node->push_idx, 1);
                BLNode* expected_null = NULL;
                if (atomic_compare_exchange_strong(&tail->next, &expected_null, new_node)) {
                    atomic_compare_exchange_strong(&queue->tail, &tail, new_node);
                    return;
                }
                else {
                    free(new_node);
                }
            }
        }
    }
}


Value BLQueue_pop(BLQueue* queue)
{
    while (true) {
        BLNode* head = atomic_load(&queue->head);
        _Atomic(long long int) pop_idx_before_inc = atomic_fetch_add(&head->pop_idx, 1);

        if (pop_idx_before_inc < BUFFER_SIZE) { // Potencjalnie coś jeszcze może być

            while (true) {
                Value v = atomic_load(&head->buffer[pop_idx_before_inc]);
                if (atomic_compare_exchange_strong(&head->buffer[pop_idx_before_inc], &v, TAKEN_VALUE)) {
                    if (v != EMPTY_VALUE) {
                        return v;
                    }
                    else {
                        return EMPTY_VALUE; // TODO
                    }
                }
            }
        }
        else { // W buforze nic nie ma
            if (head->next != NULL) { // Przechodzimy do następnego węzła
                atomic_compare_exchange_strong(&queue->head, &head, head->next);
            } else {
                return EMPTY_VALUE;
            }
        }
    }
}

bool BLQueue_is_empty(BLQueue* queue)
{
    BLNode* head = atomic_load(&queue->head);
    _Atomic(long long int) pop_idx= atomic_load(&head->pop_idx);

    if (pop_idx < BUFFER_SIZE) {
        if (atomic_load(&head->buffer[pop_idx]) == EMPTY_VALUE) {
            return true;
        }
    }
    else { // W buforze nic nie ma
        if (head->next == NULL) {
            return true;
        }
    }

    return false;
}
