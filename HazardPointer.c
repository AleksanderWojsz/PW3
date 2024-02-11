#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "HazardPointer.h"

thread_local int _thread_id = -1;
int _num_threads = -1;

void HazardPointer_register(int thread_id, int num_threads)
{
    _thread_id = thread_id;
    _num_threads = num_threads;
}

void HazardPointer_initialize(HazardPointer* hp) {
    for (int i = 0; i < MAX_THREADS; i++) {
        atomic_store(&hp->pointer[i], NULL);
        hp->retired_count[i] = 0;
    }

    // Tablica wskaźników do usunięcia
    hp->retired = malloc(MAX_THREADS * sizeof(void*));
    for (int i = 0; i < MAX_THREADS; i++) {
        hp->retired[i] = malloc(RETIRED_THRESHOLD * sizeof(void*));
        for (int j = 0; j < RETIRED_THRESHOLD; j++) {
            hp->retired[i][j] = NULL;
        }
    }
}

void HazardPointer_finalize(HazardPointer* hp)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        for (int j = 0; j < RETIRED_THRESHOLD; j++) {
            if (hp->retired[i][j] != NULL) {
                free(hp->retired[i][j]);
            }
        }
        free(hp->retired[i]);
    }
    free(hp->retired);
}


// zapisuje w tablicy zarezerwowanych adresów adres odczytany z atom pod indeksem thread_id i zwraca go (nadpisuje istniejącą rezerwację, jeśli taka była dla thread_id).
// Korzystający z protect musi się upewnić, że podczas protect, atom nie zmienił wartości.
void* HazardPointer_protect(HazardPointer* hp, const _Atomic(void*)* atom)
{
    void* ptr = atomic_load(atom);
    atomic_store(&hp->pointer[_thread_id], ptr);
    return ptr;
}

// usuwa rezerwację, tzn. zamienia adres pod indeksem thread_id na NULL.
void HazardPointer_clear(HazardPointer* hp)
{
    atomic_store(&hp->pointer[_thread_id], NULL);
}

// dodaje ptr do zbioru adresów wycofanych, za których zwolnienie odpowiedzialny jest wątek thread_id. Następnie jeśli rozmiar zbióru wycofanych przekracza próg zdefiniowany stałą
// RETIRED_THRESHOLD (równą np. MAX_THREADS), to przegląda wszystkie adresy w swoim zbiorze i zwalnia (free()) te, które nie są zarezerwowane przez żaden wątek (usuwając je też ze zbioru).
void HazardPointer_retire(HazardPointer* hp, void* ptr)
{
    // jesli nie ma juz miejsca to czyścimy

    if (hp->retired_count[_thread_id] >= RETIRED_THRESHOLD) {
        for (int i = 0; i < RETIRED_THRESHOLD; i++) {
            void* retired_ptr = hp->retired[_thread_id][i];
            if (retired_ptr != NULL) {
                bool is_protected = false;
                for (int j = 0; j < _num_threads; j++) {
                    if (atomic_load(&hp->pointer[j]) == retired_ptr) {
                        is_protected = true;
                        break;
                    }
                }
                if (is_protected == false) {
                    free(retired_ptr);
                    hp->retired[_thread_id][i] = NULL;
                    hp->retired_count[_thread_id]--;
                }
            }
        }
    }

    // dodajemy ptr do zbioru wycofanych
    for (int i = 0; i < RETIRED_THRESHOLD; i++) {
        if (hp->retired[_thread_id][i] == NULL) {
            hp->retired[_thread_id][i] = ptr;
            hp->retired_count[_thread_id]++;
            break;
        }
    }

}
