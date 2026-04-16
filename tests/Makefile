# paths
LIB = ../lib
INCLUDE = ../include
MODULES = ../modules

# compiler
CC = gcc

# Compile options. Το -I<dir> λέει στον compiler να αναζητήσει εκεί include files
CFLAGS = -Wall -Werror -g -I$(INCLUDE)
LDFLAGS = -lm

# Αρχεία .o
OBJS = state_test.o $(MODULES)/state.o $(LIB)/k08.a $(LIB)/k08midi.a $(LIB)/libraylib.a

# Αρχεία .o για state_alt
OBJS_ALT = state_test.o $(MODULES)/state_alt.o $(LIB)/k08.a $(LIB)/k08midi.a $(LIB)/libraylib.a

# Το εκτελέσιμο πρόγραμμα
EXEC = state_test

# Το εκτελέσιμο πρόγραμμα για το alt_state
EXEC_ALT = state_alt_test

# Παράμετροι για δοκιμαστική εκτέλεση
ARGS =

# Παράμετροι της βιβλιοθήκης raylib
include $(LIB)/libraylib.mk


all: $(EXEC) $(EXEC_ALT)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

$(EXEC_ALT): $(OBJS_ALT)
	$(CC) $(OBJS_ALT) -o $(EXEC_ALT) $(LDFLAGS)


clean:
	rm -f $(OBJS) $(EXEC) $(OBJS_ALT) $(EXEC_ALT)

run: $(EXEC)
	./$(EXEC) $(ARGS)

run_alt: $(EXEC_ALT)
	./$(EXEC_ALT) $(ARGS_ALT)

# Για να φτιάξουμε τα k08.a/libraylib.a τρέχουμε το make στο lib directory.
$(LIB)/%.a:
	$(MAKE) -C $(LIB) $*.a

$(LIB)/k08midi/%.a:
	$(MAKE) -C $(LIB)/k08midi $*.a
