#include <psp2/ctrl.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <string.h>
#include <taihen.h>

/* Offsets are relative to executable segment 0 of PCSA00133 rc1.self. */
#define HOOK1 0x00124226u
#define HOOK2 0x00124460u
#define STREAM_PREFLIGHT 0x00023FD4u
#define ACTIVE_CHECK_OFFSET 0x00151DC4u
#define CAVE1 0x005FCAFCu
#define CAVE2 0x005E1194u
#define RC1_TEXT_FILESZ 0x00604DA4u
#define RC1_TEXT_MEMSZ  0x00604DACu
#define SOUND_BASE      0x008D0220u
#define SOUND_STRIDE    0x70u
#define SOUND_VOICE     0x70u
#define SOUND_STATE     0x74u
#define SOUND_FLAGS     0x75u
#define SOUND_ID        0x7Eu
#define SOUND_OWNER     0x88u
#define SLOT_COUNT      30
#define RELEASE_DELAY_TICKS 4

static const uint8_t old_hook1[] = {0x38, 0x1c, 0x0a, 0x68};
static const uint8_t old_hook2[] = {0x03, 0x2c, 0x2f, 0xd1};
static const uint8_t old_stream_preflight[] = {0xff, 0xf7, 0x7a, 0xfb};
static const uint8_t old_active_check[] = {0x10, 0xB5, 0x00, 0x29};
/* The real asynchronous open still runs; skip its preceding open/stat/close. */
static const uint8_t skip_stream_preflight[] = {0x01, 0x20, 0x00, 0x21};
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

typedef struct TrackedLoop {
    int32_t voice;
    int16_t sound_id;
    uintptr_t owner;
    uint8_t armed;
} TrackedLoop;

static SceUID injections[5] = {-1, -1, -1, -1, -1};
static tai_hook_ref_t active_check_ref;
static SceUID active_check_hook_uid = -1;
static volatile int fix_running;
static SceUID fix_thread = -1;
static uintptr_t rc1_base;

/* RC1 considers states 1 and 2 active but overlooks state 7 (starting).
 * A second start then replaces the owner's remembered slot, leaving the
 * first looping voice without a matching stop. Preserve all other behavior. */
static int active_check_hook(const void *owner, int index) {
    if (index >= 0 && index < SLOT_COUNT && owner != NULL) {
        const volatile uint8_t *slot =
            (const volatile uint8_t *)(rc1_base + SOUND_BASE +
                                       (uintptr_t)index * SOUND_STRIDE);
        if (slot[SOUND_STATE] == 7u &&
            *(const volatile uintptr_t *)(slot + SOUND_OWNER) ==
                (uintptr_t)owner)
            return 1;
    }
    return TAI_CONTINUE(int, active_check_ref, owner, index);
}

static void release_active_check_hook(void) {
    if (active_check_hook_uid >= 0) {
        taiHookRelease(active_check_hook_uid, active_check_ref);
        active_check_hook_uid = -1;
    }
}

static void release_injections(void) {
    int i;
    for (i = 4; i >= 0; --i) {
        if (injections[i] >= 0) {
            taiInjectRelease(injections[i]);
            injections[i] = -1;
        }
    }
}

static volatile uint8_t *slot_address(int index) {
    return (volatile uint8_t *)(rc1_base + SOUND_BASE +
                                (uintptr_t)index * SOUND_STRIDE);
}

static void clear_tracking(TrackedLoop tracked[SLOT_COUNT]) {
    memset(tracked, 0, sizeof(TrackedLoop) * SLOT_COUNT);
}

static void remember_weapon_loops(TrackedLoop tracked[SLOT_COUNT]) {
    int i;
    for (i = 0; i < SLOT_COUNT; ++i) {
        volatile uint8_t *slot = slot_address(i);
        const uint8_t state = slot[SOUND_STATE];
        const uint8_t flags = slot[SOUND_FLAGS];
        const int16_t sound_id =
            *(const volatile int16_t *)(slot + SOUND_ID);
        const uintptr_t owner =
            *(const volatile uintptr_t *)(slot + SOUND_OWNER);

        if ((state == 1u || state == 2u) && flags == 4u && owner != 0u &&
            (sound_id == 0 || sound_id == 4)) {
            tracked[i].voice =
                *(const volatile int32_t *)(slot + SOUND_VOICE);
            tracked[i].sound_id = sound_id;
            tracked[i].owner = owner;
            tracked[i].armed = 1u;
        }
    }
}

static void stop_stale_weapon_loops(TrackedLoop tracked[SLOT_COUNT]) {
    int i;
    for (i = 0; i < SLOT_COUNT; ++i) {
        volatile uint8_t *slot;
        uint8_t state;
        int32_t voice;
        int16_t sound_id;
        uintptr_t owner;

        if (!tracked[i].armed)
            continue;

        slot = slot_address(i);
        state = slot[SOUND_STATE];
        voice = *(const volatile int32_t *)(slot + SOUND_VOICE);
        sound_id = *(const volatile int16_t *)(slot + SOUND_ID);
        owner = *(const volatile uintptr_t *)(slot + SOUND_OWNER);

        /* Match the complete identity before requesting RC1's native stop. */
        if ((state == 1u || state == 2u) &&
            voice == tracked[i].voice && sound_id == tracked[i].sound_id &&
            owner == tracked[i].owner && slot[SOUND_FLAGS] == 4u)
            slot[SOUND_STATE] = 4u;
    }
    clear_tracking(tracked);
}

static int fix_main(SceSize argc, void *args) {
    TrackedLoop tracked[SLOT_COUNT];
    SceCtrlData pad;
    uint32_t previous_buttons = 0;
    unsigned int release_delay = 0;
    (void)argc;
    (void)args;

    clear_tracking(tracked);
    memset(&pad, 0, sizeof(pad));
    while (fix_running) {
        const int was_firing =
            (previous_buttons & SCE_CTRL_CIRCLE) != 0;
        int is_firing;

        sceCtrlPeekBufferPositive(0, &pad, 1);
        is_firing = (pad.buttons & SCE_CTRL_CIRCLE) != 0;

        if (is_firing && !was_firing) {
            if (release_delay != 0)
                stop_stale_weapon_loops(tracked);
            clear_tracking(tracked);
            release_delay = 0;
        }
        if (is_firing) {
            remember_weapon_loops(tracked);
        } else if (was_firing) {
            release_delay = RELEASE_DELAY_TICKS;
        } else if (release_delay != 0) {
            --release_delay;
            if (release_delay == 0)
                stop_stale_weapon_loops(tracked);
        }

        previous_buttons = pad.buttons;
        sceKernelDelayThread(10000);
    }
    return 0;
}

static void stop_fix_thread(void) {
    fix_running = 0;
    if (fix_thread >= 0) {
        sceKernelWaitThreadEnd(fix_thread, NULL, NULL);
        sceKernelDeleteThread(fix_thread);
        fix_thread = -1;
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
        memcmp(base + STREAM_PREFLIGHT, old_stream_preflight,
               sizeof(old_stream_preflight)) != 0 ||
        memcmp(base + ACTIVE_CHECK_OFFSET, old_active_check,
               sizeof(old_active_check)) != 0 ||
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
    injections[4] = taiInjectData(modid, 0, STREAM_PREFLIGHT,
                                  skip_stream_preflight,
                                  sizeof(skip_stream_preflight));
    if (injections[4] < 0) goto fail;

    rc1_base = (uintptr_t)base;
    active_check_hook_uid = taiHookFunctionOffset(
        &active_check_ref, modid, 0, ACTIVE_CHECK_OFFSET, 1,
        active_check_hook);
    if (active_check_hook_uid < 0) goto fail;
    fix_running = 1;
    fix_thread = sceKernelCreateThread("rc1_audio_fixes", fix_main,
                                       0x10000100, 0x4000, 0, 0, NULL);
    if (fix_thread < 0) goto fail;
    if (sceKernelStartThread(fix_thread, 0, NULL) < 0) {
        sceKernelDeleteThread(fix_thread);
        fix_thread = -1;
        goto fail;
    }
    return SCE_KERNEL_START_SUCCESS;

fail:
    fix_running = 0;
    release_active_check_hook();
    release_injections();
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
    stop_fix_thread();
    release_active_check_hook();
    release_injections();
    return SCE_KERNEL_STOP_SUCCESS;
}
