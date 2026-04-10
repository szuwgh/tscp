
#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#include <stdbool.h>

#define FLEXIBLE_ARRAY_MEMBER
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef signed short int16;
typedef unsigned short uint16;
typedef uint8_t u8;
typedef uint64_t usize;
typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef float f32;
typedef double f64;
typedef u8 *data_ptr_t;
typedef u64 block_id_t;
typedef u64 idx_t;
typedef int16 i16;
typedef uint16 u16;

#endif