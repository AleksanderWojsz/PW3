CC=gcc
CFLAGS=-I. -Wall -g
DEPS = SimpleQueue.h RingsQueue.h LLQueue.h BLQueue.h common.h HazardPointer.h
OBJ = SimpleQueue.o RingsQueue.o LLQueue.o BLQueue.o HazardPointer.o
TESTS_DIR = testy
SRC = $(wildcard $(TESTS_DIR)/*.c)
EXE = $(SRC:.c=)

# testy wykluczone (bez rozszerzeń .c)
EXCLUDE_TESTS := one_read_one_write

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TESTS_DIR)/%: $(TESTS_DIR)/%.c $(OBJ)
	$(CC) -o $@ $< $(OBJ) $(CFLAGS)

build: $(OBJ) $(EXE)

test: build
	@for test in $(EXE); do \
		testname=$$(basename $$test); \
		if echo $(EXCLUDE_TESTS) | grep -wq $$testname; then \
			echo "Skipping $$test"; \
			continue; \
		fi; \
		echo Running $$test with Valgrind; \
		valgrind --track-origins=yes --leak-check=yes  --show-leak-kinds=all --quiet --error-exitcode=1 $$test > /dev/null; \
		RESULT=$$?; \
		if [ $$RESULT -eq 0 ]; then \
			echo "OK"; \
		else \
			echo "MEMORY LEAK OR ERROR DETECTED IN $$test"; \
		fi \
	done

clean:
	rm -f *.o $(EXE)
