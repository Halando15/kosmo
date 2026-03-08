/* =============================================================================
 * KOSMO OS — Tipos Básicos del Sistema
 * Archivo : include/types.h
 * Función : Define tipos de datos portables para el kernel.
 *           En un kernel freestanding no hay <stdint.h> estándar,
 *           así que los definimos manualmente.
 * ============================================================================= */

#ifndef KOSMO_TYPES_H
#define KOSMO_TYPES_H

/* -----------------------------------------------------------------------------
 * TIPOS ENTEROS CON TAMAÑO FIJO
 * Para x86 de 32 bits (i686):
 *   char  = 8 bits
 *   short = 16 bits
 *   int   = 32 bits
 *   long  = 32 bits
 * ----------------------------------------------------------------------------- */

/* Enteros sin signo */
typedef unsigned char       uint8_t;    /* 0 a 255 */
typedef unsigned short      uint16_t;   /* 0 a 65535 */
typedef unsigned int        uint32_t;   /* 0 a 4,294,967,295 */
typedef unsigned long long  uint64_t;   /* 0 a 18,446,744,073,709,551,615 */

/* Enteros con signo */
typedef signed char         int8_t;     /* -128 a 127 */
typedef signed short        int16_t;    /* -32768 a 32767 */
typedef signed int          int32_t;    /* -2,147,483,648 a 2,147,483,647 */
typedef signed long long    int64_t;

/* Tipos de tamaño de puntero (32 bits en x86) */
typedef uint32_t            uintptr_t;  /* Puntero como entero sin signo */
typedef int32_t             intptr_t;   /* Puntero como entero con signo */
typedef uint32_t            size_t;     /* Tamaño de objetos en memoria */
typedef int32_t             ssize_t;    /* Tamaño con signo */
typedef int32_t             ptrdiff_t;  /* Diferencia entre punteros */

/* -----------------------------------------------------------------------------
 * TIPOS ESPECIALES
 * ----------------------------------------------------------------------------- */

/* Booleano */
typedef uint8_t             bool;
#define true                1
#define false               0

/* NULL pointer */
#ifndef NULL
#define NULL                ((void*)0)
#endif

/* -----------------------------------------------------------------------------
 * MACROS ÚTILES
 * ----------------------------------------------------------------------------- */

/* Valores máximos y mínimos */
#define UINT8_MAX           255
#define UINT16_MAX          65535
#define UINT32_MAX          4294967295U
#define INT8_MAX            127
#define INT8_MIN            (-128)
#define INT16_MAX           32767
#define INT16_MIN           (-32768)
#define INT32_MAX           2147483647
#define INT32_MIN           (-2147483648)

/* Alineación de memoria */
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/* Tamaño de array en tiempo de compilación */
#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof((arr)[0]))

/* Obtener el offset de un campo dentro de una estructura */
#define offsetof(type, member) ((size_t)&((type*)0)->member)

/* Macros de mínimo y máximo */
#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

/* Bit operations */
#define BIT(n)              (1U << (n))
#define SET_BIT(x, n)       ((x) |=  BIT(n))
#define CLEAR_BIT(x, n)     ((x) &= ~BIT(n))
#define TOGGLE_BIT(x, n)    ((x) ^=  BIT(n))
#define TEST_BIT(x, n)      (((x) >> (n)) & 1)

/* Atributos GCC útiles para el kernel */
#define PACKED              __attribute__((packed))
#define UNUSED              __attribute__((unused))
#define NORETURN            __attribute__((noreturn))
#define ALWAYS_INLINE       __attribute__((always_inline))
#define NOINLINE            __attribute__((noinline))
#define WEAK                __attribute__((weak))

/* Verificación de tamaño en tiempo de compilación */
#define STATIC_ASSERT(cond, msg) \
    typedef char static_assert_##msg[(cond) ? 1 : -1]

/* Verificar que los tipos tienen el tamaño esperado */
STATIC_ASSERT(sizeof(uint8_t)  == 1, uint8_must_be_1_byte);
STATIC_ASSERT(sizeof(uint16_t) == 2, uint16_must_be_2_bytes);
STATIC_ASSERT(sizeof(uint32_t) == 4, uint32_must_be_4_bytes);
STATIC_ASSERT(sizeof(uint64_t) == 8, uint64_must_be_8_bytes);

#endif /* KOSMO_TYPES_H */
