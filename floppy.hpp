#pragma once 

#include <string>

struct Floppy {
  int image_fd;

  int type;
  int num_sector;
  int num_head;
  int num_cylinder;

  Floppy(const std::string &path);
};
