all: user_console
user_console: user_console.c ./lib/functions.c
	gcc -Wall -o $(OUTPUT_DIR)/user_console user_console.c ./lib/functions.c

# Flags
OUTPUT_DIR = bin

#! Note: you need to create the 'bin' directory
