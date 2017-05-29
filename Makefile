PROGRAM_NAME = xenonchess

CC = cc
INSTALL = install

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

PREFIX = /usr
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib

TSCP = /usr/local/bin/tscp

INCLUDES =
LIBS =

CUTECHESS=cutechess-cli

HEADERS := $(wildcard *.h)

CFLAGS = -Ofast -g -Wall -Wextra -std=gnu99 $(INCLUDES)

all: Makefile $(PROGRAM_NAME) $(PROGRAM_NAME)-old

$(PROGRAM_NAME): Makefile $(HEADERS) $(SRC)
	$(CC) $(SRC) -o $@ $(CFLAGS) $(LIBS) -DTEST_FEATURE

$(PROGRAM_NAME)-old: Makefile $(HEADERS) $(SRC)
	$(CC) $(SRC) -o $@ $(CFLAGS) $(LIBS)

test: all
	$(CUTECHESS) -engine name=xenon-new proto=uci dir=`pwd` cmd=./xenonchess -engine proto=uci dir=`pwd` cmd=./xenonchess-old name=xenon-old -each tc=inf -rounds 200 -ratinginterval 1

test-tscp: $(PROGRAM_NAME)
	$(CUTECHESS) -engine name=xenon-new proto=uci dir=`pwd` cmd=./xenonchess -engine proto=xboard dir=/ cmd=$(TSCP) name=tscp -each tc=1+.01 -rounds 1000

%.o: %.c Makefile $(HEADERS)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

install: all
	@echo "INSTALL $(PROGRAM_NAME)"
	@install $(PROGRAM_NAME) $(BINDIR)

clean:
	@echo "Cleaning build directory..."
	@rm -f $(OBJ) $(PROGRAM_NAME)
