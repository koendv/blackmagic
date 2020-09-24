#include <stdlib.h>
#include <py/mpconfig.h>
#include <py/mperrno.h>
#include <py/misc.h>
#include <py/runtime.h>
#include <errno.h>

#define DEBUG_MALLOC_printf(...) // debug_printf(__VA_ARGS__)

/*
 * This is a copy of the malloc code of nimble, mbedtls, ...
 *
 * Maintain a linked list of malloc'ed memory, linked to the bmp_memory root
 * pointer, so gc.collect() knows these allocations are in use. If you don't do
 * this, bmp will crash when micropython runs the garbage collector.
 */

/******************************************************************************/
// malloc

// Maintain a linked list of heap memory that we've passed to bmp,
// discoverable via the bmp_memory root pointer.

// MP_STATE_PORT(bmp_memory) is a pointer to [next, prev, data...].

void *m_malloc_bmp(size_t size) {
  void **ptr = m_malloc0(size + 2 * sizeof(uintptr_t));
  if (MP_STATE_PORT(bmp_memory) != NULL) {
    MP_STATE_PORT(bmp_memory)[0] = ptr;
  }
  ptr[0] = NULL;
  ptr[1] = MP_STATE_PORT(bmp_memory);
  MP_STATE_PORT(bmp_memory) = ptr;
  return &ptr[2];
}

void m_free_bmp(void *ptr_in) {
  void **ptr = &((void **)ptr_in)[-2];
  if (ptr[1] != NULL) {
    ((void **)ptr[1])[0] = ptr[0];
  }
  if (ptr[0] != NULL) {
    ((void **)ptr[0])[1] = ptr[1];
  } else {
    MP_STATE_PORT(bmp_memory) = ptr[1];
  }
  m_free(ptr);
}

// Check if a ptr is tracked.
// If it isn't, that means that it's from a previous soft-reset cycle.

STATIC bool is_valid_bmp_malloc(void *ptr) {
  DEBUG_MALLOC_printf("NIMBLE is_valid_nimble_malloc(%p)\n", ptr);
  void **search = MP_STATE_PORT(bmp_memory);
  while (search) {
    if (&search[2] == ptr) {
      return true;
    }
    search = (void **)search[1];
  }
  return false;
}

void *bmp_malloc(size_t size) {
  DEBUG_MALLOC_printf("bmp_malloc(%u)\n", (uint)size);
  void *ptr = m_malloc_bmp(size);
  DEBUG_MALLOC_printf("  --> %p\n", ptr);
  return ptr;
}

// Only free if it's still a valid pointer.
void bmp_free(void *ptr) {
  DEBUG_MALLOC_printf("bmp_free(%p)\n", ptr);
  if (ptr && is_valid_bmp_malloc(ptr)) {
    m_free_bmp(ptr);
  }
}

// Only realloc if it's still a valid pointer. Otherwise just malloc.
void *bmp_realloc(void *ptr, size_t size) {
  DEBUG_MALLOC_printf("bmp_realloc(%p, %u)\n", ptr, (uint)size);
  void *ptr2 = bmp_malloc(size);
  if (ptr && is_valid_bmp_malloc(ptr)) {
    // If it's a realloc and we still have the old data, then copy it.
    // This will happen as we add services.
    memcpy(ptr2, ptr, size);
    m_free_bmp(ptr);
  }
  return ptr2;
}

/* from uClibc */
void *bmp_calloc(size_t nmemb, size_t lsize) {
  DEBUG_MALLOC_printf("bmp_calloc(%u, %u)\n", (uint)nmemb, (uint)size);
  void *result;
  size_t size = lsize * nmemb;

  /* guard vs integer overflow, but allow nmemb to fall through and call
   * malloc(0) */
  if (nmemb && lsize != (size / nmemb)) {
    errno = MP_ENOMEM;
    return NULL;
  }
  result = m_malloc_bmp(size);

  /* we have to blank memory ourselves */
  if (result != NULL) {
    memset(result, 0, size);
  }
  return result;
}
// not truncated
