files := kilo
kilo: kilo.c
	$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

all: $(files)
clean:
	rm -f $(files)