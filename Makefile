all: user_console
user_console: user_console.c
	gcc $(CFLAGS) user_console.c -o user_console

# Flags
CFLAGS = -Wall -Wextra -pedantic