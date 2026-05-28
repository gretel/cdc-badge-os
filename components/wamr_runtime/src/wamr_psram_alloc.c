/**
 * \file wamr_psram_alloc.c
 * \brief PSRAM-resident allocator hooks for WAMR.
 *
 * All WAMR allocations (runtime structures, module bytecode, linear memory,
 * operand stack) flow through these wrappers, which forward to
 * heap_caps_*(MALLOC_CAP_SPIRAM). The badge's internal DRAM stays untouched
 * by the runtime itself.
 */

#include "esp_heap_caps.h"
#include "wasm_export.h"

#include <stddef.h>
#include <string.h>

static void *wamr_psram_malloc(unsigned int size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void *wamr_psram_realloc(void *ptr, unsigned int size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void wamr_psram_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

void wamr_psram_alloc_fill(RuntimeInitArgs *args)
{
    memset(args, 0, sizeof(*args));
    args->mem_alloc_type = Alloc_With_Allocator;
    args->mem_alloc_option.allocator.malloc_func  = (void *)wamr_psram_malloc;
    args->mem_alloc_option.allocator.realloc_func = (void *)wamr_psram_realloc;
    args->mem_alloc_option.allocator.free_func    = (void *)wamr_psram_free;
}
