#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2common/fios2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Collection-wide loose-file replacement layer for PCSA00133.
 *
 * Each translucent FIOS overlay checks the loose directory first and falls
 * back to the game's mounted PSARC when no replacement exists.  The three
 * destination forms cover the path before Flux normalization, the PSARC's
 * rooted virtual path, and its relative virtual path.
 */
#define RC_OVERRIDE_ROOT "ux0:data/rc_override"
#define RC_OVERRIDE_LOG  "ux0:data/rc_loose_override.log"
#define GAME_COUNT 3
#define PATH_FORMS 3
#define OVERLAY_COUNT (GAME_COUNT * PATH_FORMS)

extern int sceFiosOverlayAdd(SceFiosOverlay *overlay,
                             SceFiosOverlayID *out_id);
extern int sceFiosOverlayRemove(SceFiosOverlayID id);

static SceFiosOverlay rc_overlays[OVERLAY_COUNT];
static SceFiosOverlayID rc_overlay_ids[OVERLAY_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

static const char *const rc_overlay_destinations[OVERLAY_COUNT] = {
    "app0:/rc1/", "/rc1/", "rc1/",
    "app0:/rc2/", "/rc2/", "rc2/",
    "app0:/rc3/", "/rc3/", "rc3/"};

static const char *const rc_overlay_sources[OVERLAY_COUNT] = {
    RC_OVERRIDE_ROOT "/rc1/", RC_OVERRIDE_ROOT "/rc1/",
    RC_OVERRIDE_ROOT "/rc1/", RC_OVERRIDE_ROOT "/rc2/",
    RC_OVERRIDE_ROOT "/rc2/", RC_OVERRIDE_ROOT "/rc2/",
    RC_OVERRIDE_ROOT "/rc3/", RC_OVERRIDE_ROOT "/rc3/",
    RC_OVERRIDE_ROOT "/rc3/"};

static void rc_overlay_log(const char *path, int result, int id) {
    SceUID fd = sceIoOpen(RC_OVERRIDE_LOG,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    char line[192];
    int length;
    if (fd < 0)
        return;
    length = snprintf(line, sizeof(line), "%s result=0x%08X id=%d\n", path,
                      (unsigned int)result, id);
    if (length > 0 && length < (int)sizeof(line))
        sceIoWrite(fd, line, (SceSize)length);
    sceIoClose(fd);
}

static void rc_ensure_override_directories(void) {
    sceIoMkdir(RC_OVERRIDE_ROOT, 0777);
    sceIoMkdir(RC_OVERRIDE_ROOT "/rc1", 0777);
    sceIoMkdir(RC_OVERRIDE_ROOT "/rc2", 0777);
    sceIoMkdir(RC_OVERRIDE_ROOT "/rc3", 0777);
}

int module_start(SceSize argc, const void *args) {
    SceUID fd;
    int i;
    static const char heading[] =
        "RC collection loose overrides v1 root=" RC_OVERRIDE_ROOT "\n";
    (void)argc;
    (void)args;

    fd = sceIoOpen(RC_OVERRIDE_LOG,
                   SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, heading, sizeof(heading) - 1u);
        sceIoClose(fd);
    }
    rc_ensure_override_directories();

    for (i = 0; i < OVERLAY_COUNT; ++i) {
        int result;
        SceFiosOverlay *overlay = &rc_overlays[i];
        memset(overlay, 0, sizeof(*overlay));
        overlay->type = SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT;
        overlay->order = (uint8_t)i;
        overlay->pid = 0;
        overlay->id = -1;
        overlay->dst_len =
            (uint16_t)strlen(rc_overlay_destinations[i]);
        overlay->src_len = (uint16_t)strlen(rc_overlay_sources[i]);
        snprintf(overlay->dst, sizeof(overlay->dst), "%s",
                 rc_overlay_destinations[i]);
        snprintf(overlay->src, sizeof(overlay->src), "%s",
                 rc_overlay_sources[i]);
        result = sceFiosOverlayAdd(overlay, &rc_overlay_ids[i]);
        rc_overlay_log(rc_overlay_destinations[i], result,
                       rc_overlay_ids[i]);
    }
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    int i;
    (void)argc;
    (void)args;
    for (i = OVERLAY_COUNT - 1; i >= 0; --i) {
        if (rc_overlay_ids[i] >= 0) {
            sceFiosOverlayRemove(rc_overlay_ids[i]);
            rc_overlay_ids[i] = -1;
        }
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
