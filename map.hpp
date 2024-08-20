static constexpr int DOS_IO_SEG = 0x60;
static constexpr int INITTAB = 16 * 3;
static constexpr int DRVPARAM = 16 * 3 + 8;
static constexpr int DOS_IO_SIZE = 128;
static constexpr int dosseg = dos_io_seg + DOS_IO_SIZE / 16;
