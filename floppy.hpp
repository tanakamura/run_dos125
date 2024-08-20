#pragma once 

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
  void *mapped_image;

  Floppy(const std::string &path);
  ~Floppy();

  bool read(void *buf, size_t sz, const std::string &path);
};
