#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t util_getUUID(void);
void 	   util_gethostname(char *pzStr,int len);

// Fixup for certain compilers
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

// Support-macros for endianess

#define _endian_swap64(x)      \
  ((((x) & 0xff00000000000000ULL) >> 56) | \
   (((x) & 0x00ff000000000000ULL) >> 40) | \
   (((x) & 0x0000ff0000000000ULL) >> 24) | \
   (((x) & 0x000000ff00000000ULL) >>  8) | \
   (((x) & 0x00000000ff000000ULL) <<  8) | \
   (((x) & 0x0000000000ff0000ULL) << 24) | \
   (((x) & 0x000000000000ff00ULL) << 40) | \
   (((x) & 0x00000000000000ffULL) << 56))
#define _endian_swap32(x)      \
  ((((x) & 0xff000000) >> 24) | \
   (((x) & 0x00ff0000) >>  8) | \
   (((x) & 0x0000ff00) <<  8) | \
   (((x) & 0x000000ff) << 24))
#define _endian_swap16(x) \
  ((((x) & 0xff00) >> 8) | \
   (((x) & 0x00ff) << 8))

#ifndef _BIG_ENDIAN
#define be64_to_cpu(x) _endian_swap64(x)
#define be32_to_cpu(x) _endian_swap32(x)
#define be16_to_cpu(x) _endian_swap16(x)
#define le64_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)
#define cpu_to_be64(x) _endian_swap64(x)
#define cpu_to_be32(x) _endian_swap32(x)
#define cpu_to_be16(x) _endian_swap16(x)
#define cpu_to_le64(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)
#else
#define be64_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define be16_to_cpu(x) (x)
#define le64_to_cpu(x) _endian_swap64(x)
#define le32_to_cpu(x) _endian_swap32(x)
#define le16_to_cpu(x) _endian_swap16(x)
#define cpu_to_be64(x) (x)
#define cpu_to_be32(x) (x)
#define cpu_to_be16(x) (x)
#define cpu_to_le64(x) _endian_swap64(x)
#define cpu_to_le32(x) _endian_swap32(x)
#define cpu_to_le16(x) _endian_swap16(x)
#endif

#define net64_to_cpu(x) be64_to_cpu(x)
#define net32_to_cpu(x) be32_to_cpu(x)
#define net16_to_cpu(x) be16_to_cpu(x)
#define cpu_to_net64(x) cpu_to_be64(x)
#define cpu_to_net32(x) cpu_to_be32(x)
#define cpu_to_net16(x) cpu_to_be16(x)


#ifdef __cplusplus
}
#endif
