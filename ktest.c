/* =============================================================================
 * KOSMO OS — Suite de Tests del Kernel
 * Archivo : tests/ktest.c
 * ============================================================================= */

#include "ktest.h"
#include "string.h"
#include "stdio.h"
#include "kosmofs.h"
#include "pit.h"
#include "vga.h"
#include "types.h"

/* Contadores globales */
uint32_t    ktest_passed = 0;
uint32_t    ktest_failed = 0;
uint32_t    ktest_total  = 0;
const char* ktest_current_suite = "";

/* =============================================================================
 * SUITE: STRING
 * ============================================================================= */
void ktest_suite_string(void) {
    KTEST_SUITE("libc/string");

    /* strlen */
    KTEST_EQ(strlen(""),          0);
    KTEST_EQ(strlen("hola"),      4);
    KTEST_EQ(strlen("Kosmo OS"),  8);

    /* strcmp */
    KTEST_ASSERT(strcmp("abc", "abc") == 0);
    KTEST_ASSERT(strcmp("abc", "abd")  < 0);
    KTEST_ASSERT(strcmp("abd", "abc")  > 0);
    KTEST_ASSERT(strcmp("", "")       == 0);

    /* strncmp */
    KTEST_ASSERT(strncmp("abcdef", "abcxyz", 3) == 0);
    KTEST_ASSERT(strncmp("abcdef", "abcxyz", 4) != 0);

    /* strcpy */
    char buf[32];
    strcpy(buf, "Kosmo");
    KTEST_STREQ(buf, "Kosmo");

    /* strncpy */
    strncpy(buf, "Hello World", 5);
    buf[5] = '\0';
    KTEST_STREQ(buf, "Hello");

    /* strcat */
    strcpy(buf, "Hello");
    strcat(buf, " OS");
    KTEST_STREQ(buf, "Hello OS");

    /* strchr */
    const char* p = strchr("Kosmo OS", 'O');
    KTEST_ASSERT(p != NULL);
    KTEST_ASSERT(*p == 'O');
    KTEST_ASSERT(strchr("Kosmo", 'z') == NULL);

    /* strstr */
    KTEST_ASSERT(strstr("Kosmo OS", "OS") != NULL);
    KTEST_ASSERT(strstr("Kosmo OS", "XY") == NULL);

    /* memset */
    memset(buf, 0xAB, 8);
    bool all_ab = true;
    for (int i = 0; i < 8; i++) if ((uint8_t)buf[i] != 0xAB) all_ab = false;
    KTEST_ASSERT(all_ab);

    /* memcpy */
    char src[] = "TestData";
    char dst[16];
    memcpy(dst, src, 9);
    KTEST_STREQ(dst, "TestData");

    /* memcmp */
    KTEST_ASSERT(memcmp("abc", "abc", 3) == 0);
    KTEST_ASSERT(memcmp("abc", "abd", 3) != 0);

    /* atoi */
    KTEST_EQ(atoi("0"),     0);
    KTEST_EQ(atoi("42"),    42);
    KTEST_EQ(atoi("-7"),    -7);
    KTEST_EQ(atoi("1024"),  1024);
    KTEST_EQ(atoi("  99"),  99);

    /* itoa */
    char ibuf[16];
    itoa(0,    ibuf, 10); KTEST_STREQ(ibuf, "0");
    itoa(255,  ibuf, 16); KTEST_STREQ(ibuf, "ff");
    itoa(1024, ibuf, 10); KTEST_STREQ(ibuf, "1024");
    itoa(-42,  ibuf, 10); KTEST_STREQ(ibuf, "-42");
}

/* =============================================================================
 * SUITE: STDIO (kprintf / ksprintf)
 * ============================================================================= */
void ktest_suite_stdio(void) {
    KTEST_SUITE("libc/stdio");

    char out[128];

    /* %d */
    ksprintf(out, "%d", 42);        KTEST_STREQ(out, "42");
    ksprintf(out, "%d", -100);      KTEST_STREQ(out, "-100");
    ksprintf(out, "%d", 0);         KTEST_STREQ(out, "0");

    /* %u */
    ksprintf(out, "%u", 65535);     KTEST_STREQ(out, "65535");

    /* %x */
    ksprintf(out, "%x", 0xFF);      KTEST_STREQ(out, "ff");
    ksprintf(out, "%X", 0xFF);      KTEST_STREQ(out, "FF");
    ksprintf(out, "%x", 0xDEAD);    KTEST_STREQ(out, "dead");

    /* %s */
    ksprintf(out, "%s", "hello");   KTEST_STREQ(out, "hello");
    ksprintf(out, "%s", "");        KTEST_STREQ(out, "");

    /* %c */
    ksprintf(out, "%c", 'K');       KTEST_STREQ(out, "K");

    /* Combinado */
    ksprintf(out, "v%d.%d", 0, 1);  KTEST_STREQ(out, "v0.1");
    ksprintf(out, "0x%08X", 0x1234ABCD);
    KTEST_STREQ(out, "0x1234ABCD");

    /* Zero-padding */
    ksprintf(out, "%05d", 42);      KTEST_STREQ(out, "00042");
    ksprintf(out, "%04x", 0xFF);    KTEST_STREQ(out, "00ff");

    /* %% */
    ksprintf(out, "100%%");         KTEST_STREQ(out, "100%");
}

/* =============================================================================
 * SUITE: KOSMOFS
 * ============================================================================= */
void ktest_suite_kosmofs(void) {
    KTEST_SUITE("KosmoFS");

    /* El FS ya está montado desde kernel_main */
    KTEST_ASSERT(kfs_is_mounted());

    /* Directorio raíz existe */
    KTEST_ASSERT(kfs_exists("/"));

    /* Directorios del sistema */
    KTEST_ASSERT(kfs_exists("/etc"));
    KTEST_ASSERT(kfs_exists("/bin"));
    KTEST_ASSERT(kfs_exists("/home"));
    KTEST_ASSERT(kfs_exists("/tmp"));
    KTEST_ASSERT(kfs_exists("/boot"));

    /* Archivos del sistema */
    KTEST_ASSERT(kfs_exists("/etc/hostname"));
    KTEST_ASSERT(kfs_exists("/etc/motd"));
    KTEST_ASSERT(kfs_exists("/etc/config"));

    /* Inodo raíz */
    int root = kfs_resolve_path("/");
    KTEST_EQ(root, 0);

    /* Ruta inexistente */
    KTEST_ASSERT(kfs_resolve_path("/nonexistent") < 0);
    KTEST_ASSERT(kfs_resolve_path("/etc/ghost.txt") < 0);

    /* Escritura y lectura */
    int r = kfs_write("/tmp/test1.txt", "hello world", 11);
    KTEST_EQ(r, KFS_OK);
    KTEST_ASSERT(kfs_exists("/tmp/test1.txt"));
    KTEST_EQ(kfs_get_size("/tmp/test1.txt"), 11);

    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rd = kfs_read("/tmp/test1.txt", rbuf, sizeof(rbuf));
    KTEST_EQ(rd, 11);
    KTEST_STREQ(rbuf, "hello world");

    /* Sobreescritura */
    kfs_write("/tmp/test1.txt", "overwritten!", 12);
    KTEST_EQ(kfs_get_size("/tmp/test1.txt"), 12);
    memset(rbuf, 0, sizeof(rbuf));
    kfs_read("/tmp/test1.txt", rbuf, sizeof(rbuf));
    KTEST_STREQ(rbuf, "overwritten!");

    /* Append */
    kfs_write("/tmp/test2.txt", "line1\n", 6);
    kfs_append("/tmp/test2.txt", "line2\n", 6);
    KTEST_EQ(kfs_get_size("/tmp/test2.txt"), 12);

    /* Crear directorio */
    r = kfs_mkdir("/tmp/subdir");
    KTEST_EQ(r, KFS_OK);
    KTEST_ASSERT(kfs_exists("/tmp/subdir"));

    /* Doble creación → error EXISTS */
    r = kfs_mkdir("/tmp/subdir");
    KTEST_EQ(r, KFS_ERR_EXISTS);

    /* Borrar archivo */
    kfs_write("/tmp/todel.txt", "x", 1);
    KTEST_ASSERT(kfs_exists("/tmp/todel.txt"));
    r = kfs_delete("/tmp/todel.txt");
    KTEST_EQ(r, KFS_OK);
    KTEST_ASSERT(!kfs_exists("/tmp/todel.txt"));

    /* Limpiar tests */
    kfs_delete("/tmp/test1.txt");
    kfs_delete("/tmp/test2.txt");
    kfs_delete("/tmp/subdir");

    /* Recursos libres */
    KTEST_ASSERT(kfs_free_inodes() > 0);
    KTEST_ASSERT(kfs_free_blocks() > 0);
}

/* =============================================================================
 * SUITE: MEMORIA
 * ============================================================================= */
void ktest_suite_memory(void) {
    KTEST_SUITE("Memory");

    /* Verificar que el kernel está en la dirección correcta */
    extern uint32_t _kernel_end;
    uint32_t kend = (uint32_t)(uintptr_t)&_kernel_end;
    KTEST_ASSERT(kend > 0x100000);  /* Kernel empieza en 1MB */
    KTEST_ASSERT(kend < 0x400000);  /* Kernel < 4MB (razonable) */

    /* Stack activo y accesible */
    uint32_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    KTEST_ASSERT(esp > 0x80000);    /* Stack en zona alta */
    KTEST_ASSERT(esp < 0xA0000);    /* Stack debajo del área de video */

    /* VGA buffer accesible */
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    uint16_t saved = vga[0];
    vga[0] = 0x0741;   /* 'A' gris */
    KTEST_ASSERT(vga[0] == 0x0741);
    vga[0] = saved;

    /* Escritura/lectura en zona baja de RAM */
    volatile uint32_t* ptr = (volatile uint32_t*)0x10000;
    *ptr = 0xDEADBEEF;
    KTEST_ASSERT(*ptr == 0xDEADBEEF);
    *ptr = 0;

    /* Alineación de structs críticos */
    KTEST_ASSERT(sizeof(kfs_inode_t) == 128);   /* Potencia de 2 */
}

/* =============================================================================
 * SUITE: PIT / TIMER
 * ============================================================================= */
void ktest_suite_pit(void) {
    KTEST_SUITE("PIT Timer");

    uint64_t t0 = pit_get_ticks();
    KTEST_ASSERT(t0 > 0);   /* PIT lleva corriendo desde el arranque */

    /* Esperar 100ms y verificar que avanzan los ticks */
    sleep_ms(100);
    uint64_t t1 = pit_get_ticks();
    uint64_t delta = t1 - t0;
    /* A 100Hz, 100ms = ~10 ticks (toleramos ±3) */
    KTEST_ASSERT(delta >= 7 && delta <= 15);

    /* pit_get_millis */
    uint64_t ms = pit_get_millis();
    KTEST_ASSERT(ms > 0);

    /* sleep_ms es aproximado: al menos espera el tiempo indicado */
    uint64_t before = pit_get_millis();
    sleep_ms(50);
    uint64_t after = pit_get_millis();
    KTEST_ASSERT(after - before >= 40);
}

/* =============================================================================
 * SUITE: GDT / IDT (verificación estructural)
 * ============================================================================= */
void ktest_suite_gdt_idt(void) {
    KTEST_SUITE("GDT / IDT");

    /* Leer GDTR */
    struct { uint16_t limit; uint32_t base; } PACKED gdtr;
    __asm__ volatile("sgdt %0" : "=m"(gdtr));
    KTEST_ASSERT(gdtr.base  != 0);
    KTEST_ASSERT(gdtr.limit >= 47);    /* Al menos 6 entradas × 8 - 1 */

    /* Leer IDTR */
    struct { uint16_t limit; uint32_t base; } PACKED idtr;
    __asm__ volatile("sidt %0" : "=m"(idtr));
    KTEST_ASSERT(idtr.base  != 0);
    KTEST_ASSERT(idtr.limit == 0x7FF); /* 256 entradas × 8 - 1 */

    /* Segmentos: CS debe ser 0x08 (selector kernel code) */
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    KTEST_EQ(cs, 0x08);

    /* DS debe ser 0x10 (selector kernel data) */
    uint16_t ds;
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    KTEST_EQ(ds, 0x10);

    /* Verificar que el modo protegido está activo (CR0 bit 0 = 1) */
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    KTEST_ASSERT(cr0 & 0x1);
}

/* =============================================================================
 * PUNTO DE ENTRADA — Ejecutar todas las suites
 * ============================================================================= */
int ktest_run_all(void) {
    ktest_passed = 0;
    ktest_failed = 0;
    ktest_total  = 0;

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("\n  ╔══════════════════════════════════════╗\n");
    kprintf("  ║   KOSMO OS — Kernel Test Suite       ║\n");
    kprintf("  ╚══════════════════════════════════════╝\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);

    ktest_suite_gdt_idt();
    ktest_suite_memory();
    ktest_suite_string();
    ktest_suite_stdio();
    ktest_suite_pit();
    ktest_suite_kosmofs();

    ktest_print_summary();
    return (ktest_failed == 0) ? 0 : 1;
}

void ktest_print_summary(void) {
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  ── Test Summary ────────────────────────\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("  Passed : %u\n", ktest_passed);
    if (ktest_failed > 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
    kprintf("  Failed : %u\n", ktest_failed);
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    kprintf("  Total  : %u\n", ktest_total);
    kprintf("  ────────────────────────────────────────\n");

    if (ktest_failed == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  ✓  ALL TESTS PASSED\n\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  ✗  %u TEST(S) FAILED\n\n", ktest_failed);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
}
