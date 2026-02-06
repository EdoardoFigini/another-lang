SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

CFLAGS = -Wall -Wextra -Wswitch-enum -ggdb -rdynamic

bin/main: $(OBJS) bin
	gcc $(CFLAGS) -o $@ $(OBJS) -lffi

build/%.o: src/%.c build
	gcc $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

bin:
	mkdir -p bin

clean:
	rm -rf build
	rm -rf bin
.PHONY: clean

run: bin/main
	./bin/main
.PHONY: run

debug: bin/main
	gdb ./bin/main
.PHONY: run

