#include <stdlib.h>
#include <stdio.h>
#include "capi_std.h"
#include "vm.h"
#include "spylib.h"

static spy_int
std_quit(SpyState* spy) {
	spy->bail = 1;
	return 0;
}

static spy_int
std_alloc(SpyState* spy) {
	spy_int requested_bytes = spy_pop_int(spy);
	if (requested_bytes < 0) {
		spy_push_int(spy, 0);
		return 1;
	}
	requested_bytes += MALLOC_CHUNK; /* for case when requested_bytes == 0 */
	MemoryBlockList* new_list = malloc(sizeof(MemoryBlockList));
	new_list->next = NULL;
	new_list->prev = NULL;
	new_list->block = malloc(sizeof(MemoryBlock));
	new_list->block->bytes = requested_bytes + (MALLOC_CHUNK % requested_bytes);
	MemoryBlockList* head = spy->memory_map;
	MemoryBlockList* next = head->next;
	MemoryBlockList* tail = NULL;
	int found_slot = 0;
	for (MemoryBlockList* i = spy->memory_map; i->next; i = i->next) {
		spy_int pending_addr = i->block->addr + i->block->bytes;
		spy_int delta = i->next->block->addr - pending_addr;
		/* is there enough space to fit the block? */
		if (new_list->block->bytes <= delta) {
			/* found enough space: */
			new_list->next = i->next;
			new_list->prev = i;
			i->next->prev = new_list;
			i->next = new_list->next;
			new_list->block->addr = pending_addr;
			found_slot = 1;
			break;
		}
		if (!i->next->next) {
			tail = i->next;
		}
	}
	/* space wasn't found... append to tail block */
	if (!found_slot) {
		if (tail) {
			tail->next = new_list;
			new_list->prev = tail;
			new_list->block->addr = tail->block->addr + tail->block->bytes;
		} else {
			new_list->block->addr = head->block->addr;
			new_list->prev = head;
			head->next = new_list;
		}
	}
	/* !!!! out of memory !!!! */
	/* TODO defragment when OOM and retry */
	if (new_list->block->addr + new_list->block->bytes >= SIZE_MEMORY) {
		new_list->prev->next = NULL;
		free(new_list->block);
		free(new_list);
		spy_push_int(spy, 0);
	} else {
		spy_push_int(spy, new_list->block->addr);
	}
	return 1;
}

static spy_int
std_delete(SpyState* spy) {
	spy_int addr = spy_pop_int(spy);
	for (MemoryBlockList* i = spy->memory_map; i; i = i->next) {
		if (i->block->addr == addr) {
			if (i->prev) {
				if (i->next) {
					i->next->prev = i->prev;
				}
				i->prev->next = i->next;
			} else {
				if (i->next) {
					spy->memory_map = i->next;
					spy->memory_map->prev = NULL;
				} else {
					spy->memory_map = NULL;
				}
			}
			free(i);
			return 0;
		}
	}
	spy_die("attempt to free an invalid pointer (addr=0x%llX)", addr);
	return 0;	
}

static spy_int
std_assert(SpyState* spy) {
	spy_int cond = spy_pop_int(spy);
	spy_string err = spy_gets(spy, spy_pop_int(spy));
	if (!cond) {
		spy_die(err);
	}
	return 0;
}

SpyCFunc capi_std[] = {
	{"quit", std_quit},
	{"alloc", std_alloc},
	{"delete", std_delete},
	{"assert", std_assert},
	{NULL, NULL}
};
