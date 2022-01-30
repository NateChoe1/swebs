SRC = $(wildcard src/*.c)
OBJ = $(subst .c,.o,$(subst src,work,$(SRC)))
LIBS = -pthread -pie -lrt $(shell pkg-config --libs gnutls)
CFLAGS := -O2 -pipe -Wall -Wpedantic -Werror
CFLAGS += -Isrc/include -fpie $(shell pkg-config --cflags gnutls)
INSTALLDIR := /usr/bin
OUT = swebs

build/$(OUT): $(OBJ)
	$(CC) $(OBJ) -o build/$(OUT) $(LIBS)

work/%.o: src/%.c $(wildcard src/include/*.h)
	$(CC) $(CFLAGS) $< -c -o $@

install: build/$(OUT)
	useradd -M swebs
	cp build/$(OUT) $(INSTALLDIR)/$(OUT)

uninstall: $(INSTALLDIR)/$(OUT)
	userdel swebs
	rm $(INSTALLDIR)/$(OUT)
