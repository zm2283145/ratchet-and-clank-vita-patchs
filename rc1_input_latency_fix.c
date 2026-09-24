#include <psp2/kernel/modulemgr.h>
#include <stdint.h>
#include <string.h>
#include <taihen.h>

/* Offsets are relative to executable segment 0 of PCSA00133 rc1.self. */
#define RC1_TEXT_FILESZ 0x00604DA4u
#define RC1_TEXT_MEMSZ  0x00604DACu
#define QUEUE_DEPTH_LOAD_OFFSET 0x0001A160u

/* Replace `ldr r1, [r7, #0x28]` (configured value 2) with `movs r1, #1`.
 * This limits sceGxm's pending display queue to one frame. */
static const uint8_t old_queue_depth_load[] = {0xB9, 0x6A};
static const uint8_t one_pending_frame[] = {0x01, 0x21};

static SceUID queue_depth_injection = -1;

int module_start(SceSize argc, const void *args) {
    tai_module_info_t tai_info;
    SceKernelModuleInfo kernel_info;
    const uint8_t *base;
    (void)argc;
    (void)args;

    memset(&tai_info, 0, sizeof(tai_info));
    tai_info.size = sizeof(tai_info);
    if (taiGetModuleInfo(TAI_MAIN_MODULE, &tai_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    memset(&kernel_info, 0, sizeof(kernel_info));
    kernel_info.size = sizeof(kernel_info);
    if (sceKernelGetModuleInfo(tai_info.modid, &kernel_info) < 0 ||
        !kernel_info.segments[0].vaddr ||
        kernel_info.segments[0].filesz != RC1_TEXT_FILESZ ||
        kernel_info.segments[0].memsz != RC1_TEXT_MEMSZ)
        return SCE_KERNEL_START_SUCCESS;

    base = (const uint8_t *)kernel_info.segments[0].vaddr;
    if (memcmp(base + QUEUE_DEPTH_LOAD_OFFSET, old_queue_depth_load,
               sizeof(old_queue_depth_load)) != 0)
        return SCE_KERNEL_START_SUCCESS;

    queue_depth_injection = taiInjectData(
        tai_info.modid, 0, QUEUE_DEPTH_LOAD_OFFSET, one_pending_frame,
        sizeof(one_pending_frame));

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    if (queue_depth_injection >= 0) {
        taiInjectRelease(queue_depth_injection);
        queue_depth_injection = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
