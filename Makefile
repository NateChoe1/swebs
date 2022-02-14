SRC = $(wildcard src/*.c)
OBJ = $(subst .c,.o,$(subst src,work,$(SRC)))
LIBS = -pthread -pie -lrt $(shell pkg-config --libs gnutls)
CFLAGS := -O2 -pipe -Wall -Wpedantic -Werror -ansi -D_POSIX_C_SOURCE=200809L
CFLAGS += -Isrc/include -fpie $(shell pkg-config --cflags gnutls)
INSTALLDIR := /usr/sbin
OUT = swebs

build/$(OUT): $(OBJ)
	$(CC) $(OBJ) -o build/$(OUT) $(LIBS)

work/%.o: src/%.c $(wildcard src/include/*.h)
	$(CC) $(CFLAGS) $< -c -o $@

install: build/$(OUT)
	cp build/$(OUT) $(INSTALLDIR)/$(OUT)
	if ! id swebs >> /dev/null 2>&1; then useradd -M swebs; fi

uninstall: $(INSTALLDIR)/$(OUT)
	rm $(INSTALLDIR)/$(OUT)
	if id swebs >> /dev/null 2>&1; then userdel swebs; fi
