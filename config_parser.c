#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cJSON/cJSON.h"
#include "cJSON/cJSON.c"

// void recursive_parser()

void parse_config(char *json) {
	cJSON *config = cJSON_Parse(json);
	if(config == NULL) {
		printf("Error while JSON parsing.'n");
		return;
	}

	const cJSON *name = cJSON_GetObjectItemCaseSensitive(config, "project");
	if (cJSON_IsString(name) && (name->valuestring != NULL)) {
		printf("Project \"%s\"\n", name->valuestring);
	}

	// const cJSON *sources = cJSON_GetObjectItemCaseSensitive(config, "sources"), *source;
	// cJSON_ArrayForEach(source, sources) {
	// 	//
	// }

	const cJSON *paths = cJSON_GetObjectItemCaseSensitive(config, "paths"), *path;
	cJSON_ArrayForEach(path, paths) {
		printf("Path \"%s\"\n", path->valuestring);
	}
	// if (cJSON_IsString(name) && (name->valuestring != NULL)) {
	// 	printf("Sources \"%s\"\n", name->valuestring);
	// }

	cJSON_Delete(config);
}

size_t read_config(char *path, char **config) {
	FILE *f_in = fopen(path, "rb");
	fseek(f_in, 0, SEEK_END);
	size_t config_len = ftell(f_in);
	rewind(f_in);
	*config = (char *)malloc(config_len);
	config_len = fread(*config, 1, config_len, f_in);
	fclose(f_in);
	return config_len;
}

int main() {
	char *config;
	size_t config_len = read_config("build.json", &config);
	parse_config(config);

	return 0;
}