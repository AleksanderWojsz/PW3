#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "HazardPointer.h"
#include "LLQueue.h"

typedef struct LLNode LLNode;
typedef _Atomic(LLNode*) AtomicLLNodePtr;

struct LLNode {
    AtomicLLNodePtr next;
    Value value;
};


LLNode* LLNode_new(Value item) {
    LLNode* node = (LLNode*)malloc(sizeof(LLNode));
    atomic_store(&node->next, NULL);
    node->value = item;
    return node;
}

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
    _Atomic (uint64_t) head_counter;
};


LLQueue* LLQueue_new(void) {
    LLQueue* queue = (LLQueue*)malloc(sizeof(LLQueue));
    LLNode* guard_node = LLNode_new(EMPTY_VALUE);
    atomic_store(&queue->head, guard_node);
    atomic_store(&queue->tail, guard_node);
    atomic_store(&queue->head_counter, 0);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void LLQueue_delete(LLQueue* queue) {

    while (atomic_load(&queue->head) != NULL) {
        LLNode* head = atomic_load(&queue->head);
        LLNode* next = atomic_load(&head->next);
        free(atomic_load(&queue->head)); // Tutaj free, bo już nie ma wielowątkowości
        atomic_store(&queue->head, next);
    }

    HazardPointer_finalize(&queue->hp);
    free(queue);
}

void LLQueue_push(LLQueue* queue, Value item) {

    LLNode* new_node = LLNode_new(item);
    while (true) {
        LLNode* tail = atomic_load(&queue->tail);
        HazardPointer_protect(&queue->hp, (void*)&tail);
        if (atomic_load(&queue->tail) != tail) { // Żeby tail był usunięty, to musi być równy head'owi, ale wtedy head nie ma następnika, więc nie będzie usuwał. Czyli tail nie może zostać usunięty, więc wystarczy wiedzieć, że nasz tail jest aktualny.
            continue;
        }

        // Spodziewamy się, że następny node to null
        LLNode* expected_null = NULL;
        if (atomic_compare_exchange_strong(&tail->next, &expected_null, new_node)) {
            atomic_compare_exchange_strong(&queue->tail, &tail, new_node);
            HazardPointer_clear(&queue->hp);
            return;
        } else {
            LLNode* next = atomic_load(&tail->next);
            atomic_compare_exchange_strong(&queue->tail, &tail, next); // Potrzebne jakby wątek, który właśnie dodał node'a poszedł spać na długo i nie zdążył przestawić tail
        }

    }
}


Value LLQueue_pop(LLQueue* queue) {

    while (true) {
        LLNode* head = atomic_load(&queue->head);
        HazardPointer_protect(&queue->hp, (void*)&head);
        if (atomic_load(&queue->head) != head) { // Żeby node został zwolniony, to head najpierw musi być przesunięty
            continue;
        }
        LLNode* next = atomic_load(&head->next); // Bezpiecznie czytamy next
        if (next == NULL) {
            return EMPTY_VALUE;
        }
        uint64_t prev_head_counter = atomic_load(&queue->head_counter);
        HazardPointer_protect(&queue->hp, (void*)&next);
        if (atomic_load(&queue->head) != head || prev_head_counter != atomic_load(&queue->head_counter)) { // ABA - nowa głowa może mieć taki sam adres w pamięci jak poprzednia, stąd licznik
            continue;
        }
        Value value = atomic_load(&next->value); // Jeżeli głowa się nie przesunęła, to next nie mógł być zwolniony, stąd w tym miejscu jest już zabezpieczony.
        HazardPointer_clear(&queue->hp);


        if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
            atomic_fetch_add(&queue->head_counter, 1);

            HazardPointer_retire(&queue->hp, head);
            return value;
        }
    }
}


// Jeśli w kolejce jest tylko strażnik, to jest pusta
bool LLQueue_is_empty(LLQueue* queue) {

    while (true) {
        LLNode* head = atomic_load(&queue->head);
        HazardPointer_protect(&queue->hp, (void*)&head);
        if (atomic_load(&queue->head) != head) { // Żeby node został zwolniony, to head najpierw musi być przesunięty
            continue;
        }
        bool result = (atomic_load(&head->next) == NULL);
        HazardPointer_clear(&queue->hp);
        return result;
    }
}
