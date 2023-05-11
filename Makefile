#* - Filipe Rodrigues (2021218054)
#* - Joás Silva (2021226149)

# Flags
OUTPUT_DIR = bin
FLAGS = -Wall -g -pthread -D_REENTRANT

# Source Files
SRCS = user_console.c sensor.c # list of source files
OBJS = $(SRCS:%.c=$(OUTPUT_DIR)/%.o) $(OUTPUT_DIR)/sys_manager.o

# Targets
.PHONY: all clean
.PRECIOUS: $(OUTPUT_DIR)/%.o # Don't remove .o files automatically

all: $(OUTPUT_DIR) $(SRCS:%.c=$(OUTPUT_DIR)/%) $(OUTPUT_DIR)/home_iot
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

# Executables
$(OUTPUT_DIR)/%: $(OUTPUT_DIR)/%.o
	gcc $(FLAGS) $^ -o $@

$(OUTPUT_DIR)/home_iot: $(OUTPUT_DIR)/sys_manager.o
	gcc $(FLAGS) $^ -o $@