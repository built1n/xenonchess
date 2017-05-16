PROGRAM_NAME = chessengine

CC = cc
INSTALL = install

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

PREFIX = /usr
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib

INCLUDES =
LIBS =

HEADERS := $(wildcard *.h)

CFLAGS = -O3 -g -Wall -Wextra -std=gnu99 $(INCLUDES)

all: Makefile $(PROGRAM_NAME)

$(PROGRAM_NAME): Makefile $(OBJ) $(HEADERS)
	@echo "LD $@"
	@$(CC) $(OBJ) -o $@ $(CFLAGS) $(LIBS)

%.o: %.c Makefile $(HEADERS)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

install: all
	@echo "INSTALL $(PROGRAM_NAME)"
	@install $(PROGRAM_NAME) $(BINDIR)

clean:
	@echo "Cleaning build directory..."
	@rm -f $(OBJ) $(PROGRAM_NAME)
