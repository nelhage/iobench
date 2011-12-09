#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>

using namespace std;

#define N_INTS (1 << 20)
#define REPS   10

uint64_t now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

template <class T>
void benchmark(const char *name, const char *path) {
    uint64_t start = now();
    for (int r = 0; r < REPS; r++)
    {
        T file(path);
        uint32_t val;
        for (int i = 0; i < N_INTS; i++) {
            file.read(reinterpret_cast<char*>(&val), sizeof val);
        }
    }
    uint64_t end = now();
    printf("%-20s: %d x read(4) x %d -> %ldms\n",
           name, REPS, N_INTS, end - start);

    start = now();
    for (int r = 0; r < REPS; r++)
    {
        T file(path);
        uint32_t vals[N_INTS];
        file.read(reinterpret_cast<char*>(vals), sizeof vals);
    }
    end = now();
    printf("%-20s: %d x read(4 x %d) -> %ldms\n",
           name, REPS, N_INTS, end - start);
}

class unix_stream {
public:
    unix_stream(const char *fname) {
        fd_ = open(fname, O_RDONLY);
    }

    size_t read(char *buf, size_t bytes) {
        return ::read(fd_, buf, bytes);
    }

    ~unix_stream() {
        close(fd_);
    }
protected:
    int fd_;
};

class stdio_stream {
public:
    stdio_stream(const char *fname) {
        fp_ = fopen(fname, "r");
        setvbuf(fp_, 0, _IOFBF, 1 << 13);
    }

    size_t read(char *buf, size_t bytes) {
        return ::fread(buf, bytes, 1, fp_);
    }

    ~stdio_stream() {
        fclose(fp_);
    }
protected:
    FILE *fp_;
};

class mmap_stream {
public:
    mmap_stream(const char *fname) {
        int fd = open(fname, O_RDONLY);
        map_len_ = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, 0);
        map_ = mmap(NULL, map_len_, PROT_READ, MAP_SHARED, fd, 0);
        fp_ = static_cast<char*>(map_);
        close(fd);
    }

    size_t read(char *buf, size_t bytes) {
        memcpy(buf, fp_, bytes);
        fp_ += bytes;
    }

    ~mmap_stream() {
        munmap(map_, map_len_);
    }
protected:
    void *map_;
    size_t map_len_;
    char *fp_;
};

int main(void) {
    char fname[] = "iobench.XXXXXX";
    int fd;
    if ((fd = mkstemp(fname)) < 0)
        return 1;
    close(fd);

    {
        ofstream out(fname);
        for (int i = 0; i < N_INTS; i++) {
            uint32_t val = (rand() << 16) ^ rand();
            out.write(reinterpret_cast<char*>(&val), sizeof val);
        }
    }

    benchmark<ifstream>("ifstream", fname);
    benchmark<unix_stream>("unix", fname);
    benchmark<stdio_stream>("stdio", fname);
    benchmark<mmap_stream>("mmap", fname);

    unlink(fname);

    return 0;
}
