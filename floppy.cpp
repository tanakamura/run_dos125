#include "floppy.hpp"

#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

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

    this->byte_size = st_buf.st_size;
    this->mapped_image = mmap(0, this->byte_size, PROT_READ|PROT_WRITE, MAP_SHARED, this->image_fd, 0);
    if (this->mapped_image == MAP_FAILED) {
        perror("mmap floppy image");
        exit(1);
    }

    auto bytes = (char*)this->mapped_image;
    auto bpb = (dos_bpb*)(bytes + 11);

    if (bpb->bytes_per_sector == 512) {
        this->bpb = *bpb;
    } else {
        /* original dos 1.25 disk does not have bpb */
        this->bpb.bytes_per_sector = 512;
        this->bpb.sectors_per_cluster = 2;
        this->bpb.reserved_sectors = 1;
        this->bpb.number_of_fat = 2;
        this->bpb.num_root_entries = 112;
        this->bpb.total_sectors = 640;
    }
}

Floppy::~Floppy() {
    close(this->image_fd);
    munmap(this->mapped_image, this->byte_size);
}

bool Floppy::read(void *buf, size_t sz, const std::string &path) {
    size_t root_dir = bpb.bytes_per_sector * (bpb.reserved_sectors + bpb.number_of_fat);
    printf("%x\n", (int)root_dir);
    return true;
}
