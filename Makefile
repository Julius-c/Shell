LAB = njush
SRC = shell

.PHONY: all

all: $(SRC).c
	@gcc -std=c99 -O2 -Wall -Werror -ggdb -o $(LAB) $(SRC).c
	@./$(LAB)
