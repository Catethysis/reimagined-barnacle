#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cJSON/cJSON.h"
#include "timer.h"

struct {
	char name[100];
	struct {
		char path[200];
		char name[100];
		char flags[100];
	} sources[200];
	uint16_t n_sources;
	char paths[100][100];
	uint16_t n_paths;
} project;

void recursive_parser(uint8_t depth, cJSON *json_sources, char *flags_string) {
	const cJSON *flags = cJSON_GetObjectItemCaseSensitive(json_sources, "flags");

	if(cJSON_IsString(flags)) {
		strcpy(flags_string, flags->valuestring);
		strcat(flags_string, " ");
	}

	const cJSON *path = cJSON_GetObjectItemCaseSensitive(json_sources, "path");
	char path_string[200] = ".";
	if(cJSON_IsString(path))
		strcpy(path_string, path->valuestring);
	strcat(path_string, "/");

	const cJSON *files = cJSON_GetObjectItemCaseSensitive(json_sources, "files");
	if(cJSON_IsArray(files)) {
		cJSON *file;
		cJSON_ArrayForEach(file, files) {
			if(cJSON_IsString(file)) {
				strcpy(project.sources[project.n_sources].flags, flags_string);
				strcpy(project.sources[project.n_sources].path, path_string);
				strcat(project.sources[project.n_sources].path, file->valuestring);
				strcpy(project.sources[project.n_sources].name, file->valuestring);
				project.n_sources++;
			}
			else
			if(cJSON_IsObject(file)) {
				char new_flags_string[200];
				strcpy(new_flags_string, flags_string);
				recursive_parser(depth + 1, file, new_flags_string);
			}
		}
	}
}

void parse_config(char *json) {
	cJSON *config = cJSON_Parse(json);
	if(config == NULL) {
		printf("Error while JSON parsing.'n");
		return;
	}
	timer_tick("Parse");

	const cJSON *name = cJSON_GetObjectItemCaseSensitive(config, "project");
	if (cJSON_IsString(name) && (name->valuestring != NULL))
		strcpy(project.name, name->valuestring);

	char flags_string[100] = "";
	const cJSON *sources = cJSON_GetObjectItemCaseSensitive(config, "sources");
	cJSON *source;
	cJSON_ArrayForEach(source, sources) {
		if(cJSON_IsObject(source))
			recursive_parser(0, source, flags_string);
	}

	const cJSON *paths = cJSON_GetObjectItemCaseSensitive(config, "paths"), *path;
	cJSON_ArrayForEach(path, paths)
		strcpy(project.paths[project.n_paths++], path->valuestring);

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
	timer_start();
	
	char *config;
	size_t config_len = read_config("build.json", &config);
	timer_tick("Read");
	project.n_sources = 0;
	parse_config(config);
	timer_tick("File list");

	printf("Project \"%s\"\n", project.name);
	printf("\nSources:\n");
	for(uint16_t i = 0; i < project.n_sources; i++)
		printf("%s (%s)\n", project.sources[i].path, project.sources[i].flags);
	printf("\nPaths:\n");
	for(uint16_t i = 0; i < project.n_paths; i++)
		printf("%s\n", project.paths[i]);
	timer_end("Total");

	return 0;
}