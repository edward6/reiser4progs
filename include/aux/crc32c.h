#ifndef CRC32C_H
#define CRC32C_H

#include <aal/libaal.h>

uint32_t crc32c_le(uint32_t seed, unsigned char const *data, uint32_t length);

#define crc32c(seed, data, length)			\
  crc32c_le(seed, (unsigned char const *)data, length)
#define reiser4_crc32c crc32c
#endif
