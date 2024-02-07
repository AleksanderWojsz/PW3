cmake_minimum_required(VERSION 3.11)
project(MIMUW-PW-lockfree C)

set(THREADS_PREFER_PTHREAD_FLAG TRUE) # prefer -pthread to -lphtread
find_package(Threads REQUIRED)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-unused-parameter -march=native -fno-exceptions") # 03





add_library(queues OBJECT SimpleQueue.c RingsQueue.c LLQueue.c BLQueue.c HazardPointer.c)
target_link_libraries(queues PRIVATE Threads::Threads atomic)

include(CTest)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")

set(TEST_SOURCES
        testy/simpleTester.c
        testy/many_threads.c
        testy/big_data.c
        testy/is_empty.c
        testy/should_be_empty.c
        testy/pop_on_empty.c
        testy/sleep_big_data.c
        testy/sleep_many_threads.c
        testy/one_read_one_write.c
        testy/simple.c

)

#add_compile_options(-fsanitize=address -Og -g)
#add_link_options(-fsanitize=address -Og -g)

#add_compile_options(-fsanitize=thread -Og -g)
#add_link_options(-fsanitize=thread -Og -g)


 set(TEST_INDEX_COUNT_simpleTester 500)
 set(TEST_INDEX_COUNT_many_threads 500)
 set(TEST_INDEX_COUNT_big_data 100)
 set(TEST_INDEX_COUNT_is_empty 500)
 set(TEST_INDEX_COUNT_should_be_empty 500)
 set(TEST_INDEX_COUNT_pop_on_empty 500)
 set(TEST_INDEX_COUNT_sleep_big_data 50)
 set(TEST_INDEX_COUNT_sleep_many_threads 50)
 set(TEST_INDEX_COUNT_one_read_one_write 500)
set(TEST_INDEX_COUNT_simple 100)

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE queues Threads::Threads atomic)

     set(TEST_COUNT 0)

    if (DEFINED TEST_INDEX_COUNT_${TEST_NAME})
        set(TEST_COUNT ${TEST_INDEX_COUNT_${TEST_NAME}})
    endif()

    set(TEST_INDEX 1)
    while(TEST_INDEX LESS_EQUAL ${TEST_COUNT})
        add_test(NAME ${TEST_NAME}_${TEST_INDEX} COMMAND "${CMAKE_SOURCE_DIR}/bin/${TEST_NAME}")
        math(EXPR TEST_INDEX "${TEST_INDEX}+1")
    endwhile()
endforeach()