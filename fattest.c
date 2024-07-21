#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if 1 // don't write any changes
#define FOPEN_MODE "rb"
typedef struct {
	void *next; uint32_t pos; uint8_t buf[512];
} ram_cache_t;
static ram_cache_t* ram_cache_last;
static uint8_t *ram_find_sec(uint32_t pos) {
	ram_cache_t *next = ram_cache_last;
	while (next) {
		if (next->pos == pos) return next->buf;
		next = next->next;
	}
	return NULL;
}
static void ram_add_sec(uint32_t pos, uint8_t *buf) {
	ram_cache_t *next = malloc(sizeof(ram_cache_t));
	if (!next) {
		printf("!!! malloc failed\n");
		exit(1);
	}
	next->next = ram_cache_last;
	next->pos = pos;
	memcpy(next->buf, buf, 512);
	ram_cache_last = next;
}
#define FAT_READ_SYS \
	uint8_t *cache; \
	if (0) printf("fat: read 0x%llx\n", (long long)sector << 9); \
	cache = ram_find_sec(sector); \
	if (cache) memcpy(buf, cache, 512); \
	else { \
		if (fseek((FILE*)fatdata->file, (uint64_t)sector << 9, SEEK_SET)) break; \
		if (fread(buf, 1, 512, (FILE*)fatdata->file) != 512) break; \
	}

#define FAT_WRITE_SYS \
	uint8_t *cache; \
	if (1) printf("fat: write 0x%llx\n", (long long)sector << 9); \
	cache = ram_find_sec(sector); \
	if (cache) memcpy(cache, buf, 512); \
	else ram_add_sec(sector, buf);
#else
#define FOPEN_MODE "rb+"
#define FAT_READ_SYS \
	if (0) printf("fat: read 0x%llx\n", (long long)sector << 9); \
	if (fseek((FILE*)fatdata->file, (uint64_t)sector << 9, SEEK_SET)) break; \
	if (fread(buf, 1, 512, (FILE*)fatdata->file) != 512) break;

#define FAT_WRITE_SYS \
	if (1) printf("fat: write 0x%llx\n", (long long)sector << 9); \
	if (fseek((FILE*)fatdata->file, (uint64_t)sector << 9, SEEK_SET)) break; \
	if (fwrite(buf, 1, 512, (FILE*)fatdata->file) != 512) break;
#endif

#include "microfat.h"
#include "fatfile.h"

static int name_cb(void *cbdata, fat_entry_t *p, const char *name) {
	uint8_t start = fat_entry_clust(p);
	(void)cbdata;
	printf("\"%s\": attr = 0x%02x, start = 0x%x, size = %u\n",
		name, p->entry.attr, start, p->entry.size);
	return 0;
}

static uint32_t randseed = 12345;
static uint32_t nextrand(uint32_t n) {
	randseed = (randseed * 0x08088405) + 1;
	return ((uint64_t)randseed * n) >> 32;
}

#define ERR_EXIT(...) \
	do { fprintf(stderr, __VA_ARGS__); goto err; } while (0)

int main(int argc, char **argv) {
	fatdata_t *fatdata = &fatdata_glob;
	uint8_t buf[512 * 2];
	unsigned blk_size = 1024;

	if (argc < 2) return 1;
	fatdata->buf = buf;
#if !FAT_WRITE
	fatdata->file = fopen(argv[1], "rb");
#else
	fatdata->file = fopen(argv[1], FOPEN_MODE);
#endif
	if (!fatdata->file) return 1;
	argc -= 1; argv += 1;

	printf("# fat_init\n");
	if (fat_init(fatdata, 0))
		ERR_EXIT("fat_init failed\n");

	while (argc > 1) {
		if (!strcmp(argv[1], "ls")) {
			unsigned clust;
			printf("\n# list\n");
			if (argc <= 2) ERR_EXIT("bad command\n");

			clust = fat_dir_clust(fatdata, argv[2]);
			if (clust) {
				printf("clust = 0x%x\n", clust);
				fat_enum_name(fatdata, clust, &name_cb, NULL);
			}
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "cd")) {
			unsigned clust;
			printf("\n# change dir\n");
			if (argc <= 2) ERR_EXIT("bad command\n");

			clust = fat_dir_clust(fatdata, argv[2]);
			if (clust) {
				printf("curdir = 0x%x\n", clust);
				fatdata->curdir = clust;
			}
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "read")) {
			fatfile_t *fi; FILE *fo;
			printf("\n# read\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
			fi = fat_fopen(argv[2], "rb");
			fo = fopen(argv[3], "wb");
			if (fi) {
				size_t size, i, j;
				uint8_t *buf = malloc(blk_size);
				fat_fseek(fi, 0, SEEK_END);
				size = fat_ftell(fi);
				fat_fseek(fi, 0, SEEK_SET);
				for (i = 0; i < size;) {
					j = fat_fread(buf, 1, blk_size, fi);
					fwrite(buf, 1, j, fo); i += j;
					if (j != blk_size) break;
				}
				printf("bytes read = %zu / %zu\n", i, size);
				fat_fclose(fi);
				free(buf);
			}
			if (fo) fclose(fo);
			argc -= 3; argv += 3;

		} else if (!strcmp(argv[1], "write")) {
			FILE *fi; fatfile_t *fo;
			printf("\n# write\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
#if !FAT_WRITE
			ERR_EXIT("unsupported in this build\n");
#else
			fo = fat_fopen(argv[2], "wb");
			fi = fopen(argv[3], "rb");
			if (fi && fo) {
				size_t size, i, j;
				uint8_t *buf = malloc(blk_size);
				fseek(fi, 0, SEEK_END);
				size = ftell(fi);
				fseek(fi, 0, SEEK_SET);
				for (i = 0; i < size;) {
					j = fread(buf, 1, blk_size, fi);
					j = fat_fwrite(buf, 1, j, fo); i += j;
					if (j != blk_size) break;
				}
				printf("bytes written = %zu / %zu\n", i, size);
				free(buf);
			}
			if (fo) fat_fclose(fo);
			if (fi) fclose(fi);
			argc -= 3; argv += 3;
#endif

		} else if (!strcmp(argv[1], "truncate")) {
			fatfile_t *fi; uint32_t new_size;
			printf("\n# truncate\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
#if !FAT_WRITE
			ERR_EXIT("unsupported it this build\n");
#else
			fi = fat_fopen(argv[2], "rb+");
			new_size = strtoul(argv[3], NULL, 0);
			if (fi) {
				printf("size = %u\n", fi->size);
				if (fi->size > new_size) {
					fi->size = new_size;
					printf("new size = %u\n", fi->size);
				}
				fat_fclose(fi);
			}
			argc -= 3; argv += 3;
#endif

		} else if (!strcmp(argv[1], "rm")) {
			fat_entry_t *p;
			printf("\n# remove\n");
			if (argc <= 2) ERR_EXIT("bad command\n");
#if !FAT_WRITE
			ERR_EXIT("unsupported in this build\n");
#else
			p = fat_find_path(fatdata, argv[2]);
			if (!p) printf("file not found\n");
			else if (!(p->entry.attr & FAT_ATTR_DIR))
				fat_delete_entry(fatdata, p);
			else {
				int old_pos = fatdata->buf_pos;
				if (fat_rmdir_check(fatdata, fat_entry_clust(p)))
					printf("directory not empty\n");
				else if (fat_read_sec(fatdata, fatdata->buf, old_pos))
					fat_delete_entry(fatdata, p);
			}
			argc -= 2; argv += 2;
#endif

		} else if (!strcmp(argv[1], "mkdir")) {
			unsigned clust; fat_entry_t *p;
			const char *name;
			printf("\n# make directory\n");
			if (argc <= 2) ERR_EXIT("bad command\n");
#if !FAT_WRITE
			ERR_EXIT("unsupported in this build\n");
#else
			p = fat_find_path(fatdata, argv[2]);
			if (p) printf("dir already exist\n");
			else if (!(name = fatdata->lastname))
				printf("path to firectory not exist\n");
			else {
				clust = fat_make_dir(fatdata, fatdata->lastdir, argv[2]);
				if (clust != FAT_CLUST_ERR)
					printf("dir = 0x%x\n", clust);
				else printf("mkdir failed\n");
			}
			argc -= 2; argv += 2;
#endif

#if 1 // tests
		} else if (!strcmp(argv[1], "test_read")) {
			fatfile_t *fi; FILE *fo;
			printf("\n# test_read\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
			fi = fat_fopen(argv[2], "rb");
			fo = fopen(argv[3], "wb");
			if (fi) {
				size_t size, i, j;
				uint8_t *buf = malloc(blk_size);

				fat_fseek(fi, 0, SEEK_END);
				size = fat_ftell(fi);
				fat_fseek(fi, 0, SEEK_SET);
				for (i = 0; i < size;) {
					size_t n;
					if (nextrand(2)) {
						if (fat_fseek(fi, nextrand(size), SEEK_SET)) break;
						if (fat_fread(buf, 1, 1, fi) != 1) break;
						if (fat_fseek(fi, i, SEEK_SET)) break;
					}
					n = nextrand(blk_size) + 1;
					j = fat_fread(buf, 1, n, fi); i += j;
					if (fo) fwrite(buf, 1, j, fo);
					if (j != n) break;
				}
				printf("bytes read = %zu / %zu\n", i, size);
				fat_fclose(fi);
				free(buf);
			}
			if (fo) fclose(fo);
			argc -= 3; argv += 3;

		} else if (!strcmp(argv[1], "test_write")) {
			FILE *fi; fatfile_t *fo;
			printf("\n# test_write\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
#if !FAT_WRITE
			ERR_EXIT("unsupported in this build\n");
#else
			fo = fat_fopen(argv[2], "wb+");
			fi = fopen(argv[3], "rb");
			if (fi && fo) {
				size_t size, i, j;
				uint8_t *buf = malloc(blk_size);
				fseek(fi, 0, SEEK_END);
				size = ftell(fi);
				fseek(fi, 0, SEEK_SET);
				for (i = 0; i < size;) {
					size_t n;
					if (i && nextrand(2)) {
						if (fat_fseek(fo, nextrand(i), SEEK_SET)) break;
						if (fat_fread(buf, 1, 1, fo) != 1) break;
						if (fat_fseek(fo, i, SEEK_SET)) break;
					}
					n = nextrand(blk_size) + 1;
					j = fread(buf, 1, n, fi);
					j = fat_fwrite(buf, 1, j, fo); i += j;
					if (j != n) break;
				}
				printf("bytes written = %zu / %zu\n", i, size);
				free(buf);
			}
			if (fo) fat_fclose(fo);
			if (fi) fclose(fi);
			argc -= 3; argv += 3;
#endif

		} else if (!strcmp(argv[1], "test_create")) {
			char name[64];
			fatfile_t *fo; unsigned i, n;
			printf("\n# test_create\n");
			if (argc <= 3) ERR_EXIT("bad command\n");
			n = strtoul(argv[3], NULL, 0);
			if (n >> 16) ERR_EXIT("counter too big\n");
			for (i = 0; i < n; i++) {
				snprintf(name, sizeof(name), "%s/test%u.bin", argv[2], i);
				fo = fat_fopen(name, "wb");
				if (!fo) break;
				fat_fclose(fo);
			}
			argc -= 3; argv += 3;
#endif

		} else if (!strcmp(argv[1], "seed")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			randseed = strtoul(argv[2], NULL, 0);
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "blk_size")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			blk_size = strtol(argv[2], NULL, 0);
			if ((int)blk_size < 1) ERR_EXIT("bad blk_size\n");
			argc -= 2; argv += 2;

		} else {
			ERR_EXIT("unknown command\n");
		}
	}
err:
	if (fatdata->file) fclose(fatdata->file);
}
