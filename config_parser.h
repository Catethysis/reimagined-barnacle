#include <stdint.h>
#include "cJSON/cJSON.h"

struct project_struct project;

void search_project_files();
void recursive_config_parser(uint8_t depth, cJSON *json_sources, char *flags_string);
void parse_config(char *json);
size_t read_config(char *path, char **config);
int get_build_json_name(int argc, char *argv[], char *config_file);