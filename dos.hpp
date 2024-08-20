#pragma once
#include <stdint.h>

struct __attribute__((__packed__)) dos_bpb {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fat;
    uint16_t num_root_entries;
    uint16_t total_sectors;
};
