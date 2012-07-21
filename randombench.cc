#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

using namespace std;

#define PAGE_SIZE       4096
#define ITERATIONS      2000

uint64_t now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    struct stat st;

    assert(fstat(fd, &st) == 0);
    assert(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_DONTNEED) == 0);
    assert(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_RANDOM) == 0);

    off_t pages = st.st_size / PAGE_SIZE;

    char buf[PAGE_SIZE];

    uint64_t start = now();

    for (int i = 0; i < ITERATIONS; i++) {
        off_t off = lrand48() % pages;
        assert(pread(fd, buf, PAGE_SIZE, off * PAGE_SIZE) == PAGE_SIZE);
    }

    uint64_t end = now();

    printf("Performed %d reads in %4ldms (%fms/read)\n",
           ITERATIONS, (end - start), double(end - start)/ITERATIONS);

    assert(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_DONTNEED) == 0);
    char *map = static_cast<char*>(mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0));
    assert(map != MAP_FAILED);
    assert(madvise(map, st.st_size, MADV_RANDOM) == 0);

    start = now();
    for (int i = 0; i < ITERATIONS; i++) {
        off_t off = lrand48() % pages;
        memcpy(buf, map + off*PAGE_SIZE, PAGE_SIZE);
    }
    end = now();

    printf("Performed %d memory-mapped reads in %4ldms (%fms/read)\n",
           ITERATIONS, (end - start), double(end - start)/ITERATIONS);
    return 0;
}
