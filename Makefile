# Flags
OUTPUT_DIR = bin

# Targets
all: $(OUTPUT_DIR) user_console sensor home_iot

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

user_console: user_console.c ./lib/functions.c
	gcc -Wall -o $(OUTPUT_DIR)/$@ $^

sensor: sensor.c ./lib/functions.c
	gcc -Wall -o $(OUTPUT_DIR)/$@ $^

home_iot: sys_manager.c
	gcc -Wall -o $(OUTPUT_DIR)/$@ $^

# $@ is the name of the target
# $^ is the list of prerequisites