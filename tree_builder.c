#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include "lz4.h"
#include "lz4.c"

struct timeval start, tick, stop;
double secs = 0;

void timer_start() {
	gettimeofday(&start, NULL);
	gettimeofday(&tick, NULL);
}

void timer_tick(char *msg) {
	gettimeofday(&stop, NULL);
	secs = (double)((stop.tv_sec * 1000000 + stop.tv_usec) - (tick.tv_sec * 1000000 + tick.tv_usec)) / 1000;
	printf("%-15s: %.0f ms\n", msg, secs);
	gettimeofday(&tick, NULL);
}

void timer_end(char *msg) {
	gettimeofday(&stop, NULL);
	secs = (double)((stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) / 1000;
	printf("%-15s: %.0f ms\n", msg, secs);
}

#define PATH_LEN 300
#define MAX_FILE_SIZE 100000
#define FILES_COUNT 500
#define DEPS_COUNT 300
#define INC_DIRS_COUNT 100
#define PART_COUNT 100
#define MAX_LEAFS 200

struct vertex {
	char name[PATH_LEN];
	char path[PATH_LEN];
	uint16_t file;	
	int16_t parents[DEPS_COUNT], n_parents;
	uint16_t childs[DEPS_COUNT], n_childs;
	char *parts[PART_COUNT];
	uint16_t part_len[PART_COUNT];
	uint16_t parts_n;
	uint16_t all_deps[DEPS_COUNT];
	uint16_t all_deps_n;
} vertexes[FILES_COUNT] = {};
uint16_t n_vertexes = 0;

uint16_t add_vertex(int16_t parent, char *name, char *path) {
	strcpy(vertexes[n_vertexes].name, name);
	strcpy(vertexes[n_vertexes].path, path);
	vertexes[n_vertexes].parents[0] = parent;
	vertexes[n_vertexes].n_parents = 1;
	if (parent != -1)
	vertexes[parent].childs[vertexes[parent].n_childs++] = n_vertexes;
	return n_vertexes++;
}

struct source {
	char path[PATH_LEN];
	uint16_t deps[DEPS_COUNT];
	uint16_t deps_n;
	uint16_t file;
	char *parts[PART_COUNT];
	uint16_t part_len[PART_COUNT];
	uint16_t parts_n;
	uint16_t all_deps[DEPS_COUNT];
	uint16_t all_deps_n;
} sources[FILES_COUNT];
uint16_t n_sources = 0;
char *files_list_buffer, *files_list_buffer_start;

char include_dirs[INC_DIRS_COUNT][PATH_LEN], include_dirs_count = 0;

struct time_acc {
	uint32_t start, stop, cycle, total;
} find_files_time, file_load_time, files_parsing, tree_flattening, deps_gluing, deps_lzoing;//time_accums[10];

uint32_t find_file_count = 0, access_file_count = 0;

void time_accumulate_reset(struct time_acc *time_accum) {
	time_accum->total = 0;
}

void time_accumulate_resume(struct time_acc *time_accum) {
	struct timeval find_file_start;
	gettimeofday(&find_file_start, NULL);
	time_accum->start = find_file_start.tv_sec * 1000000 + find_file_start.tv_usec;
}

void time_accumulate_pause(struct time_acc *time_accum) {
	find_file_count++;
	struct timeval find_file_stop;
	gettimeofday(&find_file_stop, NULL);
	time_accum->stop = find_file_stop.tv_sec * 1000000 + find_file_stop.tv_usec;
	time_accum->cycle = time_accum->stop - time_accum->start;
	time_accum->total += time_accum->cycle;
}

uint32_t time_accumulate_read(struct time_acc *time_accum) {
	return time_accum->total;
}

struct include {
	char name[PATH_LEN];
	char path[PATH_LEN];
} includes[DEPS_COUNT];
uint16_t includes_count = 0;

bool std_lib_file(char *input_file) {
	return (strcmp(input_file, "stdio.h" ) == 0) || (strcmp(input_file, "stdlib.h" ) == 0) || (strcmp(input_file, "stdint.h") == 0) ||
		   (strcmp(input_file, "stdarg.h") == 0) || (strcmp(input_file, "stdbool.h") == 0) || (strcmp(input_file, "string.h") == 0) ||
		   (strcmp(input_file, "math.h"  ) == 0);
}

uint32_t files_found = 0, files_not_found = 0;

int find_file_in_includes(char *input_file) {
	time_accumulate_resume(&find_files_time);

	if(std_lib_file(input_file)) {
		// printf("%s standard\n", input_file);
		time_accumulate_pause(&find_files_time);
		return 0;
	}
	
	for (uint16_t i = 0; i < includes_count; i++) {
		access_file_count++;
		if(strcmp(input_file, includes[i].name) == 0) {
			strcpy(input_file, includes[i].path);
			time_accumulate_pause(&find_files_time);
			files_found++;
			return 1;
		}
	}
	files_not_found++;
	// printf("%s not found\n", input_file);
	time_accumulate_pause(&find_files_time);
	return 0;
}

int16_t find_dep_in_known(char *input_file) {
	// printf("search for %s... ", input_file);
	for (uint16_t i = 0; i < n_vertexes; i++) {
		// printf("check node name %s, path %s\n", vertexes[i].name, vertexes[i].path);
		if (strcmp(input_file, vertexes[i].name) == 0) {
			// printf("found node %d\n", i);
			return i;
		}
	}
	// printf("not found\n");
	return -1;
}

struct file {
	char path[PATH_LEN];
	char content[MAX_FILE_SIZE];
};

struct file files[FILES_COUNT];
uint16_t files_count = 0;

uint16_t load_file(char *input_file) {
	time_accumulate_resume(&file_load_time);

	for (uint16_t i = 0; i < files_count; i++)
		if (strcmp(input_file, files[i].path) == 0)
			return i;

	files_count++;
	
	strcpy(files[files_count].path, input_file);

	FILE *f_in = fopen(input_file, "rb");
	fseek(f_in, 0, SEEK_END);
	long lSize = ftell(f_in);
	rewind(f_in);
	size_t file_size = fread(files[files_count].content, 1, lSize, f_in);
	fclose(f_in);

	time_accumulate_pause(&file_load_time);

	return files_count;
}

uint32_t parses = 0, total_passes = 0;
uint16_t max_depth = 0, parse_passes = 0;

void parse_source(uint16_t depth, char *input_file, int16_t parent, int16_t source_id) {
	uint16_t file_number = load_file(input_file);
	// char *(*parts[PART_COUNT]);
	// uint16_t *parts_n;
	if(parent == -1) {
		sources[source_id].file = file_number;
		// parts_n = &(sources[source_id].parts_n);
		// parts = &(sources[source_id].parts);
	} else {
		vertexes[parent].file = file_number;
		// parts_n = &(vertexes[parent].parts_n);
		// parts = &(vertexes[parent].parts);
	}
	
	char *ptr = files[file_number].content, *needle = "#include", *old_ptr = ptr, *start_ptr = ptr;
	time_accumulate_resume(&files_parsing);
	size_t ptrlen = strlen(ptr);
	size_t nlen = strlen(needle);
	uint32_t passes = 0;
	if (depth > max_depth)
	max_depth = depth;
	parse_passes++;

	if(parent == -1) {
		sources[source_id].parts_n = 0;
		sources[source_id].parts[sources[source_id].parts_n++] = ptr;
		sources[source_id].part_len[1] = 0;
		sources[source_id].all_deps_n = 0;
	} else {
		vertexes[parent].parts_n = 0;
		vertexes[parent].parts[vertexes[parent].parts_n++] = ptr;
		vertexes[parent].part_len[1] = 0;
		vertexes[parent].all_deps_n = 0;
	}
	
	while (ptr != NULL) {
		ptr = strstr(ptr, needle);
		if (ptr != NULL) {
			if(parent == -1)
				sources[source_id].part_len[sources[source_id].parts_n - 1] = ptr - old_ptr;//sources[source_id].part_len[sources[source_id].parts_n];
			else
				vertexes[parent].part_len[vertexes[parent].parts_n - 1] = ptr - old_ptr;//vertexes[parent].parts[vertexes[parent].parts_n];
			ptr += sizeof("#include ");
			char delimiter = *(ptr - 1);
			if(delimiter == '<')
			delimiter = '>';
			char *dep = (char *)malloc(sizeof(char) * PATH_LEN), *dep_name = dep;
			while ((*dep++ = *ptr++) != delimiter);
			*(dep - 1) = 0;
			old_ptr = ptr;

			if(parent == -1)
				sources[source_id].parts[sources[source_id].parts_n++] = ptr;
			else
				vertexes[parent].parts[vertexes[parent].parts_n++] = ptr;
			
			char *dep_path = (char *)malloc(sizeof(char) * PATH_LEN);
			strcpy(dep_path, dep_name);
			passes++;

			int16_t current_node = find_dep_in_known(dep_name);
			if (current_node == -1) {
				if (find_file_in_includes(dep_path)) {
					current_node = add_vertex(parent, dep_name, dep_path);
					time_accumulate_pause(&files_parsing);
					parse_source(depth + 1, dep_path, current_node, 0);
					time_accumulate_resume(&files_parsing);
					if(parent == -1)
						sources[source_id].deps[sources[source_id].deps_n++] = current_node;
				}
			} else {
				if(parent != -1) {
					vertexes[current_node].parents[vertexes[current_node].n_parents++] = parent;
					vertexes[parent].childs[vertexes[parent].n_childs++] = current_node;
				} else
					sources[source_id].deps[sources[source_id].deps_n++] = current_node;
			}
			if(parent == -1) {
				sources[source_id].all_deps[sources[source_id].all_deps_n++] = current_node;
				// char s[1000];
				// snprintf(s, 100, "%s\n", old_ptr);
				// printf(">> %ld %ld\n%s\n\n", old_ptr - start_ptr, ptrlen - (old_ptr - start_ptr), s);
			}
			else
				vertexes[parent].all_deps[vertexes[parent].all_deps_n++] = current_node;
			free(dep_path);
		}
	}
	if(parent == -1)
		sources[source_id].part_len[sources[source_id].parts_n - 1] = ptrlen - (old_ptr - start_ptr);//vertexes[parent].parts[vertexes[parent].parts_n];
		// sources[source_id].part_len[sources[source_id].parts_n++] = (start_ptr + ptrlen) - old_ptr;//vertexes[parent].parts[vertexes[parent].parts_n];
	else
		vertexes[parent].part_len[vertexes[parent].parts_n - 1] = ptrlen - (old_ptr - start_ptr);//vertexes[parent].parts[vertexes[parent].parts_n];
	total_passes += passes;
	time_accumulate_pause(&files_parsing);	
	// printf("Parse %d, %d depth, file %s, %d passes, %d total, %d tests\n", parses++, depth, input_file, passes,
			// total_passes, find_file_count);
}

void save_includes_dir() {
	DIR *d;
	struct dirent *dir;
	for (uint16_t i = 0; i < include_dirs_count; i++) {
		d = opendir(include_dirs[i]);
		if (d) {
			while ((dir = readdir(d)) != NULL)
				if(dir->d_name[0] != '.') {
					strcpy(includes[includes_count].name, dir->d_name);
					strcpy(includes[includes_count].path, include_dirs[i]);
					strcat(includes[includes_count].path, "/");
					strcat(includes[includes_count].path, dir->d_name);
					includes_count++;
				}
			closedir(d);
		}
	}
}

void print_sources_and_deps() {
	printf("\nSources and it's dependencies (only to third dependency level)");
	uint16_t i = 0;
	for (uint16_t i = 0; i < n_sources; i++) {
		printf("\nsource %d: %s has %hu parts and %d dep%s\n", i, sources[i].path, sources[i].parts_n, sources[i].deps_n,
					(sources[i].deps_n == 0 ? "." : (sources[i].deps_n == 1 ? ":" : "s:")));
		for (uint16_t j = 0; j < sources[i].deps_n; j++) {
			printf("|-> %d (%s)\n", sources[i].deps[j], vertexes[sources[i].deps[j]].path);
			for (uint16_t k = 0; k < vertexes[sources[i].deps[j]].n_childs; k++) {
				printf("|   |-> %d (%s)\n", vertexes[sources[i].deps[j]].childs[k],
					vertexes[vertexes[sources[i].deps[j]].childs[k]].path);
				for (uint16_t l = 0; l < vertexes[vertexes[sources[i].deps[j]].childs[k]].n_childs; l++)
					printf("|   |   |-> %d (%s)\n", vertexes[vertexes[sources[i].deps[j]].childs[k]].childs[l],
						vertexes[vertexes[vertexes[sources[i].deps[j]].childs[k]].childs[l]].path);
			}
		}
	}
}

void print_dependencies() {
	printf("\nAll known dependencies tree\n");
	for (uint16_t i = 0; i < n_vertexes; i++) {
		printf("dep %d: %s", i, vertexes[i].path);

		printf(", %d parents ", vertexes[i].n_parents);
		if (vertexes[i].n_parents)
			printf("(%d", vertexes[i].parents[0]);
		for (uint16_t j = 1; j < vertexes[i].n_parents; j++)
			printf(", %d", vertexes[i].parents[j]);
		if (vertexes[i].n_parents)
			printf(")");
		
		printf(", %d childs ", vertexes[i].n_childs);
		if (vertexes[i].n_childs)
			printf("(%d", vertexes[i].childs[0]);
		for (uint16_t j = 1; j < vertexes[i].n_childs; j++)
			printf(", %d", vertexes[i].childs[j]);
		if (vertexes[i].n_childs)
			printf(")");
		
		printf("\n");
	}
	// printf("node %d: %s %s, parent %s\n", i + 1, vertexes[i].name, vertexes[i].path,
		// (vertexes[i].parent != NULL ? (vertexes[i].parent)->path : "NULL"));
	// printf("Buffered files count: %d\n", files_count);
	
	// for(uint16_t i = 0; i < includes_count; i++)
	// 	printf("%s %s\n", includes[i].name, includes[i].path);
	// return 0;
}

void read_sources_list() {
	char *files_list = "./sources_list";
	FILE *f_list = fopen(files_list, "rb");
	fseek(f_list, 0, SEEK_END);
	long lSize = ftell(f_list);
	rewind(f_list);
	files_list_buffer_start = files_list_buffer = (char *)malloc(sizeof(char) * lSize);
	size_t file_size = fread(files_list_buffer, 1, lSize, f_list);
	fclose(f_list);

	char *ch = strtok(files_list_buffer, "\n");
	while (ch != NULL) {
		if(strlen(ch) > 2) {
			// printf("src %s\n", ch);
			strcpy(sources[n_sources].path, ch);
			sources[n_sources].deps_n = 0;
			n_sources++;
		}
		ch = strtok(NULL, "\n");
	}
	free(files_list_buffer);	
}

// char *mystrcat(char* dest, char* src) {
// 	while ((*dest++));
//     while ((*dest++ = *src++));
//     return --dest;
// }

// #define HEAP_ALLOC(var, size) lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]
// static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

void pack_sources_with_deps() {
	// lzo_uint cmp_len = 10000000;
	// uint32_t in_len = 0, out_len = 0;
	// uint8_t *out_buffer = (uint8_t *)malloc((size_t)cmp_len);
	// lzo_init();

	const int max_dst_size = LZ4_compressBound(5000000);
	char* compressed_data = malloc(max_dst_size);

	for(uint16_t i = 0; i < n_sources; i++) {
	// uint16_t i = 0; {
		uint16_t deps[DEPS_COUNT];
		uint16_t n_deps = 0;

		uint16_t leafs[DEPS_COUNT * 10];
		uint16_t b_leafs = 0, e_leafs = 0;
		for(uint16_t j = 0; j < sources[i].deps_n; j++)
		leafs[e_leafs++] = sources[i].deps[j];
		
		time_accumulate_resume(&tree_flattening);
		while(b_leafs != e_leafs && e_leafs < MAX_LEAFS) {
			uint16_t dep = deps[n_deps++] = leafs[b_leafs++];
			for(uint16_t j = 0; j < vertexes[dep].n_childs; j++) {
				bool occured = false;
				uint16_t child_to_push = vertexes[dep].childs[j];
				for(uint16_t k = 0; k < e_leafs; k++)
					if(leafs[k] == child_to_push)
						occured = true;
				if(!occured)
					leafs[e_leafs++] = child_to_push;
			}
		}
		time_accumulate_pause(&tree_flattening);

		time_accumulate_resume(&deps_gluing);
		char *pack = (char *)malloc(sizeof(char) * 10000000), *pack_start = pack, *src;
		// strcpy(pack, files[sources[i].file].content);
		if(0)
			pack += sprintf(pack, "\n%s\t\n%s", sources[i].path, files[sources[i].file].content);
		// src = files[sources[i].file].content;
		// while ((*pack++ = *src++) != 0);
		// pack--;
		for(uint16_t k = 0; k < n_deps; k++) {
			// pack = mystrcat(pack, files[vertexes[k].file].content); //2.629 293 ms
			if(0)
				pack += sprintf(pack, "\n%s\t\n%s", vertexes[k].path, files[vertexes[k].file].content); // 94 ms
			// src = files[vertexes[k].file].content;
			// while ((*pack++ = *src++) != 0);
			// pack--;
		}
		// *++pack = 0;
		time_accumulate_pause(&deps_gluing);


		
		time_accumulate_resume(&deps_lzoing);
		uint32_t in_len = pack - pack_start;
		// const int out_len = LZ4_compress_default(pack_start, compressed_data, in_len, max_dst_size);
		// compressed_data = (char *)realloc(compressed_data, out_len);
		// lzo1x_1_compress((unsigned char *)pack_start, (lzo_uint)in_len, out_buffer, (lzo_uint *)&out_len, wrkmem);
		time_accumulate_pause(&deps_lzoing);
		
		// printf("source %d has %d deps and pack weighs %.3f MB, lzo %.3f MB, efficiency %.3f\n", i, n_deps,
		// 		in_len / 1024.0 / 1024.0, out_len / 1024.0 / 1024.0, 1.0 * out_len / in_len);
		// for(uint16_t k = 0; k < n_deps; k++)
		// 	printf("%d ", deps[k]);
		// printf("\n");
	}

	// for (uint16_t i = 0; i < n_sources; i++)
	// 	printf("> %s\n", files[sources[i].file].path);			
}

void reset_timers() {
	time_accumulate_reset(&find_files_time);
	time_accumulate_reset(&file_load_time);
	time_accumulate_reset(&files_parsing);
	time_accumulate_reset(&tree_flattening);
	time_accumulate_reset(&deps_gluing);
	time_accumulate_reset(&deps_lzoing);
}

int main(int argc, char *argv[]) {
	bool verbose = true;
	verbose = false;

	timer_start();
	reset_timers();

	include_dirs_count = argc - 1;
	for (uint16_t i = 0; i < include_dirs_count; i++)
		strcpy(include_dirs[i], argv[i + 1] + 2);
	
	save_includes_dir();
	read_sources_list();
	timer_tick("Sources reading");
	
	// for (uint16_t i = 0; i < n_sources; i++) {
	uint16_t i = 0; {
		max_depth = 0;
		parse_passes = 0;
		parse_source(1, sources[i].path, -1, i);
		// char text[100];
		// sprintf(text, "Source %d parsing depth %d, passes %d, time", i, max_depth, parse_passes);
		// timer_tick(text);
		// timer_tick("Sources parsing");
	}
	
	printf("%d files seeking spent %.2f ms, %d access tries, %d found, %d not found.\n", find_file_count,
	time_accumulate_read(&find_files_time) / 1000.0, access_file_count, files_found, files_not_found);
	printf("loaded in %.2f ms, parsed in %.2f ms\n", time_accumulate_read(&file_load_time) / 1000.0,
	time_accumulate_read(&files_parsing) / 1000.0);
	timer_tick("Sources parsing");
	
	if (verbose) {
		print_sources_and_deps();
		print_dependencies();
	}
	printf("Resorces: %d vertexes, %d files in cache\n", n_vertexes, files_count);
	
	// pack_sources_with_deps();
	printf("tree flattened in %.2f ms, deps glued in %.2f ms, lzoed in %.2f ms\n", time_accumulate_read(&tree_flattening) / 1000.0,
		time_accumulate_read(&deps_gluing) / 1000.0, time_accumulate_read(&deps_lzoing) / 1000.0);
	timer_tick("Sources packing");
	
	timer_end("Total");

	char s[100000] = "";

	for(uint16_t i = 0; i < sources[0].parts_n; i++) {
		snprintf(s, sources[0].part_len[i] + 4, "XXX\n%s", sources[0].parts[i]);
		printf("> %s\n", s);
		// printf("> %d\n", sources[0].part_len[i]);
	}

	return 0;
}