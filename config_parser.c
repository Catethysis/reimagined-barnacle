#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cJSON/cJSON.h"
#include "timer/timer.h"
#include <unistd.h>
#include <dirent.h>

const uint32_t name_max_length = 100;
const uint32_t path_max_length = 300;
const uint32_t flags_max_length = 100;
const uint32_t sources_max_count = 200;

struct {
	char name[name_max_length];
	char path[path_max_length];
	struct {
		//
	} dependencies[sources_max_count];
	uint16_t n_dependencies;

	struct {
		char name[path_max_length];
		char path[path_max_length];
	} project_files[sources_max_count];
	uint16_t n_project_files;

	struct {
		char path[path_max_length];
		char name[name_max_length];
		char flags[flags_max_length];
	} sources[sources_max_count];
	uint16_t n_sources;

	char include_dirs[sources_max_count][path_max_length];
	uint16_t n_include_dirs;
} project;

void search_project_files() {
	DIR *d;
	struct dirent *dir;
	for (uint16_t i = 0; i < project.n_include_dirs; i++) {
		d = opendir(project.include_dirs[i]);
		printf("dir %s\n", project.include_dirs[i]);
		if (d) {
			while ((dir = readdir(d)) != NULL)
				if(dir->d_name[0] != '.') {
					printf("   %s\n", dir->d_name);
					strcpy(project.project_files[project.n_project_files].name, dir->d_name);
					strcpy(project.project_files[project.n_project_files].path, project.include_dirs[i]);
					strcat(project.project_files[project.n_project_files].path, "/");
					strcat(project.project_files[project.n_project_files].path, dir->d_name);
					project.n_project_files++;
				}
			closedir(d);
		}
	}
}

void recursive_config_parser(uint8_t depth, cJSON *json_sources, char *flags_string) {
	const cJSON *flags = cJSON_GetObjectItemCaseSensitive(json_sources, "flags");

	if(cJSON_IsString(flags)) {
		strcpy(flags_string, flags->valuestring);
		strcat(flags_string, " ");
	}

	const cJSON *path = cJSON_GetObjectItemCaseSensitive(json_sources, "path");
	char path_string[path_max_length] = ".";
	if(cJSON_IsString(path))
		strcpy(path_string, path->valuestring);
	strcat(path_string, "/");

	const cJSON *files = cJSON_GetObjectItemCaseSensitive(json_sources, "files");
	if(cJSON_IsArray(files)) {
		cJSON *file;
		cJSON_ArrayForEach(file, files) {
			if(cJSON_IsString(file)) {
				strcpy(project.sources[project.n_sources].flags, flags_string);
				strcat(project.sources[project.n_sources].path, project.path);
				strcat(project.sources[project.n_sources].path, "/");
				strcat(project.sources[project.n_sources].path, path_string);
				strcat(project.sources[project.n_sources].path, file->valuestring);
				strcpy(project.sources[project.n_sources].name, file->valuestring);
				project.n_sources++;
			}
			else
			if(cJSON_IsObject(file)) {
				char new_flags_string[200];
				strcpy(new_flags_string, flags_string);
				recursive_config_parser(depth + 1, file, new_flags_string);
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
	timer_tick("JSON parsing");

	const cJSON *name = cJSON_GetObjectItemCaseSensitive(config, "project");
	if (cJSON_IsString(name) && (name->valuestring != NULL))
		strcpy(project.name, name->valuestring);

	const cJSON *project_path = cJSON_GetObjectItemCaseSensitive(config, "path");
	if (cJSON_IsString(project_path) && (project_path->valuestring != NULL))
		strcpy(project.path, project_path->valuestring);
	else
		strcpy(project.path, ".");

	char flags_string[flags_max_length] = "";
	const cJSON *sources = cJSON_GetObjectItemCaseSensitive(config, "sources");
	cJSON *source;
	cJSON_ArrayForEach(source, sources) {
		if(cJSON_IsObject(source))
			recursive_config_parser(0, source, flags_string);
	}

	const cJSON *paths = cJSON_GetObjectItemCaseSensitive(config, "paths"), *path;
	cJSON_ArrayForEach(path, paths) {
		strcpy(project.include_dirs[project.n_include_dirs], project.path);
		strcat(project.include_dirs[project.n_include_dirs], "/");
		strcat(project.include_dirs[project.n_include_dirs], path->valuestring);
		project.n_include_dirs++;
	}

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

void list_project_files() {
	printf("Project \"%s\"\n", project.name);
	// printf("\nSources:\n");
	// for(uint16_t i = 0; i < project.n_sources; i++)
	// 	printf("%s (%s)\n", project.sources[i].path, project.sources[i].flags);
	// printf("\nPaths:\n");
	// for(uint16_t i = 0; i < project.n_include_dirs; i++)
	// 	printf("%s\n", project.include_dirs[i]);
	printf("\nProject files:\n");
	for(uint16_t i = 0; i < project.n_project_files; i++)
		printf("%s\n", project.project_files[i].path);
}

int main(int argc, char *argv[]) {
	timer_start();

	// Find build.json in command line arguments and check if it exists
	char *config_file;
	if(argc == 2)
		config_file = argv[1];
	else
		config_file = "./build.json";

	if (access(config_file, F_OK) == -1) {
		printf("Build config file %s not found, terminating.\n", config_file);
		return -1;
	}

	char *config;
	read_config(config_file, &config);
	timer_tick("Read");

	project.n_sources = 0;
	parse_config(config);
	timer_tick("Config parsing");

	search_project_files();
	timer_tick("Files search");

	list_project_files();
	timer_end("Total");

	return 0;
}