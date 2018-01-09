#ifndef __COMMON_H
#define __COMMON_H
#include <stdint.h>

static const uint32_t name_max_length = 100;
static const uint32_t path_max_length = 300;
static const uint32_t flags_max_length = 100;
static const uint32_t sources_max_count = 200;

struct project_struct {
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
};

#endif // __COMMON_H