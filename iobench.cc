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
#include <assert.h>
#include <map>

using namespace std;

#define N_INTS (1 << 20)
#define REPS   10

map<string, size_t> iostats() {
    char buf[4096];
    int fd;
    size_t bytes;
    map<string, size_t> out;

    fd = open("/proc/self/io", O_RDONLY);
    bytes = read(fd, buf, sizeof buf);
    close(fd);

    char *p = buf, *e;
    while (p < buf + bytes) {
        e = strchr(p, ':');
        string key(p, e - p);
        size_t val = strtoul(e + 1, &e, 10);
        assert(*e == '\n');
        out[key] = val;
        p = e + 1;
    }
    return out;
}

uint64_t now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

template <class T>
void benchmark(const char *name, const char *path) {
    map<string, size_t> init_stats = iostats();
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
    map<string, size_t> stats = iostats();

    printf("%-20s: %d x read(4) x %d -> %4ldms (%-8d x read(2))\n",
           name, REPS, N_INTS, end - start,
           /* -1  for the read() in iostats() */
           int(stats["syscr"] - init_stats["syscr"] - 1));


    init_stats = iostats();
    start = now();
    for (int r = 0; r < REPS; r++)
    {
        T file(path);
        uint32_t vals[N_INTS];
        file.read(reinterpret_cast<char*>(vals), sizeof vals);
    }
    end = now();
    stats = iostats();
    printf("%-20s: %d x read(4 x %d) -> %4ldms (%-8d x read(2))\n",
           name, REPS, N_INTS, end - start,
           int(stats["syscr"] - init_stats["syscr"] - 1));
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
