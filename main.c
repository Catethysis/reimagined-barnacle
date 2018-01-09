#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "config_parser.h"
#include "common.h"
#include "timer/timer.h"

struct project_struct project;

void parse_source(uint16_t n_source) {
	char *ptr/* = file*/, *needle = "#include";
	while (ptr != NULL) {
		ptr = strstr(ptr, needle);
		if (ptr != NULL) {
			ptr += sizeof("#include ");
			char delimiter = *(ptr - 1);
			if(delimiter == '<')
				delimiter = '>';
			char *dep = (char *)malloc(path_max_length), *dep_name = dep;
			while ((*dep++ = *ptr++) != delimiter);
			*(dep - 1) = 0;
			// old_ptr = ptr;
		}
	}
}

void list_project_files() {
	printf("Project \"%s\"\n", project.name);
	// printf("\nSources:\n");
	// for(uint16_t i = 0; i < project.n_sources; i++)
	// 	printf("%s (%s)\n", project.sources[i].path, project.sources[i].flags);
	// printf("\nPaths:\n");
	// for(uint16_t i = 0; i < project.n_include_dirs; i++)
	// 	printf("%s\n", project.include_dirs[i]);
	// printf("\nProject files:\n");
	// for(uint16_t i = 0; i < project.n_project_files; i++)
	// 	printf("%s\n", project.project_files[i].path);
}

int main(int argc, char *argv[]) {
	timer_start();

	// Find build.json in command line arguments and check if it exists
	char *config_file;
	
	if(get_build_json_name(argc, argv, config_file) == -1)
		return -1;

	char *config;
	read_config(config_file, &config);
	timer_tick("Read");

	project.n_sources = 0;
	parse_config(config);
	timer_tick("Config parsing");

	search_project_files();
	timer_tick("Files search");

	parse_source(0);

	list_project_files();
	timer_end("Total");

	return 0;
}