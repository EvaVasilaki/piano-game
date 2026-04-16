EXEC = game
WIN_EXEC = game.exe
WEB_EXEC = game.html

ROOT = ../..
INCLUDE = -I$(ROOT)/include -I$(ROOT)/modules
LIBDIR = -L$(ROOT)/lib

SRC = game.c $(ROOT)/modules/interface.c $(ROOT)/modules/state_alt.c

CC = gcc
CFLAGS = -Wall -g $(INCLUDE)
LDFLAGS = $(LIBDIR) -l:k08_linux.a -l:k08midi_linux.a -l:libraylib_linux.a -lm -ldl -lpthread -lGL -lrt -lX11

WINCC = x86_64-w64-mingw32-gcc
WINCFLAGS = -Wall -g $(INCLUDE)
WINLDFLAGS = $(LIBDIR) -l:k08_win.a -l:k08midi_win.a -l:libraylib_win.a -lopengl32 -lgdi32 -lwinmm -lm

WEBCC = emcc
WEBCFLAGS = -Wall -O2 $(INCLUDE)
WEBLDFLAGS = \
	$(ROOT)/lib/k08.a \
	$(ROOT)/lib/k08midi.a \
	$(ROOT)/lib/libraylib.a \
	-s USE_GLFW=3 \
	-s ASYNCIFY \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s FORCE_FILESYSTEM=1 \
	--preload-file assets \
	--shell-file $(ROOT)/lib/libraylib.html

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(EXEC)

run: $(EXEC)
	./$(EXEC)

windows: $(WIN_EXEC)

$(WIN_EXEC): $(SRC)
	$(WINCC) $(WINCFLAGS) $(SRC) $(WINLDFLAGS) -o $(WIN_EXEC)

web: $(WEB_EXEC)

$(WEB_EXEC): $(SRC)
	$(WEBCC) $(WEBCFLAGS) $(SRC) $(WEBLDFLAGS) -o $(WEB_EXEC)

clean:
	rm -f $(EXEC) $(WIN_EXEC) $(WEB_EXEC) game.js game.wasm game.data

.PHONY: all run windows web clean