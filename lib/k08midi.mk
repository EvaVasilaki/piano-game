# Παράμετροι για χρήση της βιλιοθήκης k08midi

ifdef WASM
	# Emscripten
else ifeq ($(OS),Windows_NT)
	# Windows
	LDFLAGS += -lwinmm -pthread
else ifeq ($(shell uname -s),Linux)
	# Linux
	LDFLAGS += -lasound
else
	# macOS
	LDFLAGS += -framework CoreMIDI
endif