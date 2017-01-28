#include <stdlib.h>
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

SpyCFunc capi_std[] = {
	{"quit", std_quit},
	{"alloc", std_alloc},
	{NULL, NULL}
};
