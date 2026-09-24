#include <psp2/kernel/modulemgr.h>
#include <stdint.h>
#include <string.h>
#include <taihen.h>

/* The games submit their Bink quad in a 512 x 416 coordinate space.  RC1
 * builds its edges in registers and also has an inset 472 x 400 path.  RC2
 * and RC3 copy a zeroed four-vertex template to the stack and then fill its
 * coordinates.  Center each path instead of widening only its right edge.
 */
typedef enum FmvQuadLayout {
    FMV_QUAD_RC1_REGISTERS,
    FMV_QUAD_STACK_VERTICES
} FmvQuadLayout;

typedef struct FmvGameSpec {
    uint32_t text_filesz;
    uint32_t text_memsz;
    FmvQuadLayout layout;
    uint32_t offsets[4];
} FmvGameSpec;

static const FmvGameSpec fmv_games[] = {
    {0x00604DA4u, 0x00604DACu, FMV_QUAD_RC1_REGISTERS,
     {0x00025E3Cu, 0x00025E46u, 0x00025E54u, 0x00025E5Au}}, /* RC1 */
    {0x00FD58A4u, 0x00FD58ACu, FMV_QUAD_STACK_VERTICES,
     {0x00025418u, 0x0002540Cu, 0x00025414u, 0x00025424u}}, /* RC2 */
    {0x007F93C4u, 0x007F93CCu, FMV_QUAD_STACK_VERTICES,
     {0x00025BD8u, 0x00025BCCu, 0x00025BD4u, 0x00025BE4u}}, /* RC3 */
};

static const uint8_t old_normal_right[] = {0x4F, 0xF4, 0x00, 0x72};
static const uint8_t old_inset_right[] = {0x4F, 0xF4, 0xF6, 0x72};
/* Thumb-2 MOVW r2,#740: round(416 * 16 / 9). */
static const uint8_t old_normal_left[] = {0x4F, 0xF0, 0x00, 0x00};
static const uint8_t old_inset_left[] = {0x14, 0x20};
/* Center 740 units around the original x=256 midpoint: -114..626. */
static const uint8_t wide_normal_right[] = {0x40, 0xF2, 0x72, 0x22};
static const uint8_t wide_normal_left[] = {0x4F, 0xF6, 0x8E, 0x70};
/* Center the 400-high inset path at approximately -100..612. */
static const uint8_t wide_inset_right[] = {0x40, 0xF2, 0x64, 0x22};
static const uint8_t wide_inset_left[] = {0x64, 0x38};

/* RC2/RC3 initialize the 48-byte vertex template to zero before this block.
 * Replace a redundant x=0 store with MOVW r4,#0xff8e (-114 as a halfword),
 * then write r4 only to the two left-edge x fields.  r4 is dead on the
 * successful render path after the texture pointer has been emitted.
 */
static const uint8_t old_stack_right[] = {0x5F, 0xF4, 0x00, 0x72};
static const uint8_t old_stack_load_site[] = {0xAD, 0xF8, 0x08, 0x00};
static const uint8_t old_stack_top_site[] = {0xAD, 0xF8, 0x0A, 0x00};
static const uint8_t old_stack_bottom_site[] = {0xAD, 0xF8, 0x14, 0x00};
static const uint8_t wide_stack_right[] = {0x40, 0xF2, 0x72, 0x22};
static const uint8_t wide_stack_load_left[] = {0x4F, 0xF6, 0x8E, 0x74};
static const uint8_t wide_stack_store_top[] = {0xAD, 0xF8, 0x08, 0x40};
static const uint8_t wide_stack_store_bottom[] = {0xAD, 0xF8, 0x14, 0x40};

static SceUID fmv_injections[4] = {-1, -1, -1, -1};

static void fmv_release_injections(void) {
    int i;
    for (i = 3; i >= 0; --i) {
        if (fmv_injections[i] >= 0) {
            taiInjectRelease(fmv_injections[i]);
            fmv_injections[i] = -1;
        }
    }
}

int module_start(SceSize argc, const void *args) {
    tai_module_info_t tai_info;
    SceKernelModuleInfo kernel_info;
    const uint8_t *base;
    const FmvGameSpec *game = NULL;
    SceUID modid;
    unsigned int i;
    (void)argc;
    (void)args;

    memset(&tai_info, 0, sizeof(tai_info));
    tai_info.size = sizeof(tai_info);
    if (taiGetModuleInfo(TAI_MAIN_MODULE, &tai_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    modid = tai_info.modid;
    memset(&kernel_info, 0, sizeof(kernel_info));
    kernel_info.size = sizeof(kernel_info);
    if (sceKernelGetModuleInfo(modid, &kernel_info) < 0 ||
        !kernel_info.segments[0].vaddr)
        return SCE_KERNEL_START_SUCCESS;

    for (i = 0; i < sizeof(fmv_games) / sizeof(fmv_games[0]); ++i) {
        if (kernel_info.segments[0].filesz == fmv_games[i].text_filesz &&
            kernel_info.segments[0].memsz == fmv_games[i].text_memsz) {
            game = &fmv_games[i];
            break;
        }
    }
    if (game == NULL)
        return SCE_KERNEL_START_SUCCESS;

    base = (const uint8_t *)kernel_info.segments[0].vaddr;
    if (game->layout == FMV_QUAD_RC1_REGISTERS) {
        if (memcmp(base + game->offsets[0], old_normal_right,
                   sizeof(old_normal_right)) != 0 ||
            memcmp(base + game->offsets[1], old_inset_right,
                   sizeof(old_inset_right)) != 0 ||
            memcmp(base + game->offsets[2], old_normal_left,
                   sizeof(old_normal_left)) != 0 ||
            memcmp(base + game->offsets[3], old_inset_left,
                   sizeof(old_inset_left)) != 0)
            return SCE_KERNEL_START_SUCCESS;

        fmv_injections[0] = taiInjectData(
            modid, 0, game->offsets[0], wide_normal_right,
            sizeof(wide_normal_right));
        if (fmv_injections[0] < 0)
            goto fail;
        fmv_injections[1] = taiInjectData(
            modid, 0, game->offsets[1], wide_inset_right,
            sizeof(wide_inset_right));
        if (fmv_injections[1] < 0)
            goto fail;
        fmv_injections[2] = taiInjectData(
            modid, 0, game->offsets[2], wide_normal_left,
            sizeof(wide_normal_left));
        if (fmv_injections[2] < 0)
            goto fail;
        fmv_injections[3] = taiInjectData(
            modid, 0, game->offsets[3], wide_inset_left,
            sizeof(wide_inset_left));
        if (fmv_injections[3] < 0)
            goto fail;
    } else {
        if (memcmp(base + game->offsets[0], old_stack_right,
                   sizeof(old_stack_right)) != 0 ||
            memcmp(base + game->offsets[1], old_stack_load_site,
                   sizeof(old_stack_load_site)) != 0 ||
            memcmp(base + game->offsets[2], old_stack_top_site,
                   sizeof(old_stack_top_site)) != 0 ||
            memcmp(base + game->offsets[3], old_stack_bottom_site,
                   sizeof(old_stack_bottom_site)) != 0)
            return SCE_KERNEL_START_SUCCESS;

        fmv_injections[0] = taiInjectData(
            modid, 0, game->offsets[0], wide_stack_right,
            sizeof(wide_stack_right));
        if (fmv_injections[0] < 0)
            goto fail;
        fmv_injections[1] = taiInjectData(
            modid, 0, game->offsets[1], wide_stack_load_left,
            sizeof(wide_stack_load_left));
        if (fmv_injections[1] < 0)
            goto fail;
        fmv_injections[2] = taiInjectData(
            modid, 0, game->offsets[2], wide_stack_store_top,
            sizeof(wide_stack_store_top));
        if (fmv_injections[2] < 0)
            goto fail;
        fmv_injections[3] = taiInjectData(
            modid, 0, game->offsets[3], wide_stack_store_bottom,
            sizeof(wide_stack_store_bottom));
        if (fmv_injections[3] < 0)
            goto fail;
    }

    return SCE_KERNEL_START_SUCCESS;

fail:
    fmv_release_injections();
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    fmv_release_injections();
    return SCE_KERNEL_STOP_SUCCESS;
}
