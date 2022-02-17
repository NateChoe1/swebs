SRC = $(wildcard src/*.c)
OBJ = $(subst .c,.o,$(subst src,work,$(SRC)))
LIBS = -pthread -pie -lrt -ldl $(shell pkg-config --libs gnutls)
CFLAGS := -O2 -pipe -Wall -Wpedantic -ansi
CFLAGS += -Isrc/ -fpie -D_POSIX_C_SOURCE=200809L $(shell pkg-config --cflags gnutls)
INSTALLDIR := /usr/sbin
HEADERDIR := /usr/include/
INCLUDE_DIRECTORY := swebs
OUT = swebs

build/$(OUT): $(OBJ)
	$(CC) $(OBJ) -o build/$(OUT) $(LIBS)

work/%.o: src/%.c $(wildcard src/swebs/*.h)
	$(CC) $(CFLAGS) $< -c -o $@

install: build/$(OUT)
	cp build/$(OUT) $(INSTALLDIR)/$(OUT)
	cp -r src/$(INCLUDE_DIRECTORY) $(HEADERDIR)/
	if ! id swebs >> /dev/null 2>&1; then useradd -M swebs; fi

uninstall: $(INSTALLDIR)/$(OUT)
	rm $(INSTALLDIR)/$(OUT)
	rm -r $(HEADERDIR)/$(INCLUDE_DIRECTORY)
	if id swebs >> /dev/null 2>&1; then userdel swebs; fi
