NAME = cecs-example

OUT = ./out
BIN = ./bin
SRC = ./src
INC = ./include

CC = gcc

CFLAGS = -g -O1 -fpic -std=c11 -fverbose-asm -I$(INC) -Wextra -Wall -Werror -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wswitch-default -Wswitch-enum -Wconversion

SRCS = $(wildcard $(SRC)/*.c)
LIBS =
OBJS = $(patsubst $(SRC)/%.c,$(OUT)/%.o,$(SRCS))
INCS = $(wildcard $(INC)/*.h)

TARGET = $(BIN)/$(NAME)

all: $(BIN)/$(NAME)

$(OUT)/%.o: $(SRC)/%.c $(INCS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -Wall $(LIBS) -o $@

clean:
	rm -f $(OUT)/* $(BIN)/*