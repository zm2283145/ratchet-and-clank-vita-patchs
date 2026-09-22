#include <psp2/kernel/modulemgr.h>
#include <string.h>
#include <stdint.h>
#include <taihen.h>

/* Offsets are relative to executable segment 0 of PCSA00133 rc1.self. */
#define HOOK1 0x00124226u
#define HOOK2 0x00124460u
#define CAVE1 0x005FCAFCu
#define CAVE2 0x005E1194u
#define RC1_TEXT_FILESZ 0x00604DA4u
#define RC1_TEXT_MEMSZ  0x00604DACu

static const uint8_t old_hook1[] = {0x38, 0x1c, 0x0a, 0x68};
static const uint8_t old_hook2[] = {0x03, 0x2c, 0x2f, 0xd1};
static const uint8_t branch1[] = {0xd8, 0xf0, 0x69, 0xb4};
static const uint8_t branch2[] = {0xbc, 0xf0, 0x98, 0xb6};
static const uint8_t code1[] = {
    0x38, 0x1c, 0x0a, 0x68, 0xb4, 0xf8, 0x7c, 0x50,
    0xa4, 0xf8, 0x44, 0x50, 0x27, 0xf7, 0x8f, 0xb3
};
static const uint8_t code2[] = {
    0x00, 0x2e, 0x03, 0xd1, 0x00, 0x21, 0x01, 0x84,
    0x43, 0xf7, 0x92, 0xb1, 0x03, 0x2c, 0x01, 0xd0,
    0x43, 0xf7, 0x8e, 0xb1, 0x43, 0xf7, 0x5c, 0xb1
};
static const uint8_t zero_cave2[sizeof(code2)] = {0};

static SceUID injections[4] = {-1, -1, -1, -1};

static void release_injections(void) {
    int i;
    for (i = 3; i >= 0; --i) {
        if (injections[i] >= 0) {
            taiInjectRelease(injections[i]);
            injections[i] = -1;
        }
    }
}

int module_start(SceSize argc, const void *args) {
    tai_module_info_t tai_info;
    SceKernelModuleInfo kernel_info;
    const uint8_t *base;
    SceUID modid;
    (void)argc;
    (void)args;

    memset(&tai_info, 0, sizeof(tai_info));
    tai_info.size = sizeof(tai_info);
    if (taiGetModuleInfo(TAI_MAIN_MODULE, &tai_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    modid = tai_info.modid;
    memset(&kernel_info, 0, sizeof(kernel_info));
    kernel_info.size = sizeof(kernel_info);
    if (sceKernelGetModuleInfo(modid, &kernel_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    /* Fail closed on the launcher, RC2, RC3, and different RC1 revisions. */
    if (!kernel_info.segments[0].vaddr ||
        kernel_info.segments[0].filesz != RC1_TEXT_FILESZ ||
        kernel_info.segments[0].memsz != RC1_TEXT_MEMSZ)
        return SCE_KERNEL_START_SUCCESS;

    base = (const uint8_t *)kernel_info.segments[0].vaddr;
    if (memcmp(base + HOOK1, old_hook1, sizeof(old_hook1)) != 0 ||
        memcmp(base + HOOK2, old_hook2, sizeof(old_hook2)) != 0 ||
        memcmp(base + CAVE1, zero_cave2, sizeof(code1)) != 0 ||
        memcmp(base + CAVE2, zero_cave2, sizeof(code2)) != 0)
        return SCE_KERNEL_START_SUCCESS;

    /* Install destinations first, then redirect execution to them. */
    injections[0] = taiInjectData(modid, 0, CAVE1, code1, sizeof(code1));
    if (injections[0] < 0) goto fail;
    injections[1] = taiInjectData(modid, 0, CAVE2, code2, sizeof(code2));
    if (injections[1] < 0) goto fail;
    injections[2] = taiInjectData(modid, 0, HOOK1, branch1, sizeof(branch1));
    if (injections[2] < 0) goto fail;
    injections[3] = taiInjectData(modid, 0, HOOK2, branch2, sizeof(branch2));
    if (injections[3] < 0) goto fail;
    return SCE_KERNEL_START_SUCCESS;

fail:
    release_injections();
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    release_injections();
    return SCE_KERNEL_STOP_SUCCESS;
}
