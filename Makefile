TARGET = config_parser
BUILD_DIR = build

SOURCES = config_parser.c cJSON/cJSON.c

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(SOURCES) Makefile | $(BUILD_DIR) 
	gcc $(SOURCES) -o $@

$(BUILD_DIR):
	mkdir $@

clean:
	rm -rf $(BUILD_DIR)