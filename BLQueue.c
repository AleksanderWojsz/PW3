#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
        HazardPointer_protect(&queue->hp, &tail);
        if (atomic_load(&queue->tail) != tail) {
            continue;
        }
        _Atomic(long long int) push_idx_before_inc = atomic_fetch_add(&tail->push_idx, 1);

        if (push_idx_before_inc < BUFFER_SIZE) { // jest miejsca w buforze
            Value expected_empty = EMPTY_VALUE;
            if (atomic_compare_exchange_strong(&tail->buffer[push_idx_before_inc], &expected_empty, item)) { // spodziewamy się EMPTY_VALUE
                HazardPointer_clear(&queue->hp);
                return;
            }
        }
        else { // bufor jest pełny
            if (tail->next != NULL) { // następny węzeł już istnieje
                BLNode* next = tail->next;
                atomic_compare_exchange_strong(&queue->tail, &tail, next);
            }
            else { // trzeba utworzyć nowy węzeł
                BLNode* new_node = BLNode_new();
                new_node->buffer[0] = item;
                atomic_fetch_add(&new_node->push_idx, 1);
                BLNode* expected_null = NULL;
                if (atomic_compare_exchange_strong(&tail->next, &expected_null, new_node)) {
                    atomic_compare_exchange_strong(&queue->tail, &tail, new_node);
                    HazardPointer_clear(&queue->hp);
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
        HazardPointer_protect(&queue->hp, &head);
        if (atomic_load(&queue->head) != head) {
            continue;
        }

        _Atomic(long long int) pop_idx_before_inc = atomic_fetch_add(&head->pop_idx, 1);

        if (pop_idx_before_inc < BUFFER_SIZE) { // Potencjalnie coś jeszcze może być

            while (true) { // Jak wartość się zmieniła to będziemy pobierali ją ponownie
                Value v = atomic_load(&head->buffer[pop_idx_before_inc]);
                if (atomic_compare_exchange_strong(&head->buffer[pop_idx_before_inc], &v, TAKEN_VALUE)) {
                    if (v != EMPTY_VALUE) {
                        HazardPointer_clear(&queue->hp);
                        return v;
                    }
                    else {
                        // Jak pobraliśmy EMPTY_VALUE, to nic nie wiemy, bo może proces wstawiający na to pole był powolny (nie zdążył wstawić, ale zarezerwował miejsce),
                        // ale następne pole ma już jakąś zawartość (wstawiał na nie jakiś szybszy proces, nawet mogliśmy to być my)
                        break; // Sprawdzamy kolejne pola
                    }
                }
            }
        }
        else { // W buforze nic nie ma
            if (head->next != NULL) { // Przechodzimy do następnego węzła
                BLNode* next = head->next;
                if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                    HazardPointer_retire(&queue->hp, head);
                }
            } else {
                HazardPointer_clear(&queue->hp);
                return EMPTY_VALUE;
            }
        }
    }
}

bool BLQueue_is_empty(BLQueue* queue)
{
    while (true) {
        BLNode* head = atomic_load(&queue->head);
        HazardPointer_protect(&queue->hp, &head);
        if (atomic_load(&queue->head) != head) {
            continue;
        }
        _Atomic(long long int) pop_idx= atomic_load(&head->pop_idx);

        // Czy coś jest w buforze
        for (int64_t i = pop_idx; i < BUFFER_SIZE; i++) {
            Value v = atomic_load(&head->buffer[i]);
            if (v != EMPTY_VALUE && v != TAKEN_VALUE) {
                return false;
            }
        }

        bool result = (pop_idx >= BUFFER_SIZE && head->next == NULL);

        HazardPointer_clear(&queue->hp);
        return result;
    }
}
