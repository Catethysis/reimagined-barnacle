TARGET = config_parser
BUILD_DIR = build

SOURCES = \
	main.c \
	config_parser.c \
	cJSON/cJSON.c \
	timer/timer.c

INCLUDES = -Itimer

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(SOURCES) Makefile | $(BUILD_DIR) 
	gcc $(SOURCES) $(INCLUDES) -O2 -o $@

$(BUILD_DIR):
	mkdir $@

clean:
	rm -rf $(BUILD_DIR)