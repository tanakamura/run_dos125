#pragma once

enum {
    DOSIO_STATUS = 1,
    DOSIO_INP = 2,
    DOSIO_OUTP = 3,
    DOSIO_PRINT = 4,
    DOSIO_AUXIN = 5,
    DOSIO_AUXOUT = 6,
    DOSIO_READ = 7,
    DOSIO_WRITE = 8,
    DOSIO_DSKCHG = 9,
    DOSIO_SETDATE = 0xa,
    DOSIO_SETTIME = 0xb,
    DOSIO_GETTIME = 0xc,
    DOSIO_FLUSH = 0xd,
    DOSIO_MAPDEV = 0xe
};
