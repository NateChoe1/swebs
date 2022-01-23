SRC = $(wildcard src/*.c)
OBJ = $(subst .c,.o,$(subst src,work,$(SRC)))
LIBS = 
CFLAGS := -O2 -pipe -Wall -Wpedantic -Werror
CFLAGS += -Isrc/include -pthread
INSTALLDIR := /usr/bin
OUT = swebs

build/$(OUT): $(OBJ)
	$(CC) $(OBJ) -o build/$(OUT) $(LIBS)

work/%.o: src/%.c $(wildcard src/include/*.h)
	$(CC) $(CFLAGS) $< -c -o $@

install: build/$(OUT)
	cp build/$(OUT) $(INSTALLDIR)/$(OUT)

uninstall: $(INSTALLDIR)/$(OUT)
	rm $(INSTALLDIR)/$(OUT)
