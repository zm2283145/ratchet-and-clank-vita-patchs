/*
 * Unified RC1 Vita plugin.
 *
 * Keep the proven music/looping-audio fixes, RC1 input-latency fix, FMV
 * aspect corrections, and collection-wide loose-file overlays in their own
 * translation units, but expose one taiHEN module so they are loaded and
 * stopped together from a single config entry.
 */

#define module_start rc_loose_overrides_component_start
#define module_stop  rc_loose_overrides_component_stop
#include "rc_loose_overrides.c"
#undef module_start
#undef module_stop

#define module_start rc_fmv_widescreen_component_start
#define module_stop  rc_fmv_widescreen_component_stop
#include "rc_fmv_widescreen.c"
#undef module_start
#undef module_stop

#define module_start rc1_input_latency_component_start
#define module_stop  rc1_input_latency_component_stop
#include "rc1_input_latency_fix.c"
#undef module_start
#undef module_stop

#define module_start rc1_audio_component_start
#define module_stop  rc1_audio_component_stop
#include "rc1_audio_fixes.c"
#undef module_start
#undef module_stop

int module_start(SceSize argc, const void *args) {
    rc_loose_overrides_component_start(argc, args);
    rc_fmv_widescreen_component_start(argc, args);
    rc1_input_latency_component_start(argc, args);
    rc1_audio_component_start(argc, args);
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    rc1_audio_component_stop(argc, args);
    rc1_input_latency_component_stop(argc, args);
    rc_fmv_widescreen_component_stop(argc, args);
    rc_loose_overrides_component_stop(argc, args);
    return SCE_KERNEL_STOP_SUCCESS;
}
