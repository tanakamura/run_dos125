#pragma once 

#include <optional>
#include <stdint.h>
#include <vector>
#include <string>
#include "dos.hpp"

struct Floppy {
  int image_fd;

  int type;
  int num_sector;
  int num_head;
  int num_cylinder;
  size_t byte_size;

  dos_bpb bpb;
  uint8_t *mapped_image;

  Floppy(const std::string &path);
  ~Floppy();

  std::optional<std::vector<uint8_t>> read(const std::string &filename,
                                           const std::string &ext);
};
