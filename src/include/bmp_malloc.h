#ifndef BMP_MALLOC_H
#define BMP_MALLOC_H

/* malloc and free, compatible with the micropython garbage collector */

void *bmp_malloc(size_t size);
void bmp_free(void *ptr);
void *bmp_realloc(void *ptr, size_t size);
void *bmp_calloc(size_t nmemb, size_t lsize);

#define malloc bmp_malloc
#define free bmp_free
#define realloc bmp_realloc
#define calloc bmp_calloc

#endif
