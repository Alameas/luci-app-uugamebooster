/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _FILE_OFFSET_BITS 64

#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "vtable.h"

#define MAX_RANGES 8
#define MAX_HITS 8

struct range {
	uintptr_t start, end;
	int prot;
};

static struct range scans[MAX_RANGES], relro;
static unsigned scan_count;
static bool loaded;

static void add_range(uintptr_t start, uintptr_t end, int prot)
{
	if (scan_count >= MAX_RANGES || end <= start) return;
	scans[scan_count].start = start;
	scans[scan_count].end = end;
	scans[scan_count].prot = prot;
	scan_count++;
}

static int main_segments(struct dl_phdr_info *info, size_t size, void *data)
{
	(void)size;
	(void)data;

	for (unsigned i = 0; i < info->dlpi_phnum; i++) {
		const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
		uintptr_t start = info->dlpi_addr + ph->p_vaddr;
		uintptr_t end = start + ph->p_memsz;
		int prot;

		if (ph->p_type == PT_GNU_RELRO) {
			relro.start = start;
			relro.end = end;
			continue;
		}

		if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_R)) continue;

		prot = PROT_READ;
		if (ph->p_flags & PF_W) prot |= PROT_WRITE;
		if (ph->p_flags & PF_X) prot |= PROT_EXEC;
		add_range(start, end, prot);
	}

	return 1;
}

static void load_segments(void)
{
	if (loaded) return;

	loaded = true;
	dl_iterate_phdr(main_segments, NULL);
}

static bool in_code(uintptr_t address)
{
	for (unsigned i = 0; i < scan_count; i++) {
		if ((scans[i].prot & PROT_EXEC) && address >= scans[i].start &&
		    address < scans[i].end) {
			return true;
		}
	}

	return false;
}

static unsigned find_string(const char *needle, uintptr_t *hits)
{
	unsigned count = 0;
	size_t length = strlen(needle) + 1;

	for (unsigned i = 0; i < scan_count && count < MAX_HITS; i++) {
		uintptr_t at = scans[i].start;
		uintptr_t end = scans[i].end;

		if (end - at < length) continue;

		while (at < end) {
			unsigned char *found = memmem((const void *)at, end - at, needle, length);

			if (!found) break;
			hits[count++] = (uintptr_t)found;
			if (count == MAX_HITS) break;
			at = (uintptr_t)found + 1;
		}
	}

	return count;
}

static void **vtable_for_name(uintptr_t name, unsigned slots)
{
	const size_t width = sizeof(void *);
	const size_t span = ((size_t)slots + 2) * width;
	uintptr_t types[MAX_HITS];
	unsigned count = 0;

	for (unsigned i = 0; i < scan_count; i++) {
		uintptr_t at;

		if (scans[i].end - scans[i].start < 2 * width) continue;
		for (at = (scans[i].start + width) & ~(width - 1); at + width <= scans[i].end;
		     at += width) {
			void *word;

			memcpy(&word, (const void *)at, width);
			if ((uintptr_t)word == name && count < MAX_HITS) {
				types[count++] = at - width;
			}
		}
	}

	if (!count) return NULL;

	for (unsigned i = 0; i < scan_count; i++) {
		uintptr_t at, end;

		if (scans[i].end - scans[i].start < span) continue;
		end = scans[i].end - (span - width);
		for (at = (scans[i].start + width) & ~(width - 1); at < end; at += width) {
			void *word, *top;
			void **v = (void **)(at + width);

			memcpy(&word, (const void *)at, width);
			for (unsigned t = 0; t < count; t++) {
				if ((uintptr_t)word != types[t]) continue;
				memcpy(&top, (const void *)(at - width), width);
				if (top != NULL) break;
				if (!in_code((uintptr_t)v[0]) || !in_code((uintptr_t)v[1])) {
					break;
				}
				if (!in_code((uintptr_t)v[slots])) break;
				return v;
			}
		}
	}

	return NULL;
}

static int protection(uintptr_t address)
{
	int prot = PROT_READ;

	for (unsigned i = 0; i < scan_count; i++) {
		if (address >= scans[i].start && address < scans[i].end) {
			prot = scans[i].prot;
			break;
		}
	}

	if (relro.end > relro.start && address >= relro.start && address < relro.end) {
		prot &= ~PROT_WRITE;
	}

	return prot;
}

void *uu_vtable_find(const char *typeinfo, unsigned slots)
{
	uintptr_t hits[MAX_HITS];
	unsigned count;

	load_segments();
	if (!scan_count) return NULL;

	count = find_string(typeinfo, hits);
	for (unsigned i = 0; i < count; i++) {
		void **vtable = vtable_for_name(hits[i], slots);

		if (vtable) return vtable;
	}

	return NULL;
}

bool uu_vtable_patch(void *table, unsigned slot, void *function)
{
	void **vtable = table;
	long pagesize = sysconf(_SC_PAGESIZE);
	uintptr_t page;
	int prot;

	if (!vtable || pagesize <= 0) return false;

	page = (uintptr_t)&vtable[slot] & ~((uintptr_t)pagesize - 1);
	prot = protection((uintptr_t)&vtable[slot]);
	if (mprotect((void *)page, (size_t)pagesize, prot | PROT_WRITE) < 0) return false;

	__atomic_store_n(&vtable[slot], function, __ATOMIC_RELEASE);
	mprotect((void *)page, (size_t)pagesize, prot);
	return true;
}
