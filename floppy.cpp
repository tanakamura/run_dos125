#include "floppy.hpp"

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>

Floppy::Floppy(const std::string &path) {
    struct stat st_buf;
    int r = stat(path.c_str(), &st_buf);
    if (r < 0) {
        perror(path.c_str());
        exit(1);
    }

    if (st_buf.st_size == (512*2*40*8)) {
        this->type = 1;
        this->num_sector = 8;
        this->num_head = 2;
        this->num_cylinder = 40;
    } else if (st_buf.st_size == (512*18*2*80)) {
        this->type = 4;
        this->num_sector = 18;
        this->num_head = 2;
        this->num_cylinder = 80;
    } else {
        fprintf(stderr, "unknown floppy size %d\n", (int)st_buf.st_size);
        exit(1);
    }

    this->image_fd = open(path.c_str(), O_RDWR);
    if (this->image_fd < 0) {
        perror(path.c_str());
        exit(1);
    }
};
