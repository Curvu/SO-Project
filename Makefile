# Flags
OUTPUT_DIR = bin
FLAGS = -Wall -g -pthread -D_REENTRANT

# Source Files
SRCS = user_console.c sensor.c # list of source files
OBJS = $(SRCS:%.c=$(OUTPUT_DIR)/%.o) $(OUTPUT_DIR)/functions.o $(OUTPUT_DIR)/sys_manager.o

# Targets
.PHONY: all clean

all: $(OUTPUT_DIR) $(SRCS:%.c=$(OUTPUT_DIR)/%) $(OUTPUT_DIR)/home_iot
	@rm -f $(OBJS)
	@clear
	@echo "\033[1;32mBuild Complete\033[0m"

# Delete output directory
clean:
	rm -rf $(OUTPUT_DIR)

# Create output directory
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# Object Files
$(OUTPUT_DIR)/%.o: %.c
	gcc $(FLAGS) -c $^ -o $@

$(OUTPUT_DIR)/functions.o: ./lib/functions.c ./lib/functions.h
	gcc -c ./lib/functions.c -o $@

# Executables
$(OUTPUT_DIR)/%: $(OUTPUT_DIR)/%.o $(OUTPUT_DIR)/functions.o
	gcc $(FLAGS) $^ -o $@

$(OUTPUT_DIR)/home_iot: $(OUTPUT_DIR)/sys_manager.o $(OUTPUT_DIR)/functions.o
	gcc $(FLAGS) $^ -o $@