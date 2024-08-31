#include "floppy.hpp"

#include <sys/stat.h>
#include <sys/mman.h>
#include <algorithm>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
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
    this->mapped_image = (uint8_t*)mmap(0, this->byte_size, PROT_READ|PROT_WRITE, MAP_SHARED, this->image_fd, 0);
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

namespace {
    struct __attribute__((__packed__)) fat_dirent {
        uint8_t name[11];
        uint8_t attr;
        uint8_t zero[10];
        uint16_t time;
        uint16_t date;
        uint16_t first_allocation_unit;
        uint32_t file_size;
    };

    struct FAT_Walker {
        const uint8_t *fat;
        size_t cur;

        FAT_Walker(const uint8_t *fat, size_t start)
        :fat(fat), cur(start)
        {
        }

        size_t current() const {
            return cur - 2;
        }

        size_t read_fat() {
            size_t byte_pos = (cur / 2) * 3;
            if ((cur & 1) == 0) {
                /* even */
                return fat[byte_pos+0] | ((fat[byte_pos+1]&0xf)<<8);
            } else {
                /* odd */
                return (fat[byte_pos+1]>>4) | ((fat[byte_pos+2])<<4);
            }
        }

        bool next() {
            size_t cur_val = read_fat();
            if (cur_val == 0xfff) {
                return false;
            }

            cur = cur_val;
            return true;
        }
    };
}


std::optional<std::vector<uint8_t>> Floppy::read(const std::string &filename,
                                                 const std::string &ext)
{
    size_t bps = bpb.bytes_per_sector;
    size_t root_dir_offset = bps * (bpb.reserved_sectors + bpb.number_of_fat);
    size_t root_dir_size = bpb.num_root_entries * sizeof(fat_dirent);

    const uint8_t *fat = this->mapped_image + bps * bpb.reserved_sectors;
    const uint8_t *root_dir = fat + bpb.number_of_fat * bps;
    const uint8_t *clusters = root_dir + bpb.num_root_entries * sizeof(fat_dirent);

    char cmp_11[11];
    memset(cmp_11, ' ', 11);
    size_t fnamelen = std::min(filename.size(), (size_t)8);
    memcpy(cmp_11, filename.data(), fnamelen);

    size_t extlen = std::min(ext.size(), (size_t)3);
    memcpy(cmp_11+8, ext.data(), extlen);

    struct fat_dirent *d = (fat_dirent*)(this->mapped_image + root_dir_offset);
    bool found = false;

    while (1) {
        if (d->name[0] == 0) {
            break;
        }
        if (d->name[0] == 0xe5) {
            continue;
        }

        if (memcmp(d->name, cmp_11, 11) == 0) {
            found = true;
            break;
        }
        d++;
    }

    if (!found) {
        return std::nullopt;
    }

    FAT_Walker w(fat, d->first_allocation_unit);
    std::vector<uint8_t> ret;
    size_t rem = d->file_size;
    size_t byte_per_cluster = bps * bpb.sectors_per_cluster;

    while (true) {
        size_t rdsz = std::min(rem, byte_per_cluster);
        auto c = w.current();

        size_t last = ret.size();
        ret.resize(last + rdsz);
        auto src = clusters + c*byte_per_cluster;
        memcpy(ret.data() + last, src, std::min(byte_per_cluster,rem));

        rem -= byte_per_cluster;

        if (! w.next()) {
            break;
        }
    }

    return ret;
}
