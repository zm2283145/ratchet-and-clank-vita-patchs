#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <taihen.h>

/* PCSA00133 USA rc3.self only. */
#define RC3_TEXT_FILESZ 0x007F93C4u
#define RC3_TEXT_MEMSZ  0x007F93CCu
#define RC3_DATA_FILESZ 0x000AA754u
#define RC3_DATA_MEMSZ  0x0098724Cu

/* Native RC3 front-end menu.  These are offsets from the RX segment. */
#define MENU_TABLE_OFFSET       0x00221D84u
#define MENU_FRAME_SCALE_MOVW_OFFSET 0x00221C50u
#define MENU_FRAME_SCALE_MOVT_OFFSET 0x00221C5Cu
#define MENU_INIT_COUNT_OFFSET  0x00221E52u
#define MENU_LAYOUT_COUNT_OFFSET 0x00221E70u
#define MENU_TEXT_Y_PAD_MOVW_OFFSET 0x00221E8Au
#define MENU_TEXT_Y_PAD_MOVT_OFFSET 0x00221E8Eu
#define MENU_DIVIDER_COUNT_OFFSET 0x00221F78u
#define MENU_LAST_INDEX_OFFSET  0x00221FE0u
#define MENU_TOUCH_COUNT_OFFSET 0x00222090u
#define MENU_DOWN_COUNT_OFFSET  0x002220ACu
#define MENU_DRAW_COUNT_OFFSET  0x00222260u

#define MENU_TABLE_REF_0_MOVW 0x00221D84u
#define MENU_TABLE_REF_0_MOVT 0x00221D88u
#define MENU_TABLE_REF_1_MOVW 0x0022209Cu
#define MENU_TABLE_REF_1_MOVT 0x002220A4u
#define MENU_TABLE_REF_2_MOVW 0x002220D6u
#define MENU_TABLE_REF_2_MOVT 0x002220DEu
#define MENU_TABLE_REF_3_MOVW 0x0022211Au
#define MENU_TABLE_REF_3_MOVT 0x00222120u
#define MENU_TABLE_REF_4_MOVW 0x00222222u
#define MENU_TABLE_REF_4_MOVT 0x00222226u

/* The original three-record Vita table is in the RW segment. */
#define ORIGINAL_TABLE_DATA_OFFSET 0x0004D7A8u
#define FRAME_ORNAMENT_Y_DATA_OFFSET 0x0004D878u
#define DIVIDER_Y_DATA_OFFSET       0x0004D880u
#define SELECTOR_Y_DATA_OFFSET      0x0004D89Cu

#define CALLBACK_NEW_GAME_OFFSET 0x00221C0Cu
#define CALLBACK_LOAD_GAME_OFFSET 0x00221C14u
#define CALLBACK_OPTIONS_OFFSET  0x00221C34u

#define MENU_NATIVE_INIT_OFFSET    0x00221C48u
#define MENU_NATIVE_UPDATE_OFFSET  0x00221FCEu
#define MENU_NATIVE_CLEANUP_OFFSET 0x00222278u
#define MENU_NATIVE_NOOP_0_OFFSET  0x00222288u
#define MENU_NATIVE_NOOP_1_OFFSET  0x0022228Au
#ifdef RC3_LOBBY_TEST
#define SET_LEVEL_OFFSET                0x0034E35Cu
#define STATE39_NATIVE_UPDATE_OFFSET    0x001EFC2Eu
#define STATE16_NATIVE_UPDATE_OFFSET    0x001EE320u
#define NATIVE_UI_FRAME_OWNER_OFFSET    0x001DD576u
#define FRONTEND_CALLBACKS_RESET_OFFSET 0x0023CCD2u
#define FRONTEND_MODE_SET_OFFSET        0x00101048u
#define GENERAL_GAME_INIT_OFFSET        0x00215AB8u
#define GENERAL_GAME_UPDATE_OFFSET      0x00215D6Cu
#define PLAYER_CONTROLLER_UPDATE_OFFSET 0x000BA9AEu
#define GAMEPLAY_FRAME_OWNER_OFFSET     0x001F1D80u
#define MOBY_DRAW_BUILDER_OFFSET        0x0017E73Au
#define COMMON_LEVEL_LOADER_OFFSET       0x0016F716u
#define ARENA_TRANSITION_OFFSET          0x001E6E46u

/* Internal calls made by the retained transition before control can reach
 * the common loader.  These hooks are installed only when Local Play Start
 * is pressed, after state 39 has completed. */
#define ARENA_STAGE_READY_OFFSET          0x001EA890u
#define ARENA_STAGE_DELAY_OFFSET          0x000E7236u
#define ARENA_STAGE_TASK_RESET_OFFSET     0x0002351Cu
#define ARENA_STAGE_FRONTEND_CLOSE_OFFSET 0x001DF6C4u
#define ARENA_STAGE_SCENE_CLOSE_OFFSET    0x001B964Eu
#define ARENA_STAGE_PREPARE_OFFSET        0x001EA402u
#define ARENA_STAGE_SESSION_OFFSET        0x001B239Eu
#define ARENA_STAGE_STATE_CLEAN_OFFSET    0x0017A356u
#define ARENA_STAGE_RENDER_RESET_OFFSET   0x000FBF6Eu
#define ARENA_STAGE_LEVEL_BIND_OFFSET     0x000B89F4u
#define ARENA_STAGE_CALLBACK_SETUP_OFFSET 0x001EA2FEu
#define ARENA_STAGE_FINALIZE_OFFSET       0x00200D10u
#define ARENA_STAGE_GPU_CLOSE_OFFSET      0x00200852u
#define ARENA_STAGE_GPU_WAIT_OFFSET       0x002007DAu
#define ARENA_STAGE_FRAME_CLOSE_OFFSET    0x000FC090u
#define ARENA_STAGE_FRAME_MODE_OFFSET     0x000FC0B6u
#define ARENA_STAGE_FRAME_SYNC_OFFSET     0x000FBFA8u
#define ARENA_STAGE_FRAME_FLUSH_OFFSET    0x000FBFAAu
#define ARENA_STAGE_LOAD_RESET_OFFSET     0x000E337Cu
#define ARENA_STAGE_SCENE_RESET_OFFSET    0x001BABECu
#define ARENA_STAGE_MP_INIT_OFFSET        0x001F0306u
#define ARENA_STAGE_MP_UPDATE_OFFSET      0x001F035Eu
#define ALLOCATOR_LIST_NEXT_OFFSET        0x0061FBE4u
#define PLAYER_POSITION_SETUP_OFFSET      0x000BBA4Eu
#define PLAYER_OBJECT_CALLBACK_OFFSET     0x00246EACu

/* Exact Vita translations of the PS3 local-session helpers at 0x0023B8E8
 * and 0x0023B628.  The latter keeps the same five-argument contract and
 * return values (-1/-2/-3 on rejected registrations). */
#define RESET_LOCAL_SESSION_OFFSET      0x001B22C0u
#define REGISTER_LOCAL_PLAYER_OFFSET    0x001B20E8u
#define MULTIPLAYER_STATE_GETTER_OFFSET 0x0002EDE0u

/* Surviving Vita ports of the PS3 multiplayer front-end path. */
#define MULTIPLAYER_MODE_STORE_0_OFFSET  0x000011CEu
#define MULTIPLAYER_MODE_STORE_1_OFFSET  0x000011D6u

/* RW offsets from the Vita data segment (runtime base 0x817FA000). */
#define FRONTEND_TRANSITION_DATA_OFFSET      0x003599D0u
#define FRONTEND_REQUEST_DATA_OFFSET         0x003599D4u
#define MAIN_CONTEXT_PTR_DATA_OFFSET         0x00001CA0u
#define TRANSITION_OWNER_SLOTS_DATA_OFFSET   0x00207D50u

#define CURRENT_LEVEL_OFFSET 0x00816464u
#define LOCAL_PLAYER_COUNT_OFFSET 0x00BAE0ACu
#define LOCAL_PLAYER_LIST_OFFSET  0x00BAE08Cu
#define LOCAL_PROFILE_OFFSET      0x00B539F4u
#define PRESENTATION_CURRENT_OFFSET 0x00B539F4u
#define PRESENTATION_PENDING_OFFSET 0x00816870u
#define FRONTEND_SCREEN_39_CALLBACKS_DATA_OFFSET 0x00046AF8u
#define MENU_SELECTION_DATA_OFFSET               0x0004D7D8u

/* Retained native widget operations.  These are exact Vita counterparts of
 * the PS3 operations used by LobbyGUI and the title menu. */
#define NATIVE_UI_LOCALIZE_OFFSET        0x00103294u
#define NATIVE_UI_CREATE_CONTAINER_OFFSET 0x00689C68u
#define NATIVE_UI_CREATE_RECT_OFFSET     0x0020D3B2u
#define NATIVE_UI_CREATE_TEXT_OFFSET     0x0020D27Cu
#define NATIVE_UI_ADD_CHILD_OFFSET       0x0020C76Cu
#define NATIVE_UI_ATTACH_GROUP_OFFSET    0x0020C4D2u
#define NATIVE_UI_DESTROY_GROUP_OFFSET   0x0020C6C6u
#define NATIVE_UI_SET_VISIBLE_OFFSET     0x0020B620u
#define NATIVE_UI_SET_POSITION_OFFSET    0x0020B03Au
#define NATIVE_UI_SET_SCALE_OFFSET       0x0020CA76u
#define NATIVE_UI_SET_ANCHOR_OFFSET      0x0020CB2Au
#define NATIVE_UI_SET_COLOR_OFFSET       0x0020BC50u
#define NATIVE_UI_SET_MODE_OFFSET        0x0020D0E8u
#endif

#define TEXT_LOAD_GAME            0x014Cu
#define TEXT_NEW_GAME             0x014Eu
#define TEXT_MULTIPLAYER          0x11E6u
#define TEXT_MULTIPLAYER_TUTORIAL 0x169Cu
#define TEXT_OPTIONS              0x0107u

typedef void (*MenuCallback)(void);
typedef void (*SetLevelFn)(int level);

typedef struct MenuRecord {
    uint16_t widget_id;
    uint16_t group_id;
    uint32_t text_id;
    uint32_t enabled;
    MenuCallback callback;
} MenuRecord;

typedef char MenuRecordMustBe16Bytes[(sizeof(MenuRecord) == 16) ? 1 : -1];

static const uint8_t expected_original_table[48] = {
    0x0C, 0x00, 0x0D, 0x00, 0x4C, 0x01, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x15, 0x1C, 0x22, 0x81,
    0x0D, 0x00, 0x0D, 0x00, 0x4E, 0x01, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0D, 0x1C, 0x22, 0x81,
    0x10, 0x00, 0x0D, 0x00, 0x07, 0x01, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x35, 0x1C, 0x22, 0x81
};

typedef struct CodePatch {
    uint32_t offset;
    uint8_t size;
    uint8_t expected[4];
    uint8_t replacement[4];
} CodePatch;

static uintptr_t rc3_base;
static uintptr_t rc3_data_base;
static MenuRecord restored_menu[5] __attribute__((aligned(4)));
static SceUID injections[40];
static int injection_count;

#ifdef RC3_LOBBY_TEST
static SceUID rc3_modid = -1;
static tai_hook_ref_t title_update_ref;
static SceUID title_update_hook_uid = -1;
static tai_hook_ref_t state39_update_ref;
static SceUID state39_update_hook_uid = -1;
static tai_hook_ref_t state16_update_ref;
static SceUID state16_update_hook_uid = -1;
static tai_hook_ref_t native_ui_frame_owner_ref;
static SceUID native_ui_frame_owner_hook_uid = -1;
static tai_hook_ref_t general_game_init_ref;
static SceUID general_game_init_hook_uid = -1;
static tai_hook_ref_t general_game_update_ref;
static SceUID general_game_update_hook_uid = -1;
static tai_hook_ref_t player_controller_update_ref;
static SceUID player_controller_update_hook_uid = -1;
static tai_hook_ref_t gameplay_frame_owner_ref;
static SceUID gameplay_frame_owner_hook_uid = -1;
static tai_hook_ref_t moby_draw_builder_ref;
static SceUID moby_draw_builder_hook_uid = -1;
static tai_hook_ref_t common_level_loader_ref;
static SceUID common_level_loader_hook_uid = -1;
static tai_hook_ref_t arena_transition_ref;
static SceUID arena_transition_hook_uid = -1;
static tai_hook_ref_t allocator_list_next_ref;
static SceUID allocator_list_next_hook_uid = -1;
static tai_hook_ref_t player_position_setup_ref;
static SceUID player_position_setup_hook_uid = -1;
static tai_hook_ref_t player_object_callback_ref;
static SceUID player_object_callback_hook_uid = -1;
static tai_hook_ref_t reset_local_session_ref;
static SceUID reset_local_session_hook_uid = -1;
static volatile int arena_roster_repair_active;
static unsigned int player_object_missing_mask;
enum { ARENA_STAGE_COUNT = 14 };
static tai_hook_ref_t arena_stage_refs[ARENA_STAGE_COUNT];
static SceUID arena_stage_hook_uids[ARENA_STAGE_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1
};
static volatile int arena_stage_trace_active;
static int arena_last_mp_update_state = -999;
static int arena_last_mp_update_result = -999;
static volatile int arena_gate_override_active;
static int arena_gate_hero_positioned;
static int arena_post_slot_hero_positioned;
static int arena_game_update_calls;
static int player_controller_update_calls;
static int arena_frame_owner_calls;
static int arena_player_sync_calls;
static int arena_final_record_seeded;
static volatile float arena_controller_position[4];
static volatile int arena_controller_position_valid;
static int arena_visibility_repair_logged;
static void general_game_update_hook(int view);
static void player_controller_update_hook(uintptr_t record);
static void gameplay_frame_owner_hook(void);
static int moby_draw_builder_hook(uintptr_t start, uintptr_t output,
                                  unsigned int count);
/* Keep the next crash dump free of transition-hook side effects.  This is a
 * runtime flag (rather than compiling the hooks out) so the same source keeps
 * the established tracing machinery ready for the following diagnostic. */
static int arena_clean_core_control = 1;
static int allocator_bad_link_seen;
static volatile int lobby_menu_pending;
static volatile int local_menu_pending;
static volatile int title_menu_pending;
static volatile int state39_request_pending;
static volatile int state39_handoff_armed;
static volatile int state39_update_seen;
static volatile int state39_completion_seen;
static volatile int state39_controller_started;
static volatile int state39_activation_frames;
static volatile int state39_arena_launch_pending;
static volatile int lobby_scene_ready;
static volatile int lobby_reopen_pending;
static volatile int arena_target_level = 44;
static volatile int lobby_menu_active;
static volatile int lobby_screen;
static void arena_transition_hook(void);
#endif

static void initialize_restored_table(void);
static void append_log(const char *message);

#ifdef RC3_LOBBY_TEST
static SceUID lobby_worker = -1;
static volatile int lobby_worker_running;
static volatile int lobby_ui_active;
static volatile int lobby_ui_selected;
static volatile int lobby_ui_screen;
static volatile int lobby_native_rebuild_pending;
static volatile int lobby_native_cleanup_pending;
static volatile int lobby_native_style_pending;
static int lobby_native_built;
static unsigned int local_team_index;
static unsigned int local_skin_index;

enum {
    LOBBY_GUI_SCREEN_LOCAL_PROFILES = 0x16,
    LOBBY_GUI_SCREEN_MAIN = 0x18
};

enum {
    LOBBY_NATIVE_PANEL_ID = 0x006C0028,
    LOBBY_NATIVE_ROOT_ID = 0x006C000B,
    LOBBY_NATIVE_MENU_ROOT_ID = 0x006C0025,
    LOBBY_NATIVE_HEADING_ID = 0x006C0100,
    LOBBY_NATIVE_ROW_FIRST_ID = 0x006C0110
};

typedef const char *(*NativeUiLocalizeFn)(int text_id);
typedef void (*NativeUiCreateContainerFn)(int object_id);
typedef void (*NativeUiCreateRectFn)(int object_id, uint32_t color, int flags,
                                     float x, float y, float width,
                                     float height);
typedef void (*NativeUiCreateTextFn)(int object_id, const char *text,
                                     uint32_t color, float x, float y,
                                     float width, float height);
typedef void (*NativeUiPairFn)(int first, int second);
typedef void (*NativeUiTripleFn)(int object_id, int value, int enabled);
typedef void (*NativeUiSetPositionFn)(int object_id, float x, float y);
typedef void (*NativeUiSetScaleFn)(int object_id, float x, float y);
typedef void (*NativeUiSetColorFn)(int object_id, uint32_t color);
typedef void (*NativeUiDestroyGroupFn)(int group);

static void native_lobby_destroy(void) {
    NativeUiDestroyGroupFn destroy_group;

    if (!lobby_native_built)
        return;
    destroy_group = (NativeUiDestroyGroupFn)
        ((rc3_base + NATIVE_UI_DESTROY_GROUP_OFFSET) | 1u);
    destroy_group(0x10);
    destroy_group(0x11);
    lobby_native_built = 0;
    append_log("Native LobbyGUI: widget groups destroyed.\n");
}

static void native_lobby_set_colors(void) {
    NativeUiSetColorFn set_color = (NativeUiSetColorFn)
        ((rc3_base + NATIVE_UI_SET_COLOR_OFFSET) | 1u);
    int count = lobby_ui_screen == LOBBY_GUI_SCREEN_MAIN ? 4 : 5;
    int item;

    if (!lobby_native_built)
        return;
    for (item = 0; item < count; ++item) {
        set_color(LOBBY_NATIVE_ROW_FIRST_ID + item,
                  item == lobby_ui_selected ? 0x66D6EEFAu : 0x8066CCFFu);
    }
}

static void native_lobby_create_text(int object_id, int text_id, float y) {
    NativeUiLocalizeFn localize = (NativeUiLocalizeFn)
        ((rc3_base + NATIVE_UI_LOCALIZE_OFFSET) | 1u);
    NativeUiCreateTextFn create_text = (NativeUiCreateTextFn)
        ((rc3_base + NATIVE_UI_CREATE_TEXT_OFFSET) | 1u);
    NativeUiTripleFn set_visible = (NativeUiTripleFn)
        ((rc3_base + NATIVE_UI_SET_VISIBLE_OFFSET) | 1u);
    NativeUiTripleFn set_mode = (NativeUiTripleFn)
        ((rc3_base + NATIVE_UI_SET_MODE_OFFSET) | 1u);
    NativeUiSetPositionFn set_position = (NativeUiSetPositionFn)
        ((rc3_base + NATIVE_UI_SET_POSITION_OFFSET) | 1u);
    NativeUiSetScaleFn set_scale = (NativeUiSetScaleFn)
        ((rc3_base + NATIVE_UI_SET_SCALE_OFFSET) | 1u);
    NativeUiPairFn set_anchor = (NativeUiPairFn)
        ((rc3_base + NATIVE_UI_SET_ANCHOR_OFFSET) | 1u);

    create_text(object_id, localize(text_id), 0x8066CCFFu,
                0.0f, 0.0f, 1.0f, 1.0f);
    set_visible(object_id, 0x40, 1);
    set_mode(object_id, 1, 1);
    set_scale(object_id, 0.425f, 0.85f);
    set_anchor(object_id, 1);
    set_position(object_id, 0.5f, y);
}

static void native_lobby_rebuild(void) {
    static const int main_text_ids[4] = {
        0x0F56, 0x0F58, 0x18CC, 0x15CE
    };
    int local_text_ids[5];
    const int *text_ids;
    int count;
    int item;
    NativeUiCreateContainerFn create_container = (NativeUiCreateContainerFn)
        ((rc3_base + NATIVE_UI_CREATE_CONTAINER_OFFSET) | 1u);
    NativeUiCreateRectFn create_rect = (NativeUiCreateRectFn)
        ((rc3_base + NATIVE_UI_CREATE_RECT_OFFSET) | 1u);
    NativeUiPairFn add_child = (NativeUiPairFn)
        ((rc3_base + NATIVE_UI_ADD_CHILD_OFFSET) | 1u);
    NativeUiPairFn attach_group = (NativeUiPairFn)
        ((rc3_base + NATIVE_UI_ATTACH_GROUP_OFFSET) | 1u);

    native_lobby_destroy();
    if (lobby_ui_screen == LOBBY_GUI_SCREEN_MAIN) {
        text_ids = main_text_ids;
        count = 4;
    } else {
        local_text_ids[0] = 0x0FF5;
        local_text_ids[1] = 0x0F1A + (int)local_team_index;
        local_text_ids[2] = 0x0F22 + (int)local_skin_index;
        local_text_ids[3] = 0x0FF9;
        local_text_ids[4] = 0x0CD5;
        text_ids = local_text_ids;
        count = 5;
    }

    create_rect(LOBBY_NATIVE_PANEL_ID, 0x402299DEu, 0,
                0.5f, 0.5f, 0.72f, 0.62f);
    create_container(LOBBY_NATIVE_ROOT_ID);
    add_child(LOBBY_NATIVE_ROOT_ID, LOBBY_NATIVE_PANEL_ID);
    create_container(LOBBY_NATIVE_MENU_ROOT_ID);
    native_lobby_create_text(LOBBY_NATIVE_HEADING_ID, TEXT_MULTIPLAYER, 0.25f);
    add_child(LOBBY_NATIVE_MENU_ROOT_ID, LOBBY_NATIVE_HEADING_ID);
    for (item = 0; item < count; ++item) {
        native_lobby_create_text(LOBBY_NATIVE_ROW_FIRST_ID + item,
                                 text_ids[item], 0.38f + item * 0.095f);
        add_child(LOBBY_NATIVE_MENU_ROOT_ID,
                  LOBBY_NATIVE_ROW_FIRST_ID + item);
    }
    attach_group(0x10, LOBBY_NATIVE_ROOT_ID);
    attach_group(0x11, LOBBY_NATIVE_MENU_ROOT_ID);
    lobby_native_built = 1;
    native_lobby_set_colors();
    append_log("Native LobbyGUI: native panel and localized rows created.\n");
}

static void native_lobby_tick(void) {
    if (lobby_native_cleanup_pending) {
        lobby_native_cleanup_pending = 0;
        native_lobby_destroy();
    }
    if (!lobby_ui_active)
        return;
    if (lobby_native_rebuild_pending) {
        lobby_native_rebuild_pending = 0;
        native_lobby_rebuild();
        lobby_native_style_pending = 0;
    } else if (lobby_native_style_pending) {
        lobby_native_style_pending = 0;
        native_lobby_set_colors();
    }
}
#endif

static void append_log(const char *message) {
    SceUID fd;
    sceIoMkdir("ux0:data/rc3mp", 0777);
    fd = sceIoOpen("ux0:data/rc3mp/native_menu.log",
                   SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, message, strlen(message));
        sceIoClose(fd);
    }
}

#ifdef RC3_LOBBY_TEST
typedef uintptr_t (*ArenaStageFn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef uintptr_t (*AllocatorListNextFn)(uintptr_t node);
typedef void (*PlayerPositionSetupFn)(int player, int use_camera);
typedef void (*PlayerObjectCallbackFn)(uintptr_t object);
typedef void (*ResetLocalSessionFn)(int profile);
typedef int (*RegisterLocalPlayerFn)(int player, int kind,
                                     int controller, int flags,
                                     uint8_t *status);

static void reset_local_session_hook(int profile) {
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)reset_local_session_ref;
    struct _tai_hook_user *next = (struct _tai_hook_user *)current->next;
    ResetLocalSessionFn function =
        (ResetLocalSessionFn)(next ? next->func : current->old);
    RegisterLocalPlayerFn register_local_player =
        (RegisterLocalPlayerFn)((rc3_base + REGISTER_LOCAL_PLAYER_OFFSET) | 1u);
    volatile int *count =
        (volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET);
    volatile uintptr_t *list =
        (volatile uintptr_t *)(rc3_base + LOCAL_PLAYER_LIST_OFFSET);
    volatile uintptr_t *profile_source =
        (volatile uintptr_t *)(rc3_base + LOCAL_PROFILE_OFFSET);
    int result;
    char line[192];

    function(profile);
    if (!arena_roster_repair_active)
        return;

    result = register_local_player(0, 1, 0, 0, 0);
    snprintf(line, sizeof(line),
             "Arena roster repair: reset profile=%d register=%d count=%d source=%08lX list0=%08lX.\n",
             profile, result, *count, (unsigned long)*profile_source,
             (unsigned long)list[0]);
    append_log(line);
}

static int install_reset_local_session_repair(void) {
    if (reset_local_session_hook_uid >= 0)
        return 0;
    reset_local_session_hook_uid = taiHookFunctionOffset(
        &reset_local_session_ref, rc3_modid, 0,
        RESET_LOCAL_SESSION_OFFSET, 1, reset_local_session_hook);
    if (reset_local_session_hook_uid < 0) {
        append_log("Arena roster repair: reset hook installation failed.\n");
        return -1;
    }
    append_log("Arena roster repair: reset-boundary hook installed.\n");
    return 0;
}
/* v23 slot-bracket probes.  Single-player level init calls 0x81107BF2 and
 * 0x81108644 before the per-player slot initializer 0x810D924E; multiplayer
 * skips both.  Record slot0+0x50 (player moby), slot0+0x240 (the missing
 * view object) and player_state0+0x140 around each stage so the stage that
 * binds slot+0x240 in single-player is identified from the log. */
enum { SLOT_PROBE_COUNT = 5 };
static const uint32_t slot_probe_offsets[SLOT_PROBE_COUNT] = {
    0x00107BF2u, 0x00108644u, 0x000D924Eu, 0x000E337Cu, 0x000D8DAAu
};
static const char *const slot_probe_names[SLOT_PROBE_COUNT] = {
    "sp-hero-spawn 107BF2", "sp-view-setup 108644", "slot-init D924E",
    "pre-slot reset E337C", "post-slot D8DAA"
};
static tai_hook_ref_t slot_probe_refs[SLOT_PROBE_COUNT];
static SceUID slot_probe_uids[SLOT_PROBE_COUNT] = { -1, -1, -1, -1, -1 };
static int slot_probe_calls[SLOT_PROBE_COUNT];
static int slot_probe_dumped;

static void slot_probe_log(int index, const char *phase) {
    uintptr_t slot0 = rc3_base + 0x009D4B80u;
    uintptr_t state0 = rc3_base + 0x009DE900u;
    uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                MAIN_CONTEXT_PTR_DATA_OFFSET);
    uintptr_t moby = *(volatile uintptr_t *)(slot0 + 0x50u);
    uintptr_t view = *(volatile uintptr_t *)(slot0 + 0x240u);
    uintptr_t target = *(volatile uintptr_t *)(state0 + 0x140u);
    int level = *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET);
    int mp = context ? *(volatile uint8_t *)(context + 0xC8u) : -1;
    char line[224];
    snprintf(line, sizeof(line),
             "Slot probe: %s %s call=%d level=%d mp=%d slot50=%08X "
             "slot240=%08X state140=%08X.\n",
             slot_probe_names[index], phase, slot_probe_calls[index],
             level, mp, (unsigned int)moby, (unsigned int)view,
             (unsigned int)target);
    append_log(line);
    if (view && !slot_probe_dumped) {
        const volatile uint32_t *w = (const volatile uint32_t *)view;
        slot_probe_dumped = 1;
        snprintf(line, sizeof(line),
                 "Slot probe: view=%08X text_off=%08X data_off=%08X "
                 "w0=%08X w1=%08X w2=%08X w3=%08X w4=%08X w5=%08X.\n",
                 (unsigned int)view, (unsigned int)(view - rc3_base),
                 (unsigned int)(view - rc3_data_base),
                 (unsigned int)w[0], (unsigned int)w[1], (unsigned int)w[2],
                 (unsigned int)w[3], (unsigned int)w[4], (unsigned int)w[5]);
        append_log(line);
        snprintf(line, sizeof(line),
                 "Slot probe: view+10=%08X %08X %08X %08X +C0=%08X %08X "
                 "%08X +E0=%08X %08X %08X +1C=%08X +70=%08X.\n",
                 (unsigned int)w[4], (unsigned int)w[5], (unsigned int)w[6],
                 (unsigned int)w[7], (unsigned int)w[0x30],
                 (unsigned int)w[0x31], (unsigned int)w[0x32],
                 (unsigned int)w[0x38], (unsigned int)w[0x39],
                 (unsigned int)w[0x3A], (unsigned int)w[7],
                 (unsigned int)w[0x1C]);
        append_log(line);
    }
}

static uintptr_t continue_slot_probe(int index, uintptr_t a0, uintptr_t a1,
                                     uintptr_t a2, uintptr_t a3) {
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)slot_probe_refs[index];
    struct _tai_hook_user *next = (struct _tai_hook_user *)current->next;
    ArenaStageFn function = (ArenaStageFn)(next ? next->func : current->old);
    return function(a0, a1, a2, a3);
}

#define DEFINE_SLOT_PROBE(index)                                            \
    static uintptr_t slot_probe_hook_##index(uintptr_t a0, uintptr_t a1,    \
                                             uintptr_t a2, uintptr_t a3) {  \
        uintptr_t result;                                                   \
        int verbose = slot_probe_calls[index] < 4;                          \
        if (verbose)                                                        \
            slot_probe_log(index, "enter");                                 \
        result = continue_slot_probe(index, a0, a1, a2, a3);                \
        if (verbose)                                                        \
            slot_probe_log(index, "exit");                                  \
        ++slot_probe_calls[index];                                          \
        return result;                                                      \
    }

DEFINE_SLOT_PROBE(0)
DEFINE_SLOT_PROBE(1)

/* v24: single-player binds slot+0x240 (the hero moby, also the global at
 * 0x81A0F470) in 0x81107BF2, which scans the level moby table for the first
 * oClass-0 instance.  Multiplayer skips that stage and 0x810D924E then runs
 * with no hero.  Before the arena's slot initializer, locate a class-0 moby
 * and, only if one exists, run the retained hero stage and view stage in the
 * same order single-player uses. */
typedef void (*VoidFn)(void);
static int arena_hero_bootstrap_done;
static void start_arena_sampler(uintptr_t hero);

static void arena_hero_bootstrap(void) {
    uintptr_t first = *(volatile uintptr_t *)(rc3_base + 0x00B8F2A0u);
    uintptr_t last = *(volatile uintptr_t *)(rc3_base + 0x00B8F2A4u);
    uintptr_t hero_global = *(volatile uintptr_t *)(rc3_base + 0x00A0F470u);
    uintptr_t marker = 0x87AA6E40u;
    uintptr_t cursor;
    uintptr_t hero = 0;
    unsigned int count = 0;
    int marker_class = -1;
    char line[224];

    if (arena_hero_bootstrap_done)
        return;
    arena_hero_bootstrap_done = 1;
    if (first && last > first && last - first < 0x01000000u) {
        count = (unsigned int)((last - first) / 0x100u);
        for (cursor = first; cursor < last; cursor += 0x100u) {
            int oclass = *(volatile int16_t *)(cursor + 0xAAu);
            if (cursor == marker)
                marker_class = oclass;
            if (!hero && oclass == 0)
                hero = cursor;
        }
    }
    snprintf(line, sizeof(line),
             "Arena hero: moby table %08X..%08X count=%u class0=%08X "
             "hero_global=%08X marker_class=%d.\n",
             (unsigned int)first, (unsigned int)last, count,
             (unsigned int)hero, (unsigned int)hero_global, marker_class);
    append_log(line);
    snprintf(line, sizeof(line),
             "Arena hero: marker %08X class=%d; class0 map byte=%02X.\n",
             (unsigned int)marker, (int)*(volatile int16_t *)(marker + 0xAAu),
             (unsigned int)*(volatile uint8_t *)(rc3_base + 0x00B8AF90u));
    append_log(line);
    if (!hero) {
        /* PS3 MP heroes are not class 0: 0x003786D0 spawns
         * skinClass[profile skin] from the table at PS3 0x00AFA004. */
        static const uint16_t mp_skin_classes[22] = {
            0x171C, 0x18CB, 0x18CC, 0x18CF, 0x18CD, 0x1ADC, 0x16DB, 0x1CFC,
            0x1E68, 0x1E07, 0x1CFB, 0x1E77, 0x1E78, 0x1CFD, 0x1CFE, 0x1E4C,
            0x1E65, 0x1E4D, 0x1E69, 0x1E59, 0x1E7A, 0x1E83
        };
        typedef uintptr_t (*SpawnMobyFn)(int oclass, int pvar_size);
        SpawnMobyFn spawn_moby =
            (SpawnMobyFn)((rc3_base + 0x001789C4u) | 1u);
        volatile uintptr_t *scan_end =
            (volatile uintptr_t *)(rc3_base + 0x00B8F2A4u);
        volatile uint8_t *class_map =
            (volatile uint8_t *)(rc3_base + 0x00B8AF90u);
        uintptr_t saved_end = *scan_end;
        int skin = -1;
        int k;
        int16_t real_class;
        char map_line[256];
        int pos = snprintf(map_line, sizeof(map_line), "Arena hero: MP skin class map:");
        for (k = 0; k < 22; ++k) {
            uint8_t v = class_map[mp_skin_classes[k]];
            if (pos < (int)sizeof(map_line) - 12)
                pos += snprintf(map_line + pos, sizeof(map_line) - pos,
                                " %X=%02X", mp_skin_classes[k], v);
            if (skin < 0 && v != 0xFFu)
                skin = k;
        }
        append_log(map_line);
        append_log(".\n");
        if (skin < 0) {
            append_log("Arena hero: no MP skin class loaded in arena data; spawn skipped.\n");
            return;
        }
        hero = spawn_moby(mp_skin_classes[skin], 0x80);
        snprintf(line, sizeof(line),
                 "Arena hero: spawned MP skin %d class=%X moby=%08X (dynamic start=%08X).\n",
                 skin, mp_skin_classes[skin], (unsigned int)hero,
                 (unsigned int)saved_end);
        append_log(line);
        if (!hero || hero < saved_end) {
            append_log("Arena hero: spawn failed; retained hero stage skipped.\n");
            return;
        }
        /* 0x81107BF2 binds the first class-0 moby in [F2A0, F2A4).  Present
         * the spawned skin hero as class 0 and in range only for that call,
         * then restore its real class and the dynamic-region start. */
        real_class = *(volatile int16_t *)(hero + 0xAAu);
        *(volatile int16_t *)(hero + 0xAAu) = 0;
        *scan_end = hero + 0x100u;
        append_log("Arena hero: running retained hero stage 107BF2 on MP skin hero.\n");
        ((VoidFn)((rc3_base + 0x00107BF2u) | 1u))();
        *scan_end = saved_end;
        *(volatile int16_t *)(hero + 0xAAu) = real_class;
        slot_probe_log(0, "arena-forced");
        start_arena_sampler(hero);
        return;
    }
    append_log("Arena hero: running retained hero stage 107BF2.\n");
    ((VoidFn)((rc3_base + 0x00107BF2u) | 1u))();
    slot_probe_log(0, "arena-forced");
    append_log("Arena hero: running retained view stage 108644.\n");
    ((VoidFn)((rc3_base + 0x00108644u) | 1u))();
    slot_probe_log(1, "arena-forced");
}

/* v27: once the MP skin hero is bound, sample the live hero and slot-0 view
 * fields once per second so the log shows whether the world is simulating
 * behind the black screen.  Floats are logged as raw IEEE words. */
static volatile uintptr_t arena_sample_hero;

enum { ARENA_DRAW_TRACE_CAPACITY = 64 };

typedef struct ArenaDrawTrace {
    volatile unsigned int complete;
    unsigned int call;
    uintptr_t start;
    uintptr_t output;
    unsigned int count;
    int result;
    uint32_t planes[5];
    uint32_t distance_squared;
    uint32_t limit_squared;
    uint32_t camera[3];
    uint16_t flags;
    uint16_t mask;
    int8_t state;
    uint8_t pre_accepted;
    uint8_t post_accepted;
    uint8_t pvs_enabled;
    uint8_t pvs_inverted;
    uint8_t pvs_active;
    uint8_t pvs_rejected;
    int8_t failed_plane;
    uint8_t distance_passed;
    uint8_t emitted;
} ArenaDrawTrace;

static ArenaDrawTrace arena_draw_traces[ARENA_DRAW_TRACE_CAPACITY];
static volatile unsigned int arena_draw_trace_calls;
static volatile unsigned int arena_draw_trace_count;
static unsigned int arena_draw_trace_logged;

static uint32_t float_word(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static int moby_draw_builder_hook(uintptr_t start, uintptr_t output,
                                  unsigned int count) {
    typedef int (*MobyDrawBuilderFn)(uintptr_t start, uintptr_t output,
                                     unsigned int count);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)moby_draw_builder_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    MobyDrawBuilderFn function =
        (MobyDrawBuilderFn)(next ? next->func : current->old);
    uintptr_t hero = arena_sample_hero;
    unsigned int call = __sync_fetch_and_add(&arena_draw_trace_calls, 1u);
    int capture =
        hero && start <= hero && (hero - start) / 0x100u < count &&
        (hero - start) % 0x100u == 0 &&
        (call < 16u || (call % 30u) == 0);
    ArenaDrawTrace trace;
    int result;

    memset(&trace, 0, sizeof(trace));
    trace.failed_plane = -1;
    if (capture) {
        uintptr_t renderer =
            *(volatile uintptr_t *)(rc3_base + 0x007FBFF4u);
        uintptr_t visibility =
            renderer ? *(volatile uintptr_t *)(renderer + 0x29Cu) : 0;
        volatile float *planes =
            (volatile float *)(rc3_base + 0x0113285Cu);
        volatile float *camera =
            (volatile float *)(rc3_base + 0x011328BCu);
        float scale = 1.0f / 1024.0f;
        float x = *(volatile float *)(hero + 0x00u) * scale;
        float y = *(volatile float *)(hero + 0x04u) * scale;
        float z = *(volatile float *)(hero + 0x08u) * scale;
        float radius = *(volatile float *)(hero + 0x0Cu);
        float extent =
            (float)*(volatile int16_t *)(hero + 0x32u) + radius * scale;
        float dx;
        float dy;
        float dz;
        float distance_squared;
        float limit_squared;
        unsigned int plane;
        unsigned int index;
        unsigned int bit;

        trace.call = call;
        trace.start = start;
        trace.output = output;
        trace.count = count;
        trace.state = *(volatile int8_t *)(hero + 0x20u);
        trace.pre_accepted = *(volatile uint8_t *)(hero + 0x31u);
        trace.flags = *(volatile uint16_t *)(hero + 0x34u);
        trace.mask = *(volatile uint16_t *)(hero + 0x36u);
        trace.pvs_enabled =
            *(volatile uint8_t *)(rc3_base + 0x007FBC70u);
        trace.pvs_inverted =
            *(volatile uint8_t *)(rc3_base + 0x0095C42Cu);
        index = trace.mask >> 8;
        bit = trace.mask & 0xFFu;
        trace.pvs_active =
            visibility &&
            (*(volatile uint8_t *)(visibility + index) & bit) != 0;
        trace.pvs_rejected =
            trace.pvs_enabled &&
            (trace.pvs_inverted ? trace.pvs_active : !trace.pvs_active);
        for (plane = 0; plane < 5u; ++plane) {
            float value =
                planes[plane * 4u + 3u] +
                planes[plane * 4u + 0u] * x +
                planes[plane * 4u + 2u] * z +
                planes[plane * 4u + 1u] * y +
                radius * scale;
            trace.planes[plane] = float_word(value);
            if (value < 0.0f && trace.failed_plane < 0)
                trace.failed_plane = (int8_t)plane;
        }
        trace.camera[0] = float_word(camera[0]);
        trace.camera[1] = float_word(camera[1]);
        trace.camera[2] = float_word(camera[2]);
        dx = x - camera[0];
        dy = y - camera[1];
        dz = z - camera[2];
        distance_squared = dx * dx + dy * dy + dz * dz;
        limit_squared = extent * extent;
        trace.distance_squared = float_word(distance_squared);
        trace.limit_squared = float_word(limit_squared);
        trace.distance_passed = distance_squared <= limit_squared;
    }

    result = function(start, output, count);
    if (capture) {
        unsigned int index;
        unsigned int slot;

        trace.result = result;
        trace.post_accepted = *(volatile uint8_t *)(hero + 0x31u);
        if (output && result > 0 && result < 2048) {
            for (index = 0; index < (unsigned int)result; ++index) {
                if (*(volatile uintptr_t *)
                        (output + (uintptr_t)index * 0x2Cu + 0x28u) == hero) {
                    trace.emitted = 1;
                    break;
                }
            }
        }
        slot = __sync_fetch_and_add(&arena_draw_trace_count, 1u);
        if (slot < ARENA_DRAW_TRACE_CAPACITY) {
            arena_draw_traces[slot] = trace;
            __sync_synchronize();
            arena_draw_traces[slot].complete = 1u;
        }
    }
    return result;
}

static void log_arena_draw_traces(void) {
    unsigned int available = arena_draw_trace_count;
    char line[320];

    if (available > ARENA_DRAW_TRACE_CAPACITY)
        available = ARENA_DRAW_TRACE_CAPACITY;
    while (arena_draw_trace_logged < available) {
        ArenaDrawTrace *trace = &arena_draw_traces[arena_draw_trace_logged];
        if (!trace->complete)
            break;
        snprintf(line, sizeof(line),
                 "Arena draw trace %u call=%u range=%08X+%u out=%08X "
                 "result=%d accepted=%u->%u emitted=%u state=%d "
                 "flags=%04X mask=%04X.\n",
                 arena_draw_trace_logged, trace->call,
                 (unsigned int)trace->start, trace->count,
                 (unsigned int)trace->output, trace->result,
                 (unsigned int)trace->pre_accepted,
                 (unsigned int)trace->post_accepted,
                 (unsigned int)trace->emitted, (int)trace->state,
                 (unsigned int)trace->flags, (unsigned int)trace->mask);
        append_log(line);
        snprintf(line, sizeof(line),
                 "Arena draw trace %u pvs: enabled=%u inverted=%u active=%u "
                 "rejected=%u; planes=%08X,%08X,%08X,%08X,%08X fail=%d.\n",
                 arena_draw_trace_logged,
                 (unsigned int)trace->pvs_enabled,
                 (unsigned int)trace->pvs_inverted,
                 (unsigned int)trace->pvs_active,
                 (unsigned int)trace->pvs_rejected,
                 (unsigned int)trace->planes[0],
                 (unsigned int)trace->planes[1],
                 (unsigned int)trace->planes[2],
                 (unsigned int)trace->planes[3],
                 (unsigned int)trace->planes[4],
                 (int)trace->failed_plane);
        append_log(line);
        snprintf(line, sizeof(line),
                 "Arena draw trace %u distance=%08X limit=%08X pass=%u "
                 "camera=%08X,%08X,%08X.\n",
                 arena_draw_trace_logged,
                 (unsigned int)trace->distance_squared,
                 (unsigned int)trace->limit_squared,
                 (unsigned int)trace->distance_passed,
                 (unsigned int)trace->camera[0],
                 (unsigned int)trace->camera[1],
                 (unsigned int)trace->camera[2]);
        append_log(line);
        ++arena_draw_trace_logged;
    }
}

static int arena_sampler_main(SceSize argc, void *args) {
    int n;
    (void)argc;
    (void)args;
    for (n = 0; n < 12; ++n) {
        uintptr_t hero = arena_sample_hero;
        uintptr_t slot0 = rc3_base + 0x009D4B80u;
        uintptr_t record = *(volatile uintptr_t *)(slot0 + 0x50u);
        volatile uintptr_t *special =
            (volatile uintptr_t *)(rc3_base + 0x00BABEACu);
        uintptr_t draw_base =
            *(volatile uintptr_t *)(rc3_base + 0x00B8F29Cu);
        uintptr_t draw_end =
            *(volatile uintptr_t *)(rc3_base + 0x00B8F298u);
        uintptr_t class_data = *(volatile uintptr_t *)(hero + 0x24u);
        int draw_count = 0;
        int draw_hero_index = -1;
        int special_count = 0;
        int special_hero_index = -1;
        char line[320];
        sceKernelDelayThread(1000000);
        if (!hero)
            break;
        while (special_count < 64 && special[special_count]) {
            if (special[special_count] == hero)
                special_hero_index = special_count;
            ++special_count;
        }
        if (draw_base && draw_end >= draw_base &&
            (draw_end - draw_base) / 0x2Cu < 2048u) {
            draw_count = (int)((draw_end - draw_base) / 0x2Cu);
            while (draw_hero_index < 0 && draw_count > 0) {
                int index;
                for (index = 0; index < draw_count; ++index) {
                    if (*(volatile uintptr_t *)
                            (draw_base + (uintptr_t)index * 0x2Cu + 0x28u) ==
                        hero) {
                        draw_hero_index = index;
                        break;
                    }
                }
                break;
            }
        }
        snprintf(line, sizeof(line),
                 "Arena sample %d: level=%d hero=%08X pos=%08X,%08X,%08X "
                 "flags34=%04X b20=%02X upd=%08X slot190=%08X,%08X,%08X "
                 "slot0=%08X,%08X,%08X rec=%08X rec30=%08X,%08X,%08X "
                 "heroglobal=%08X dispatch=%d presentation=%d views=%d.\n",
                 n, *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET),
                 (unsigned int)hero,
                 (unsigned int)*(volatile uint32_t *)(hero + 0x10u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x14u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x18u),
                 (unsigned int)*(volatile uint16_t *)(hero + 0x34u),
                 (unsigned int)*(volatile uint8_t *)(hero + 0x20u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x3Cu),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x190u),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x194u),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x198u),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x0u),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x4u),
                 (unsigned int)*(volatile uint32_t *)(slot0 + 0x8u),
                 (unsigned int)record,
                 record ? (unsigned int)*(volatile uint32_t *)(record + 0x30u) : 0u,
                 record ? (unsigned int)*(volatile uint32_t *)(record + 0x34u) : 0u,
                 record ? (unsigned int)*(volatile uint32_t *)(record + 0x38u) : 0u,
                 (unsigned int)*(volatile uintptr_t *)(rc3_base + 0x00A0F470u),
                 *(volatile int *)(rc3_base + 0x00B539FCu),
                 *(volatile int *)(rc3_base + 0x00B539F4u),
                 *(volatile int *)(rc3_base + 0x0081558Cu));
        append_log(line);
        snprintf(line, sizeof(line),
                 "Arena sample %d lists: special_count=%d hero_index=%d "
                 "active=%08X draw=%08X-%08X count=%d hero_index=%d.\n",
                 n, special_count, special_hero_index,
                 (unsigned int)*(volatile uintptr_t *)
                     (rc3_base + 0x00B8F2ACu),
                 (unsigned int)draw_base, (unsigned int)draw_end,
                 draw_count, draw_hero_index);
        append_log(line);
        snprintf(line, sizeof(line),
                 "Arena sample %d render gate: b20=%02X b31=%02X "
                 "bounds=%08X,%08X,%08X radius=%08X cull32=%04X mask=%04X "
                 "class=%08X class8=%02X "
                 "model10=%08X draw14=%08X draw18=%08X.\n",
                 n,
                 (unsigned int)*(volatile uint8_t *)(hero + 0x20u),
                 (unsigned int)*(volatile uint8_t *)(hero + 0x31u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x00u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x04u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x08u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x0Cu),
                 (unsigned int)*(volatile uint16_t *)(hero + 0x32u),
                 (unsigned int)*(volatile uint16_t *)(hero + 0x36u),
                 (unsigned int)class_data,
                 class_data
                     ? (unsigned int)*(volatile uint8_t *)(class_data + 0x08u)
                     : 0u,
                 class_data
                     ? (unsigned int)*(volatile uintptr_t *)(class_data + 0x10u)
                     : 0u,
                 class_data
                     ? (unsigned int)*(volatile uintptr_t *)(class_data + 0x14u)
                     : 0u,
                 class_data
                     ? (unsigned int)*(volatile uintptr_t *)(class_data + 0x18u)
                     : 0u);
        append_log(line);
        log_arena_draw_traces();
    }
    return sceKernelExitDeleteThread(0);
}

static void start_arena_sampler(uintptr_t hero) {
    SceUID thread;
    arena_sample_hero = hero;
    thread = sceKernelCreateThread("rc3_arena_sample", arena_sampler_main,
                                   0x10000100, 0x4000, 0, 0, 0);
    if (thread < 0 || sceKernelStartThread(thread, 0, 0) < 0)
        append_log("Arena sample: sampler thread could not start.\n");
}

static void sync_arena_player_to_hero(void) {
    static const uint32_t metropolis_spawn_position[2][3] = {
        {0x4441DC38u, 0x43DE39F4u, 0x43A7A664u},
        {0x4437D489u, 0x4369435Eu, 0x43A7A664u}
    };
    uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                MAIN_CONTEXT_PTR_DATA_OFFSET);
    uintptr_t slot = rc3_base + 0x009D4B80u;
    uintptr_t record = *(volatile uintptr_t *)(slot + 0x50u);
    uintptr_t hero = *(volatile uintptr_t *)(slot + 0x240u);
    uintptr_t player_state =
        record ? *(volatile uintptr_t *)(record + 0x70u) : 0;
    uintptr_t global_camera = rc3_base + 0x00A0CEB0u;
    float x;
    float y;
    float z;
    float w;
    float forward_x;
    float forward_y;
    float forward_z;
    float up_x;
    float up_y;
    float up_z;
    float camera_x;
    float camera_y;
    float camera_z;

    if (*(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET) !=
            arena_target_level ||
        !context || !*(volatile uint8_t *)(context + 0xC8u) ||
        *(volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET) != 1 ||
        !record || !player_state || !hero ||
        *(volatile uintptr_t *)(player_state + 0x140u) != hero)
        return;

    if (!arena_final_record_seeded &&
        *(volatile int *)(rc3_base + PRESENTATION_CURRENT_OFFSET) == 0) {
        unsigned int team = local_team_index & 1u;
        uint32_t spawn_x = metropolis_spawn_position[team][0];
        uint32_t spawn_y = metropolis_spawn_position[team][1];
        uint32_t spawn_z = metropolis_spawn_position[team][2];
        *(volatile uint32_t *)(slot + 0x00u) = spawn_x;
        *(volatile uint32_t *)(slot + 0x04u) = spawn_y;
        *(volatile uint32_t *)(slot + 0x08u) = spawn_z;
        *(volatile uint32_t *)(record + 0x30u) = spawn_x;
        *(volatile uint32_t *)(record + 0x34u) = spawn_y;
        *(volatile uint32_t *)(record + 0x38u) = spawn_z;
        *(volatile uint32_t *)(record + 0x64u) = spawn_x;
        *(volatile uint32_t *)(record + 0x68u) = spawn_y;
        *(volatile uint32_t *)(record + 0x6Cu) = spawn_z;
        *(volatile uint32_t *)(player_state + 0x60u) = spawn_x;
        *(volatile uint32_t *)(player_state + 0x64u) = spawn_y;
        *(volatile uint32_t *)(player_state + 0x68u) = spawn_z;
        *(volatile uint32_t *)(player_state + 0x6Cu) =
            *(volatile uint32_t *)(record + 0x3Cu);
        *(volatile uint32_t *)(player_state + 0x70u) = spawn_x;
        *(volatile uint32_t *)(player_state + 0x74u) = spawn_y;
        *(volatile uint32_t *)(player_state + 0x78u) = spawn_z;
        *(volatile uint32_t *)(player_state + 0x7Cu) =
            *(volatile uint32_t *)(record + 0x3Cu);
        *(volatile uint32_t *)(player_state + 0x80u) = spawn_x;
        *(volatile uint32_t *)(player_state + 0x84u) = spawn_y;
        *(volatile uint32_t *)(player_state + 0x88u) = spawn_z;
        *(volatile uint32_t *)(player_state + 0x8Cu) =
            *(volatile uint32_t *)(record + 0x3Cu);
        *(volatile uint32_t *)(player_state + 0x90u) = spawn_x;
        *(volatile uint32_t *)(player_state + 0x94u) = spawn_y;
        *(volatile uint32_t *)(player_state + 0x98u) = spawn_z;
        *(volatile uint32_t *)(player_state + 0x9Cu) =
            *(volatile uint32_t *)(record + 0x3Cu);
        *(volatile uint32_t *)(player_state + 0xB0u) = spawn_x;
        *(volatile uint32_t *)(player_state + 0xB4u) = spawn_y;
        *(volatile uint32_t *)(player_state + 0xB8u) = spawn_z;
        *(volatile uint32_t *)(player_state + 0xBCu) =
            *(volatile uint32_t *)(record + 0x3Cu);
        arena_final_record_seeded = 1;
        append_log("Arena player sync: seeded authoritative player-state transform.\n");
    }

    if (arena_controller_position_valid) {
        x = arena_controller_position[0];
        y = arena_controller_position[1];
        z = arena_controller_position[2];
        w = arena_controller_position[3];
    } else {
        x = *(volatile float *)(player_state + 0x60u);
        y = *(volatile float *)(player_state + 0x64u);
        z = *(volatile float *)(player_state + 0x68u);
        w = *(volatile float *)(player_state + 0x6Cu);
    }
    *(volatile float *)(player_state + 0x60u) = x;
    *(volatile float *)(player_state + 0x64u) = y;
    *(volatile float *)(player_state + 0x68u) = z;
    *(volatile float *)(player_state + 0x6Cu) = w;
    *(volatile float *)(slot + 0x00u) = x;
    *(volatile float *)(slot + 0x04u) = y;
    *(volatile float *)(slot + 0x08u) = z;
    *(volatile float *)(slot + 0x0Cu) = w;
    *(volatile float *)(record + 0x30u) = x;
    *(volatile float *)(record + 0x34u) = y;
    *(volatile float *)(record + 0x38u) = z;
    *(volatile float *)(record + 0x3Cu) = w;
    *(volatile float *)(record + 0x64u) = x;
    *(volatile float *)(record + 0x68u) = y;
    *(volatile float *)(record + 0x6Cu) = z;
    *(volatile float *)(hero + 0x10u) = x;
    *(volatile float *)(hero + 0x14u) = y;
    *(volatile float *)(hero + 0x18u) = z;
    *(volatile float *)(hero + 0x1Cu) = w;
    ((void (*)(uintptr_t))((rc3_base + 0x000B4C28u) | 1u))(hero);
    *(volatile float *)(hero + 0x00u) = x;
    *(volatile float *)(hero + 0x04u) = y;
    *(volatile float *)(hero + 0x08u) = z;
    if (*(volatile uint8_t *)(rc3_base + 0x007FBC70u)) {
        uintptr_t renderer =
            *(volatile uintptr_t *)(rc3_base + 0x007FBFF4u);
        uintptr_t visibility =
            renderer ? *(volatile uintptr_t *)(renderer + 0x29Cu) : 0;
        uint16_t mask = *(volatile uint16_t *)(hero + 0x36u);
        unsigned int index = mask >> 8;
        unsigned int bit = mask & 0xFFu;
        if (visibility &&
            !(*(volatile uint8_t *)(visibility + index) & bit)) {
            unsigned int active_index;
            for (active_index = 0; active_index < 256u; ++active_index) {
                unsigned int active =
                    *(volatile uint8_t *)(visibility + active_index);
                if (active) {
                    unsigned int active_bit = active & (~active + 1u);
                    *(volatile uint16_t *)(hero + 0x36u) =
                        (uint16_t)((active_index << 8) | active_bit);
                    if (!arena_visibility_repair_logged) {
                        char line[144];
                        snprintf(line, sizeof(line),
                                 "Arena hero render: visibility mask %04X "
                                 "repaired to %04X.\n",
                                 mask,
                                 (unsigned int)*(volatile uint16_t *)
                                     (hero + 0x36u));
                        append_log(line);
                        arena_visibility_repair_logged = 1;
                    }
                    break;
                }
            }
        }
    }

    forward_x = *(volatile float *)(hero + 0xC0u);
    forward_y = *(volatile float *)(hero + 0xC4u);
    forward_z = *(volatile float *)(hero + 0xC8u);
    up_x = *(volatile float *)(hero + 0xE0u);
    up_y = *(volatile float *)(hero + 0xE4u);
    up_z = *(volatile float *)(hero + 0xE8u);
    if (forward_x == 0.0f && forward_y == 0.0f && forward_z == 0.0f)
        forward_y = 1.0f;
    if (up_x == 0.0f && up_y == 0.0f && up_z == 0.0f)
        up_z = 1.0f;
    camera_x = x - forward_x * 6.0f + up_x * 2.5f;
    camera_y = y - forward_y * 6.0f + up_y * 2.5f;
    camera_z = z - forward_z * 6.0f + up_z * 2.5f;
    *(volatile float *)(slot + 0x190u) = x;
    *(volatile float *)(slot + 0x194u) = y;
    *(volatile float *)(slot + 0x198u) = z;
    *(volatile float *)(slot + 0x19Cu) = w;
    *(volatile float *)(global_camera + 0x80u) = camera_x;
    *(volatile float *)(global_camera + 0x84u) = camera_y;
    *(volatile float *)(global_camera + 0x88u) = camera_z;
    *(volatile float *)(global_camera + 0x8Cu) = w;
    __sync_synchronize();

    if (arena_player_sync_calls < 6) {
        char line[224];
        snprintf(line, sizeof(line),
                 "Arena player sync: call=%d record=%08X hero=%08X "
                 "position=%08X,%08X,%08X controller=%08X,%08X,%08X "
                 "camera=%08X,%08X,%08X.\n",
                 arena_player_sync_calls, (unsigned int)record,
                 (unsigned int)hero,
                 (unsigned int)*(volatile uint32_t *)(hero + 0x10u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x14u),
                 (unsigned int)*(volatile uint32_t *)(hero + 0x18u),
                 (unsigned int)*(volatile uint32_t *)(slot + 0x190u),
                 (unsigned int)*(volatile uint32_t *)(slot + 0x194u),
                 (unsigned int)*(volatile uint32_t *)(slot + 0x198u),
                 (unsigned int)*(volatile uint32_t *)(global_camera + 0x80u),
                 (unsigned int)*(volatile uint32_t *)(global_camera + 0x84u),
                 (unsigned int)*(volatile uint32_t *)(global_camera + 0x88u));
        append_log(line);
    }
    ++arena_player_sync_calls;
}

static uintptr_t slot_probe_hook_2(uintptr_t a0, uintptr_t a1,
                                   uintptr_t a2, uintptr_t a3) {
    uintptr_t result;
    int verbose = slot_probe_calls[2] < 4;
    uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                MAIN_CONTEXT_PTR_DATA_OFFSET);
    if (verbose)
        slot_probe_log(2, "enter");
    if (arena_roster_repair_active && context &&
        *(volatile uint8_t *)(context + 0xC8u) &&
        !*(volatile uintptr_t *)(rc3_base + 0x009D4B80u + 0x240u))
        arena_hero_bootstrap();
    result = continue_slot_probe(2, a0, a1, a2, a3);
    if (!arena_post_slot_hero_positioned && context &&
        *(volatile uint8_t *)(context + 0xC8u)) {
        uintptr_t slot = rc3_base + 0x009D4B80u;
        uintptr_t record = *(volatile uintptr_t *)(slot + 0x50u);
        uintptr_t player_state =
            record ? *(volatile uintptr_t *)(record + 0x70u) : 0;
        uintptr_t hero = *(volatile uintptr_t *)(slot + 0x240u);
        uintptr_t state_hero =
            player_state ? *(volatile uintptr_t *)(player_state + 0x140u) : 0;

        if (record && player_state && hero && state_hero == hero) {
            static const uint32_t metropolis_spawn_position[2][3] = {
                {0x4441DC38u, 0x43DE39F4u, 0x43A7A664u},
                {0x4437D489u, 0x4369435Eu, 0x43A7A664u}
            };
            static const uint32_t metropolis_spawn_basis[2][12] = {
                {
                    0xBBC32CF8u, 0xBF7FFED6u, 0x00000000u, 0x00000000u,
                    0x3F7FFED6u, 0xBBC32CF8u, 0x00000000u, 0x00000000u,
                    0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u
                },
                {
                    0xB90759AAu, 0x3F800000u, 0x00000000u, 0x00000000u,
                    0xBF800000u, 0xB90759AAu, 0x00000000u, 0x00000000u,
                    0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u
                }
            };
            static const uint32_t metropolis_spawn_rotation_z[2] = {
                0xBFC9D308u, 0x3FC91415u
            };
            unsigned int team = local_team_index & 1u;
            uint32_t before_x = *(volatile uint32_t *)(hero + 0x10u);
            uint32_t before_y = *(volatile uint32_t *)(hero + 0x14u);
            uint32_t before_z = *(volatile uint32_t *)(hero + 0x18u);
            uint32_t x;
            uint32_t y;
            uint32_t z;
            uint32_t w;
            uintptr_t global_camera = rc3_base + 0x00A0CEB0u;
            uint32_t camera_before_x =
                *(volatile uint32_t *)(slot + 0x190u);
            uint32_t camera_before_y =
                *(volatile uint32_t *)(slot + 0x194u);
            uint32_t camera_before_z =
                *(volatile uint32_t *)(slot + 0x198u);
            unsigned int matrix_word;
            char line[256];

            x = metropolis_spawn_position[team][0];
            y = metropolis_spawn_position[team][1];
            z = metropolis_spawn_position[team][2];
            w = *(volatile uint32_t *)(record + 0x3Cu);

            *(volatile uint32_t *)(slot + 0x00u) = x;
            *(volatile uint32_t *)(slot + 0x04u) = y;
            *(volatile uint32_t *)(slot + 0x08u) = z;
            *(volatile uint32_t *)(slot + 0x0Cu) = w;
            *(volatile uint32_t *)(record + 0x30u) = x;
            *(volatile uint32_t *)(record + 0x34u) = y;
            *(volatile uint32_t *)(record + 0x38u) = z;
            *(volatile uint32_t *)(record + 0x3Cu) = w;
            *(volatile uint32_t *)(record + 0x64u) = x;
            *(volatile uint32_t *)(record + 0x68u) = y;
            *(volatile uint32_t *)(record + 0x6Cu) = z;
            *(volatile uint32_t *)(player_state + 0x60u) = x;
            *(volatile uint32_t *)(player_state + 0x64u) = y;
            *(volatile uint32_t *)(player_state + 0x68u) = z;
            *(volatile uint32_t *)(player_state + 0x6Cu) = w;
            *(volatile uint32_t *)(player_state + 0x70u) = x;
            *(volatile uint32_t *)(player_state + 0x74u) = y;
            *(volatile uint32_t *)(player_state + 0x78u) = z;
            *(volatile uint32_t *)(player_state + 0x7Cu) = w;
            *(volatile uint32_t *)(player_state + 0x80u) = x;
            *(volatile uint32_t *)(player_state + 0x84u) = y;
            *(volatile uint32_t *)(player_state + 0x88u) = z;
            *(volatile uint32_t *)(player_state + 0x8Cu) = w;
            *(volatile uint32_t *)(player_state + 0x90u) = x;
            *(volatile uint32_t *)(player_state + 0x94u) = y;
            *(volatile uint32_t *)(player_state + 0x98u) = z;
            *(volatile uint32_t *)(player_state + 0x9Cu) = w;
            *(volatile uint32_t *)(player_state + 0xB0u) = x;
            *(volatile uint32_t *)(player_state + 0xB4u) = y;
            *(volatile uint32_t *)(player_state + 0xB8u) = z;
            *(volatile uint32_t *)(player_state + 0xBCu) = w;
            arena_controller_position[0] =
                *(volatile float *)(player_state + 0x60u);
            arena_controller_position[1] =
                *(volatile float *)(player_state + 0x64u);
            arena_controller_position[2] =
                *(volatile float *)(player_state + 0x68u);
            arena_controller_position[3] =
                *(volatile float *)(player_state + 0x6Cu);
            arena_controller_position_valid = 1;
            *(volatile uint32_t *)(hero + 0x10u) = x;
            *(volatile uint32_t *)(hero + 0x14u) = y;
            *(volatile uint32_t *)(hero + 0x18u) = z;
            *(volatile uint32_t *)(hero + 0x1Cu) = w;
            for (matrix_word = 0; matrix_word < 12u; ++matrix_word) {
                *(volatile uint32_t *)(hero + 0xC0u + matrix_word * 4u) =
                    metropolis_spawn_basis[team][matrix_word];
            }
            *(volatile uint32_t *)(hero + 0xF0u) = 0;
            *(volatile uint32_t *)(hero + 0xF4u) = 0;
            *(volatile uint32_t *)(hero + 0xF8u) =
                metropolis_spawn_rotation_z[team];
            *(volatile uint32_t *)(hero + 0xFCu) = 0;
            *(volatile uint8_t *)(hero + 0x30u) = 0xFFu;
            *(volatile uint8_t *)(hero + 0x31u) = 1u;
            *(volatile uint16_t *)(hero + 0x32u) = 0;
            *(volatile uint16_t *)(hero + 0x34u) |= 0x106Eu;
            *(volatile uint8_t *)(hero + 0x62u) = 0;
            *(volatile uint8_t *)(hero + 0x7Eu) |= 0x82u;
            if (*(volatile uintptr_t *)(hero + 0x68u)) {
                uintptr_t pvar = *(volatile uintptr_t *)(hero + 0x68u);
                *(volatile uintptr_t *)(pvar + 0x00u) = pvar + 0x30u;
                *(volatile uint32_t *)(pvar + 0x30u) =
                    *(volatile uint32_t *)(hero + 0x2Cu);
                *(volatile uint16_t *)(pvar + 0x34u) = 15u;
                *(volatile uint8_t *)(pvar + 0x3Eu) = 1u;
                *(volatile uint32_t *)(pvar + 0x40u) = 0x3F266666u;
            }
            if (*(volatile uintptr_t *)(hero + 0x24u))
                *(volatile uint8_t *)
                    (*(volatile uintptr_t *)(hero + 0x24u) + 0x46u) = 5u;
            if (*(volatile uintptr_t *)(hero + 0x24u) &&
                *(volatile uintptr_t *)
                    (*(volatile uintptr_t *)(hero + 0x24u) + 0x48u)) {
                uintptr_t animation_data =
                    *(volatile uintptr_t *)
                        (*(volatile uintptr_t *)(hero + 0x24u) + 0x48u);
                uint8_t animation_count =
                    *(volatile uint8_t *)(animation_data + 0x10u);
                *(volatile uint8_t *)(hero + 0x40u) = 0;
                *(volatile uint8_t *)(hero + 0x41u) =
                    animation_count > 1u ? 1u : (uint8_t)(animation_count - 1u);
                *(volatile uint8_t *)(hero + 0x42u) = 0;
                *(volatile uint8_t *)(hero + 0x43u) = 0;
                *(volatile uint32_t *)(hero + 0x44u) = 0;
                ((void (*)(uintptr_t))((rc3_base + 0x001787DAu) | 1u))(hero);
            }
            ((void (*)(uintptr_t))((rc3_base + 0x000B4C28u) | 1u))(hero);
            __sync_synchronize();
            sync_arena_player_to_hero();
            snprintf(line, sizeof(line),
                     "Arena hero init: class_data=%08X pvar=%08X model=%08X "
                     "class_update=%08X moby_update=%08X moby_model=%08X "
                     "mode=%02X state=%02X flags=%04X.\n",
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x24u),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x68u),
                     *(volatile uintptr_t *)(hero + 0x24u)
                         ? (unsigned int)*(volatile uintptr_t *)
                             (*(volatile uintptr_t *)(hero + 0x24u) + 0x10u)
                         : 0u,
                     *(volatile uintptr_t *)(hero + 0x24u)
                         ? (unsigned int)*(volatile uintptr_t *)
                             (*(volatile uintptr_t *)(hero + 0x24u) + 0x40u)
                         : 0u,
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x78u),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x98u),
                     (unsigned int)*(volatile uint8_t *)(hero + 0x7Eu),
                     (unsigned int)*(volatile uint8_t *)(hero + 0x31u),
                     (unsigned int)*(volatile uint16_t *)(hero + 0x34u));
            append_log(line);
            snprintf(line, sizeof(line),
                     "Arena hero render: scale=%08X anim=%08X time=%08X "
                     "speed=%08X duration=%08X frame0=%08X frame1=%08X "
                     "update=%08X owner=%08X.\n",
                     (unsigned int)*(volatile uint32_t *)(hero + 0x2Cu),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x40u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x44u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x48u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x4Cu),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x58u),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x5Cu),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x78u),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x90u));
            append_log(line);
            if (*(volatile uintptr_t *)(hero + 0x98u)) {
                uintptr_t model = *(volatile uintptr_t *)(hero + 0x98u);
                snprintf(line, sizeof(line),
                         "Arena hero model: %08X header=%08X,%08X,%08X,%08X,"
                         "%08X,%08X,%08X,%08X.\n",
                         (unsigned int)model,
                         (unsigned int)*(volatile uint32_t *)(model + 0x00u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x04u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x08u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x0Cu),
                         (unsigned int)*(volatile uint32_t *)(model + 0x10u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x14u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x18u),
                         (unsigned int)*(volatile uint32_t *)(model + 0x1Cu));
                append_log(line);
            }
            ((VoidFn)((rc3_base + 0x0017F4FCu) | 1u))();
            append_log("Arena hero: rebuilt retained Moby draw list after dynamic spawn.\n");
            {
                volatile uintptr_t *special =
                    (volatile uintptr_t *)(rc3_base + 0x00BABEACu);
                int special_count = 0;
                int special_hero_index = -1;
                while (special_count < 64 && special[special_count]) {
                    if (special[special_count] == hero)
                        special_hero_index = special_count;
                    ++special_count;
                }
                if (special_hero_index < 0 && special_count < 63) {
                    special[special_count] = hero;
                    special[special_count + 1] = 0;
                    special_hero_index = special_count;
                    ++special_count;
                    append_log("Arena hero: appended missing Hero to retained special list.\n");
                }
                snprintf(line, sizeof(line),
                         "Arena hero special list: count=%d hero_index=%d.\n",
                         special_count, special_hero_index);
                append_log(line);
            }
            snprintf(line, sizeof(line),
                     "Arena hero lists: special=%08X,%08X,%08X,%08X "
                     "active=%08X hero_next=%08X update=%08X "
                     "mode=%02X flags=%04X.\n",
                     (unsigned int)*(volatile uintptr_t *)
                         (rc3_base + 0x00BABEACu),
                     (unsigned int)*(volatile uintptr_t *)
                         (rc3_base + 0x00BABEB0u),
                     (unsigned int)*(volatile uintptr_t *)
                         (rc3_base + 0x00BABEB4u),
                     (unsigned int)*(volatile uintptr_t *)
                         (rc3_base + 0x00BABEB8u),
                     (unsigned int)*(volatile uintptr_t *)
                         (rc3_base + 0x00B8F2ACu),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x28u),
                     (unsigned int)*(volatile uintptr_t *)(hero + 0x64u),
                     (unsigned int)*(volatile uint8_t *)(hero + 0x7Eu),
                     (unsigned int)*(volatile uint16_t *)(hero + 0x34u));
            append_log(line);
            ((VoidFn)((rc3_base + 0x00108644u) | 1u))();
            append_log("Arena hero: retained view initialized.\n");
            snprintf(line, sizeof(line),
                     "Arena post-slot hero: record=%08X state=%08X hero=%08X "
                     "before=%08X,%08X,%08X after=%08X,%08X,%08X "
                     "state60=%08X,%08X,%08X state130=%08X "
                     "state1AC=%08X state1B0=%08X.\n",
                     (unsigned int)record, (unsigned int)player_state,
                     (unsigned int)hero, (unsigned int)before_x,
                     (unsigned int)before_y, (unsigned int)before_z,
                     (unsigned int)*(volatile uint32_t *)(hero + 0x10u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x14u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x18u),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x60u),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x64u),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x68u),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x130u),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x1ACu),
                     (unsigned int)*(volatile uint32_t *)(player_state + 0x1B0u));
            append_log(line);
            snprintf(line, sizeof(line),
                     "Arena spawn: Player Config cuboid=%u team=%u "
                     "position=%08X,%08X,%08X propagated to slot/record/Hero.\n",
                     team ? 0u : 1u, local_team_index,
                     (unsigned int)x, (unsigned int)y,
                     (unsigned int)z);
            append_log(line);
            snprintf(line, sizeof(line),
                     "Arena camera seed: slot190=%08X,%08X,%08X -> "
                     "%08X,%08X,%08X from Hero spawn; global=%08X.\n",
                     (unsigned int)camera_before_x,
                     (unsigned int)camera_before_y,
                     (unsigned int)camera_before_z,
                     (unsigned int)*(volatile uint32_t *)(slot + 0x190u),
                     (unsigned int)*(volatile uint32_t *)(slot + 0x194u),
                     (unsigned int)*(volatile uint32_t *)(slot + 0x198u),
                     (unsigned int)global_camera);
            append_log(line);
            arena_post_slot_hero_positioned = 1;
        } else {
            append_log("Arena post-slot hero: incomplete player chain; placement skipped.\n");
        }
    }
    if (verbose)
        slot_probe_log(2, "exit");
    ++slot_probe_calls[2];
    return result;
}
DEFINE_SLOT_PROBE(3)
DEFINE_SLOT_PROBE(4)

static void *const slot_probe_hooks[SLOT_PROBE_COUNT] = {
    slot_probe_hook_0, slot_probe_hook_1, slot_probe_hook_2,
    slot_probe_hook_3, slot_probe_hook_4
};

static void install_slot_probes(void) {
    int index;
    int failures = 0;
    for (index = 0; index < SLOT_PROBE_COUNT; ++index) {
        slot_probe_uids[index] = taiHookFunctionOffset(
            &slot_probe_refs[index], rc3_modid, 0, slot_probe_offsets[index],
            1, slot_probe_hooks[index]);
        if (slot_probe_uids[index] < 0)
            ++failures;
    }
    append_log(failures ? "Slot probe: one or more hooks failed.\n"
                        : "Slot probe: v23 bracket hooks installed.\n");
}

static void release_slot_probes(void) {
    int index;
    for (index = SLOT_PROBE_COUNT - 1; index >= 0; --index) {
        if (slot_probe_uids[index] >= 0) {
            taiHookRelease(slot_probe_uids[index], slot_probe_refs[index]);
            slot_probe_uids[index] = -1;
        }
    }
}

static void continue_player_object_callback(uintptr_t object) {
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)player_object_callback_ref;
    struct _tai_hook_user *next = (struct _tai_hook_user *)current->next;
    PlayerObjectCallbackFn function =
        (PlayerObjectCallbackFn)(next ? next->func : current->old);
    function(object);
}

static void player_object_callback_hook(uintptr_t object) {
    unsigned int player = 0xFFFFFFFFu;
    uintptr_t slot = 0;
    uintptr_t attached = 0;
    char line[160];

    if (object)
        player = *(volatile uint8_t *)(object + 0xBCu);
    if (player < 4u) {
        slot = rc3_base + 0x009D4B80u + (uintptr_t)player * 0x460u;
        attached = *(volatile uintptr_t *)(slot + 0x240u);
        if (!attached) {
            unsigned int bit = 1u << player;
            if (!(player_object_missing_mask & bit)) {
                player_object_missing_mask |= bit;
                snprintf(line, sizeof(line),
                         "Player object guard: player=%u object=%08X "
                         "slot=%08X attached240=null; skipped.\n",
                         player, (unsigned int)object, (unsigned int)slot);
                append_log(line);
            }
            return;
        }
    }
    continue_player_object_callback(object);
}

static void continue_player_position_setup(int player, int use_camera) {
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)player_position_setup_ref;
    struct _tai_hook_user *next = (struct _tai_hook_user *)current->next;
    PlayerPositionSetupFn function =
        (PlayerPositionSetupFn)(next ? next->func : current->old);
    function(player, use_camera);
}

/* Multiplayer player construction normally installs a camera/target object
 * at player_state + 0x140 before this placement pass.  The Vita port retained
 * the placement pass but stripped the lobby/match constructors that populate
 * that pointer.  Log the exact chain and skip only the unsafe calculation so
 * the next missing dependency can be identified from a clean retail core. */
static void player_position_setup_hook(int player, int use_camera) {
    uintptr_t slot = rc3_base + 0x009D4B80u;
    uintptr_t record = 0;
    uintptr_t player_state = 0;
    uintptr_t target = 0;
    char line[192];

    if ((unsigned int)player < 4u) {
        slot += (uintptr_t)player * 0x460u;
        record = *(volatile uintptr_t *)(slot + 0x50u);
        if (record)
            player_state = *(volatile uintptr_t *)(record + 0x70u);
        if (player_state)
            target = *(volatile uintptr_t *)(player_state + 0x140u);
    }

    if (use_camera && player_state && !target) {
        snprintf(line, sizeof(line),
                 "Player setup guard: player=%d slot=%08X record=%08X "
                 "state=%08X target140=null; skipped.\n",
                 player, (unsigned int)slot, (unsigned int)record,
                 (unsigned int)player_state);
        append_log(line);
        return;
    }
    continue_player_position_setup(player, use_camera);
}

static int install_player_position_guard(void) {
    if (player_position_setup_hook_uid >= 0)
        return 0;
    player_position_setup_hook_uid = taiHookFunctionOffset(
        &player_position_setup_ref, rc3_modid, 0,
        PLAYER_POSITION_SETUP_OFFSET, 1, player_position_setup_hook);
    if (player_position_setup_hook_uid < 0) {
        append_log("Player setup guard: hook installation failed.\n");
        return -1;
    }
    append_log("Player setup guard: installed for arena transition.\n");
    return 0;
}

static int install_player_object_guard(void) {
    if (player_object_callback_hook_uid >= 0)
        return 0;
    player_object_missing_mask = 0;
    player_object_callback_hook_uid = taiHookFunctionOffset(
        &player_object_callback_ref, rc3_modid, 0,
        PLAYER_OBJECT_CALLBACK_OFFSET, 1, player_object_callback_hook);
    if (player_object_callback_hook_uid < 0) {
        append_log("Player object guard: hook installation failed.\n");
        return -1;
    }
    append_log("Player object guard: installed for arena transition.\n");
    return 0;
}

static uintptr_t allocator_list_next_hook(uintptr_t node) {
    volatile uintptr_t *head_ptr =
        (volatile uintptr_t *)(rc3_base + 0x0089DDD4u);
    char line[192];
    struct _tai_hook_user *current;
    struct _tai_hook_user *next;
    AllocatorListNextFn function;

    if (node >= 0x80000000u && node < 0xA0000000u) {
        uint32_t slot = *(volatile uint32_t *)(node + 0x48u);
        /* The list head must refer to an allocator node whose +0x48 field is
         * a small one-based table index.  Level 44 instead leaves the head
         * pointing into float-heavy geometry at 0x8B043F20. */
        if (slot > 0x00010000u) {
            const volatile uint32_t *words =
                (const volatile uint32_t *)node;
            *head_ptr = 0;
            __sync_synchronize();
            if (!allocator_bad_link_seen) {
                allocator_bad_link_seen = 1;
                snprintf(line, sizeof(line),
                         "Allocator repair: cleared head node=%08X "
                         "slot=%08X w0=%08X w1=%08X w16=%08X.\n",
                         (unsigned int)node, (unsigned int)slot,
                         (unsigned int)words[0], (unsigned int)words[1],
                         (unsigned int)words[16]);
                append_log(line);
            }
            return 0;
        }
    }

    current = (struct _tai_hook_user *)allocator_list_next_ref;
    next = (struct _tai_hook_user *)current->next;
    function = (AllocatorListNextFn)(next ? next->func : current->old);
    return function(node);
}

static int install_allocator_guard(void) {
    if (allocator_list_next_hook_uid >= 0)
        return 0;
    allocator_bad_link_seen = 0;
    allocator_list_next_hook_uid = taiHookFunctionOffset(
        &allocator_list_next_ref, rc3_modid, 0,
        ALLOCATOR_LIST_NEXT_OFFSET, 1, allocator_list_next_hook);
    if (allocator_list_next_hook_uid < 0) {
        append_log("Allocator repair: hook installation failed.\n");
        return -1;
    }
    append_log("Allocator repair: installed for arena transition.\n");
    return 0;
}

static uintptr_t continue_arena_stage(int index, uintptr_t a0, uintptr_t a1,
                                      uintptr_t a2, uintptr_t a3) {
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)arena_stage_refs[index];
    struct _tai_hook_user *next = (struct _tai_hook_user *)current->next;
    ArenaStageFn function = (ArenaStageFn)(next ? next->func : current->old);
    return function(a0, a1, a2, a3);
}

#define DEFINE_ARENA_STAGE_HOOK(index, name)                              \
    static uintptr_t arena_stage_hook_##index(                            \
        uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3) {         \
        uintptr_t result;                                                 \
        if (arena_stage_trace_active)                                     \
            append_log("Arena stage: " name " entered.\n");             \
        result = continue_arena_stage(index, a0, a1, a2, a3);             \
        if (arena_stage_trace_active)                                     \
            append_log("Arena stage: " name " returned.\n");            \
        return result;                                                    \
    }

DEFINE_ARENA_STAGE_HOOK(0, "ready gate")
DEFINE_ARENA_STAGE_HOOK(1, "transition delay")
DEFINE_ARENA_STAGE_HOOK(2, "task reset")
DEFINE_ARENA_STAGE_HOOK(3, "front-end close")
DEFINE_ARENA_STAGE_HOOK(4, "scene close")
DEFINE_ARENA_STAGE_HOOK(5, "transition prepare")
DEFINE_ARENA_STAGE_HOOK(6, "local session finalize")
DEFINE_ARENA_STAGE_HOOK(7, "state cleanup")
DEFINE_ARENA_STAGE_HOOK(8, "render reset")
DEFINE_ARENA_STAGE_HOOK(9, "level bind")
DEFINE_ARENA_STAGE_HOOK(10, "callback setup")
DEFINE_ARENA_STAGE_HOOK(11, "transition finalize")
DEFINE_ARENA_STAGE_HOOK(12, "multiplayer loader init")

static uintptr_t arena_stage_hook_13(uintptr_t a0, uintptr_t a1,
                                     uintptr_t a2, uintptr_t a3) {
    volatile int *timer = (volatile int *)(rc3_base + 0x00FBBF80u);
    volatile int *state = (volatile int *)(rc3_base + 0x00FBBF84u);
    uintptr_t original_signal = a0;
    uintptr_t slot = rc3_base + 0x009D4B80u;
    uintptr_t record = *(volatile uintptr_t *)(slot + 0x50u);
    uintptr_t player_state =
        record ? *(volatile uintptr_t *)(record + 0x70u) : 0;
    uintptr_t hero = *(volatile uintptr_t *)(slot + 0x240u);
    uintptr_t state_hero =
        player_state ? *(volatile uintptr_t *)(player_state + 0x140u) : 0;
    uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                MAIN_CONTEXT_PTR_DATA_OFFSET);
    int current_level =
        *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET);
    int player_count =
        *(volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET);
    int ready = arena_gate_override_active && *state == 3 &&
                current_level == arena_target_level && context &&
                *(volatile uint8_t *)(context + 0xC8u) &&
                player_count == 1 && record && player_state && hero &&
                state_hero == hero;
    char line[256];

    if (ready) {
        if (!arena_gate_hero_positioned) {
            uint32_t before_x = *(volatile uint32_t *)(hero + 0x10u);
            uint32_t before_y = *(volatile uint32_t *)(hero + 0x14u);
            uint32_t before_z = *(volatile uint32_t *)(hero + 0x18u);
            uint32_t x = *(volatile uint32_t *)(slot + 0x00u);
            uint32_t y = *(volatile uint32_t *)(slot + 0x04u);
            uint32_t z = *(volatile uint32_t *)(slot + 0x08u);
            uint32_t w = *(volatile uint32_t *)(slot + 0x0Cu);

            *(volatile uint32_t *)(hero + 0x10u) = x;
            *(volatile uint32_t *)(hero + 0x14u) = y;
            *(volatile uint32_t *)(hero + 0x18u) = z;
            *(volatile uint32_t *)(hero + 0x1Cu) = w;
            __sync_synchronize();
            snprintf(line, sizeof(line),
                     "Arena hero placement: slot=%08X,%08X,%08X "
                     "hero_before=%08X,%08X,%08X "
                     "hero_after=%08X,%08X,%08X.\n",
                     (unsigned int)x, (unsigned int)y, (unsigned int)z,
                     (unsigned int)before_x, (unsigned int)before_y,
                     (unsigned int)before_z,
                     (unsigned int)*(volatile uint32_t *)(hero + 0x10u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x14u),
                     (unsigned int)*(volatile uint32_t *)(hero + 0x18u));
            append_log(line);
            arena_gate_hero_positioned = 1;
        }
        a0 = 1;
    }

    uintptr_t result = continue_arena_stage(13, a0, a1, a2, a3);
    int current_state = *state;
    int current_result = (int)result;

    if (arena_stage_trace_active &&
        (current_state != arena_last_mp_update_state ||
         current_result != arena_last_mp_update_result)) {
        char line[144];
        snprintf(line, sizeof(line),
                 "Arena gate: result=%d state=%d timer=%d signal=%lu "
                 "original=%lu ready=%d.\n",
                 current_result, current_state, *timer, (unsigned long)a0,
                 (unsigned long)original_signal, ready);
        append_log(line);
        arena_last_mp_update_state = current_state;
        arena_last_mp_update_result = current_result;
    }
    if (arena_gate_override_active && current_result == 1) {
        arena_gate_override_active = 0;
        append_log("Arena gate: staging completed through retained update.\n");
    }
    return result;
}

static void *const arena_stage_hooks[ARENA_STAGE_COUNT] = {
    arena_stage_hook_0, arena_stage_hook_1, arena_stage_hook_2,
    arena_stage_hook_3, arena_stage_hook_4, arena_stage_hook_5,
    arena_stage_hook_6, arena_stage_hook_7, arena_stage_hook_8,
    arena_stage_hook_9, arena_stage_hook_10, arena_stage_hook_11,
    arena_stage_hook_12, arena_stage_hook_13
};

static const uint32_t arena_stage_offsets[ARENA_STAGE_COUNT] = {
    ARENA_STAGE_READY_OFFSET,
    0,
    0,
    ARENA_STAGE_FRONTEND_CLOSE_OFFSET,
    ARENA_STAGE_SCENE_CLOSE_OFFSET,
    ARENA_STAGE_PREPARE_OFFSET,
    ARENA_STAGE_SESSION_OFFSET,
    ARENA_STAGE_STATE_CLEAN_OFFSET,
    0,
    0,
    ARENA_STAGE_CALLBACK_SETUP_OFFSET,
    0,
    ARENA_STAGE_MP_INIT_OFFSET,
    ARENA_STAGE_MP_UPDATE_OFFSET
};

static int install_arena_stage_hooks(void) {
    int index;
    int failures = 0;
    char line[112];
    for (index = 0; index < ARENA_STAGE_COUNT; ++index) {
        if (!arena_stage_offsets[index])
            continue;
        if (arena_stage_hook_uids[index] >= 0)
            continue;
        arena_stage_hook_uids[index] = taiHookFunctionOffset(
            &arena_stage_refs[index], rc3_modid, 0,
            arena_stage_offsets[index], 1, arena_stage_hooks[index]);
        if (arena_stage_hook_uids[index] < 0) {
            snprintf(line, sizeof(line),
                     "Arena diagnostic: stage hook %d failed (%d).\n",
                     index, arena_stage_hook_uids[index]);
            append_log(line);
            ++failures;
        }
    }
    if (failures)
        append_log("Arena diagnostic: one or more deferred stage hooks failed.\n");
    else
        append_log("Arena diagnostic: deferred stage hooks installed.\n");
    return failures ? -1 : 0;
}

static void release_arena_stage_hooks(void) {
    int index;
    arena_stage_trace_active = 0;
    for (index = ARENA_STAGE_COUNT - 1; index >= 0; --index) {
        if (arena_stage_hook_uids[index] >= 0) {
            taiHookRelease(arena_stage_hook_uids[index],
                           arena_stage_refs[index]);
            arena_stage_hook_uids[index] = -1;
        }
    }
}

/* Reproduce the final local-player setup performed by the PS3 Local Profiles
 * Start handler before it hands the selected arena to SetLevel.  Keep this in
 * a separate helper so every experimental arena handoff uses the same guarded
 * sequence and records the game's own acceptance result. */
static int prepare_single_local_player(void) {
    ResetLocalSessionFn reset_local_session =
        (ResetLocalSessionFn)((rc3_base + RESET_LOCAL_SESSION_OFFSET) | 1u);
    RegisterLocalPlayerFn register_local_player =
        (RegisterLocalPlayerFn)((rc3_base + REGISTER_LOCAL_PLAYER_OFFSET) | 1u);
    int result;
    char line[112];

    reset_local_session(0);
    /* The PS3 LobbyGUI first records this as kind 2, then its stripped match
     * manager promotes the selected profile into the active local roster.
     * For the one-player restoration target, request that final kind-1 state
     * directly so the retained code owns both the count and profile array. */
    result = register_local_player(0, 1, 0, 0, 0);
    snprintf(line, sizeof(line),
             "Arena bootstrap: local player registration returned %d.\n",
             result);
    append_log(line);
    return result;
}

/* The PS3 lobby runner sets two multiplayer bytes before constructing the
 * match and player managers.  They live in different objects: +0xC8 in the
 * main context and +0x28C in the state returned by 0x8102EDE0.  The Vita
 * runner was changed to clear them; the instruction patches restore the
 * original stores, and these runtime writes cover later resets. */
static int enforce_multiplayer_context(void) {
    typedef uintptr_t (*GetMultiplayerStateFn)(void);
    GetMultiplayerStateFn get_multiplayer_state =
        (GetMultiplayerStateFn)((rc3_base +
                                 MULTIPLAYER_STATE_GETTER_OFFSET) | 1u);
    uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                MAIN_CONTEXT_PTR_DATA_OFFSET);
    uintptr_t multiplayer_state;
    if (!context) {
        append_log("Arena bootstrap: main context pointer is null.\n");
        return -1;
    }
    multiplayer_state = get_multiplayer_state();
    if (!multiplayer_state) {
        append_log("Arena bootstrap: multiplayer state pointer is null.\n");
        return -1;
    }
    *(volatile uint8_t *)(context + 0xC8u) = 1;
    *(volatile uint8_t *)(multiplayer_state + 0x28Cu) = 1;
    __sync_synchronize();
    append_log("Arena bootstrap: multiplayer context bytes set.\n");
    return 0;
}

static int request_test_arena(void) {
    SetLevelFn set_level =
        (SetLevelFn)((rc3_base + SET_LEVEL_OFFSET) | 1u);
    int result;

    if (enforce_multiplayer_context() != 0)
        return -1;
    result = prepare_single_local_player();

    if (result != 0) {
        append_log("Arena bootstrap: level request blocked by registration failure.\n");
        return result;
    }
    arena_last_mp_update_state = -999;
    arena_last_mp_update_result = -999;
    arena_gate_hero_positioned = 0;
    arena_post_slot_hero_positioned = 0;
    arena_game_update_calls = 0;
    player_controller_update_calls = 0;
    arena_frame_owner_calls = 0;
    arena_player_sync_calls = 0;
    arena_final_record_seeded = 0;
    arena_controller_position_valid = 0;
    arena_visibility_repair_logged = 0;
    arena_sample_hero = 0;
    arena_draw_trace_calls = 0;
    arena_draw_trace_count = 0;
    arena_draw_trace_logged = 0;
    memset(arena_draw_traces, 0, sizeof(arena_draw_traces));
    arena_gate_override_active = 1;
    if (arena_stage_hook_uids[13] < 0) {
        arena_stage_hook_uids[13] = taiHookFunctionOffset(
            &arena_stage_refs[13], rc3_modid, 0,
            ARENA_STAGE_MP_UPDATE_OFFSET, 1, arena_stage_hook_13);
        if (arena_stage_hook_uids[13] < 0) {
            arena_gate_override_active = 0;
            append_log("Arena gate: dedicated update hook failed; request blocked.\n");
            return -1;
        }
        append_log("Arena gate: dedicated update hook installed.\n");
    }
    if (general_game_update_hook_uid < 0) {
        general_game_update_hook_uid = taiHookFunctionOffset(
            &general_game_update_ref, rc3_modid, 0,
            GENERAL_GAME_UPDATE_OFFSET, 1, general_game_update_hook);
        if (general_game_update_hook_uid < 0)
            append_log("Arena game update: hook installation failed.\n");
        else
            append_log("Arena game update: scheduler hook installed.\n");
    }
    if (player_controller_update_hook_uid < 0) {
        player_controller_update_hook_uid = taiHookFunctionOffset(
            &player_controller_update_ref, rc3_modid, 0,
            PLAYER_CONTROLLER_UPDATE_OFFSET, 1,
            player_controller_update_hook);
        if (player_controller_update_hook_uid < 0)
            append_log("Arena player controller: trace hook installation failed.\n");
        else
            append_log("Arena player controller: trace hook installed.\n");
    }
    if (gameplay_frame_owner_hook_uid < 0) {
        gameplay_frame_owner_hook_uid = taiHookFunctionOffset(
            &gameplay_frame_owner_ref, rc3_modid, 0,
            GAMEPLAY_FRAME_OWNER_OFFSET, 1, gameplay_frame_owner_hook);
        if (gameplay_frame_owner_hook_uid < 0)
            append_log("Arena frame owner: hook installation failed.\n");
        else
            append_log("Arena frame owner: hook installed.\n");
    }
    if (moby_draw_builder_hook_uid < 0) {
        moby_draw_builder_hook_uid = taiHookFunctionOffset(
            &moby_draw_builder_ref, rc3_modid, 0,
            MOBY_DRAW_BUILDER_OFFSET, 1, moby_draw_builder_hook);
        if (moby_draw_builder_hook_uid < 0)
            append_log("Arena draw trace: builder hook installation failed.\n");
        else
            append_log("Arena draw trace: builder hook installed.\n");
    }
    /* State 39 uses the same long-lived asynchronous transition routine.  A
     * permanent hook changes that lobby transition's lifetime, so attach the
     * diagnostic only after the lobby has completed and Start is pressed. */
    if (!arena_clean_core_control) {
        if (arena_transition_hook_uid < 0) {
            arena_transition_hook_uid = taiHookFunctionOffset(
                &arena_transition_ref, rc3_modid, 0,
                ARENA_TRANSITION_OFFSET, 1, arena_transition_hook);
            if (arena_transition_hook_uid < 0)
                append_log("Arena diagnostic: deferred transition hook failed.\n");
            else
                append_log("Arena diagnostic: deferred transition hook installed.\n");
        }
        install_arena_stage_hooks();
    } else {
        append_log("Arena diagnostic: clean core control; broad hooks disabled.\n");
    }
    install_allocator_guard();
    install_player_position_guard();
    install_player_object_guard();
    arena_roster_repair_active = 1;
    install_reset_local_session_repair();
    append_log("Arena bootstrap: requesting Siege test on Metropolis level 44.\n");
    set_level(arena_target_level);
    append_log("Arena bootstrap: Metropolis request returned.\n");
    arena_stage_trace_active = 1;
    return 0;
}
#endif

/* Phase one deliberately leaves both restored actions inert.  The callbacks
 * prove that native menu dispatch works without entering an uninitialized
 * multiplayer state or loading an arena directly. */
#ifdef RC3_LOBBY_TEST
static void multiplayer_callback(void) {
    if (lobby_scene_ready) {
        append_log("Native Multiplayer row: reusing loaded state-39 "
                   "LobbyGUI resources.\n");
        lobby_reopen_pending = 1;
        return;
    }
    append_log("Native Multiplayer row: requesting state-39 lobby scene.\n");
    state39_arena_launch_pending = 0;
    state39_request_pending = 1;
}
#else
static void multiplayer_callback(void) {
    append_log("Native Multiplayer row selected.\n");
}
#endif

static void multiplayer_tutorial_callback(void) {
    append_log("Native Multiplayer Tutorial row selected.\n");
}

#ifdef RC3_LOBBY_TEST
#define TEXT_LOBBY_ONLINE        0x0F56u
#define TEXT_LOBBY_LOCAL         0x0F58u
#define TEXT_LOBBY_EDIT_PROFILES 0x18CCu
#define TEXT_LOBBY_EXIT          0x15CEu
#define TEXT_LOCAL_PLAYER_1      0x0FF5u
#define TEXT_LOCAL_PLAYER_2      0x0FF6u
#define TEXT_LOCAL_START         0x0FF9u
#define TEXT_LOCAL_BACK          0x0CD5u
#define TEXT_LOCAL_TEAM_FIRST    0x0F1Au
#define TEXT_LOCAL_TEAM_COUNT    8u
#define TEXT_LOCAL_SKIN_FIRST    0x0F22u
#define TEXT_LOCAL_SKIN_COUNT    7u

static void lobby_online_callback(void) {
    append_log("Native lobby: Online Play selected (not wired yet).\n");
}

static void lobby_local_callback(void) {
    append_log("Native lobby: scheduling Local Players screen.\n");
    local_menu_pending = 1;
}

static void lobby_profiles_callback(void) {
    append_log("Native lobby: Edit Profiles selected (not wired yet).\n");
}

static void lobby_exit_callback(void) {
    append_log("Native lobby: scheduling return to title table.\n");
    title_menu_pending = 1;
}

static void local_player_1_callback(void) {
    append_log("Native Local Players: Player 1 is the active local slot.\n");
}

static void local_team_callback(void) {
    local_team_index = (local_team_index + 1u) % TEXT_LOCAL_TEAM_COUNT;
    append_log("Native Local Players: cycled Player 1 team.\n");
    local_menu_pending = 1;
}

static void local_skin_callback(void) {
    local_skin_index = (local_skin_index + 1u) % TEXT_LOCAL_SKIN_COUNT;
    append_log("Native Local Players: cycled Player 1 skin.\n");
    local_menu_pending = 1;
}

static void local_start_callback(void) {
    append_log("Native Local Players: scheduling state-39 arena handoff.\n");
    state39_arena_launch_pending = 1;
    state39_request_pending = 1;
}

static void local_back_callback(void) {
    append_log("Native Local Players: scheduling return to lobby main.\n");
    lobby_menu_pending = 1;
}

static void initialize_lobby_table(void) {
    memset(restored_menu, 0, sizeof(restored_menu));

    restored_menu[0].widget_id = 0x0C;
    restored_menu[0].group_id = 0x0D;
    restored_menu[0].text_id = TEXT_LOBBY_ONLINE;
    restored_menu[0].enabled = 1;
    restored_menu[0].callback = lobby_online_callback;

    restored_menu[1].widget_id = 0x0D;
    restored_menu[1].group_id = 0x0D;
    restored_menu[1].text_id = TEXT_LOBBY_LOCAL;
    restored_menu[1].enabled = 1;
    restored_menu[1].callback = lobby_local_callback;

    restored_menu[2].widget_id = 0x0E;
    restored_menu[2].group_id = 0x0D;
    restored_menu[2].text_id = TEXT_LOBBY_EDIT_PROFILES;
    restored_menu[2].enabled = 1;
    restored_menu[2].callback = lobby_profiles_callback;

    restored_menu[3].widget_id = 0x0F;
    restored_menu[3].group_id = 0x0D;
    restored_menu[3].text_id = TEXT_LOBBY_EXIT;
    restored_menu[3].enabled = 1;
    restored_menu[3].callback = lobby_exit_callback;
}

/* First functional replacement for the PS3 Local Profiles screen.  Vita does
 * not contain the PS3 screen's multi-column widget classes, so the retained
 * title-list renderer presents one local slot followed by its live team and
 * skin values.  Selecting a value cycles it and rebuilds this same table.
 * The underlying locale IDs and order exactly match the PS3 constructor. */
static void initialize_local_players_table(void) {
    memset(restored_menu, 0, sizeof(restored_menu));

    restored_menu[0].widget_id = 0x0C;
    restored_menu[0].group_id = 0x0D;
    restored_menu[0].text_id = TEXT_LOCAL_PLAYER_1;
    restored_menu[0].enabled = 1;
    restored_menu[0].callback = local_player_1_callback;

    restored_menu[1].widget_id = 0x0D;
    restored_menu[1].group_id = 0x0D;
    restored_menu[1].text_id = TEXT_LOCAL_TEAM_FIRST + local_team_index;
    restored_menu[1].enabled = 1;
    restored_menu[1].callback = local_team_callback;

    restored_menu[2].widget_id = 0x0E;
    restored_menu[2].group_id = 0x0D;
    restored_menu[2].text_id = TEXT_LOCAL_SKIN_FIRST + local_skin_index;
    restored_menu[2].enabled = 1;
    restored_menu[2].callback = local_skin_callback;

    restored_menu[3].widget_id = 0x0F;
    restored_menu[3].group_id = 0x0D;
    restored_menu[3].text_id = TEXT_LOCAL_START;
    restored_menu[3].enabled = 1;
    restored_menu[3].callback = local_start_callback;

    restored_menu[4].widget_id = 0x10;
    restored_menu[4].group_id = 0x0D;
    restored_menu[4].text_id = TEXT_LOCAL_BACK;
    restored_menu[4].enabled = 1;
    restored_menu[4].callback = local_back_callback;
}
#endif

#ifdef RC3_LOBBY_TEST
static int lobby_worker_main(SceSize argc, void *args) {
    SceCtrlData pad;
    uint32_t previous = 0;
    int last_level = 0x7FFFFFFF;
    int last_transition = 0x7FFFFFFF;
    int last_request = 0x7FFFFFFF;
    (void)argc;
    (void)args;

    memset(&pad, 0, sizeof(pad));
    while (lobby_worker_running) {
        volatile const int *current_level =
            (volatile const int *)(rc3_base + CURRENT_LEVEL_OFFSET);
        volatile const int *transition =
            (volatile const int *)(rc3_data_base +
                                   FRONTEND_TRANSITION_DATA_OFFSET);
        volatile const int *request =
            (volatile const int *)(rc3_data_base +
                                   FRONTEND_REQUEST_DATA_OFFSET);
        int current = *current_level;

        if (state39_handoff_armed && state39_completion_seen &&
            !state39_controller_started) {
            if ((current == -1 || current == 39) &&
                *transition == 0 && *request == 0 &&
                state39_activation_frames > 0) {
                --state39_activation_frames;
            } else if ((current == -1 || current == 39) &&
                       *transition == 0 && *request == 0) {
                state39_controller_started = 1;
                lobby_ui_active = 1;
                lobby_native_rebuild_pending = 1;
                previous = 0;
                append_log("LobbyGUI manager: screen 0x18 Main entered; "
                           "native widget rebuild scheduled.\n");
            } else if (current == 39 && state39_activation_frames > 0) {
                --state39_activation_frames;
            } else if (current == 39) {
                typedef void (*SetFrontendModeFn)(int mode);
                SetFrontendModeFn set_frontend_mode =
                    (SetFrontendModeFn)
                        ((rc3_base + FRONTEND_MODE_SET_OFFSET) | 1u);

                *(volatile int *)(rc3_data_base +
                    FRONTEND_REQUEST_DATA_OFFSET) = 0;
                __sync_synchronize();
                *(volatile int *)(rc3_data_base +
                    FRONTEND_TRANSITION_DATA_OFFSET) = 0;
                *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET) = -1;
                set_frontend_mode(0);
                state39_activation_frames = 30;
                append_log("State39 controller: cleared stale completed "
                           "loader ownership for LobbyGUI re-entry.\n");
            } else {
                state39_activation_frames = 30;
            }
        }
        if (state39_handoff_armed &&
            (current != last_level || *transition != last_transition ||
             *request != last_request)) {
            char line[112];
            snprintf(line, sizeof(line),
                     "State39 trace: level=%d transition=%d request=%d.\n",
                     current, *transition, *request);
            append_log(line);
            last_level = current;
            last_transition = *transition;
            last_request = *request;
        }

        if (lobby_ui_active && sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
            uint32_t pressed = pad.buttons & ~previous;
            int item_count =
                lobby_ui_screen == LOBBY_GUI_SCREEN_MAIN ? 4 : 5;
            if (pressed & SCE_CTRL_UP) {
                lobby_ui_selected =
                    (lobby_ui_selected + item_count - 1) % item_count;
                lobby_native_style_pending = 1;
            } else if (pressed & SCE_CTRL_DOWN) {
                lobby_ui_selected =
                    (lobby_ui_selected + 1) % item_count;
                lobby_native_style_pending = 1;
            }
            if (lobby_ui_screen == LOBBY_GUI_SCREEN_LOCAL_PROFILES &&
                (pressed & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT))) {
                int direction = (pressed & SCE_CTRL_RIGHT) ? 1 : -1;
                if (lobby_ui_selected == 1) {
                    local_team_index =
                        (local_team_index + TEXT_LOCAL_TEAM_COUNT +
                         direction) % TEXT_LOCAL_TEAM_COUNT;
                    append_log("LobbyGUI screen 0x16: Player 1 team changed.\n");
                } else if (lobby_ui_selected == 2) {
                    local_skin_index =
                        (local_skin_index + TEXT_LOCAL_SKIN_COUNT +
                         direction) % TEXT_LOCAL_SKIN_COUNT;
                    append_log("LobbyGUI screen 0x16: Player 1 skin changed.\n");
                }
            }
            if (pressed & SCE_CTRL_CROSS) {
                if (lobby_ui_screen == LOBBY_GUI_SCREEN_MAIN) {
                    if (lobby_ui_selected == 0) {
                        append_log("LobbyGUI screen 0x18: Online Play selected; "
                                   "network owner is unavailable.\n");
                    } else if (lobby_ui_selected == 1) {
                        lobby_ui_screen = LOBBY_GUI_SCREEN_LOCAL_PROFILES;
                        lobby_ui_selected = 0;
                        lobby_native_rebuild_pending = 1;
                        append_log("LobbyGUI manager: Local Play routed through "
                                   "selector 0x15 to screen 0x16 Local Profiles.\n");
                    } else if (lobby_ui_selected == 2) {
                        append_log("LobbyGUI screen 0x18: Edit Profiles selected; "
                                   "profile editor is not implemented.\n");
                    } else {
                        append_log("LobbyGUI screen 0x18: Exit Multiplayer selected.\n");
                        lobby_ui_active = 0;
                        lobby_native_cleanup_pending = 1;
                        state39_handoff_armed = 0;
                        state39_controller_started = 0;
                        *(volatile int *)(rc3_data_base +
                            FRONTEND_REQUEST_DATA_OFFSET) = -1;
                        __sync_synchronize();
                        *(volatile int *)(rc3_data_base +
                            FRONTEND_TRANSITION_DATA_OFFSET) = 1;
                        append_log("LobbyGUI manager: native title return "
                                   "requested.\n");
                    }
                } else {
                    if (lobby_ui_selected == 0)
                        append_log("LobbyGUI screen 0x16: Player 1 selected.\n");
                    else if (lobby_ui_selected == 1) {
                        local_team_index =
                            (local_team_index + 1u) % TEXT_LOCAL_TEAM_COUNT;
                        append_log("LobbyGUI screen 0x16: Player 1 team changed.\n");
                    } else if (lobby_ui_selected == 2) {
                        local_skin_index =
                            (local_skin_index + 1u) % TEXT_LOCAL_SKIN_COUNT;
                        append_log("LobbyGUI screen 0x16: Player 1 skin changed.\n");
                    } else if (lobby_ui_selected == 3) {
                        lobby_ui_active = 0;
                        lobby_native_cleanup_pending = 1;
                        state39_controller_started = 0;
                        if (current == 39) {
                            append_log("LobbyGUI screen 0x16: Start scheduled arena handoff.\n");
                            state39_arena_launch_pending = 1;
                        } else {
                            append_log("LobbyGUI screen 0x16: Start requesting arena level.\n");
                            state39_arena_launch_pending = 0;
                            state39_handoff_armed = 0;
                            request_test_arena();
                        }
                    } else {
                        lobby_ui_screen = LOBBY_GUI_SCREEN_MAIN;
                        lobby_ui_selected = 1;
                        lobby_native_rebuild_pending = 1;
                        append_log("LobbyGUI manager: returned to screen 0x18 Main.\n");
                    }
                }
            } else if (pressed & SCE_CTRL_CIRCLE) {
                if (lobby_ui_screen == LOBBY_GUI_SCREEN_LOCAL_PROFILES) {
                    lobby_ui_screen = LOBBY_GUI_SCREEN_MAIN;
                    lobby_ui_selected = 1;
                    lobby_native_rebuild_pending = 1;
                    append_log("LobbyGUI manager: Circle returned to screen 0x18 Main.\n");
                } else {
                    lobby_ui_selected = 3;
                    append_log("LobbyGUI screen 0x18: Circle selected Exit Multiplayer.\n");
                    lobby_ui_active = 0;
                    lobby_native_cleanup_pending = 1;
                    state39_handoff_armed = 0;
                    state39_controller_started = 0;
                    *(volatile int *)(rc3_data_base +
                        FRONTEND_REQUEST_DATA_OFFSET) = -1;
                    __sync_synchronize();
                    *(volatile int *)(rc3_data_base +
                        FRONTEND_TRANSITION_DATA_OFFSET) = 1;
                    append_log("LobbyGUI manager: native title return "
                               "requested.\n");
                }
            }
            previous = pad.buttons;
        } else {
            previous = 0;
        }
        sceKernelDelayThread(16667);
    }
    return 0;
}

static void install_lobby_ui(void) {
    lobby_worker_running = 1;
    lobby_worker = sceKernelCreateThread("rc3_lobby_ui", lobby_worker_main,
                                         0x10000100, 0x4000, 0, 0, 0);
    if (lobby_worker < 0 ||
        sceKernelStartThread(lobby_worker, 0, 0) < 0) {
        lobby_worker_running = 0;
        if (lobby_worker >= 0) {
            sceKernelDeleteThread(lobby_worker);
            lobby_worker = -1;
        }
        append_log("Lobby UI: worker could not start.\n");
        return;
    }
    append_log("State39 controller: native LobbyGUI worker armed.\n");
}

static void release_lobby_ui(void) {
    lobby_worker_running = 0;
    if (lobby_worker >= 0) {
        sceKernelWaitThreadEnd(lobby_worker, 0, 0);
        sceKernelDeleteThread(lobby_worker);
        lobby_worker = -1;
    }
    native_lobby_destroy();
}
#endif

static void put_u16le(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

/* Encode a Thumb-2 MOVW or MOVT instruction. */
static void encode_thumb_mov(uint8_t output[4], int movt,
                             unsigned int rd, uint16_t immediate) {
    uint16_t first = (uint16_t)((movt ? 0xF2C0u : 0xF240u) |
                               ((immediate >> 12) & 0xFu) |
                               (((immediate >> 11) & 1u) << 10));
    uint16_t second = (uint16_t)((((immediate >> 8) & 7u) << 12) |
                                ((rd & 0xFu) << 8) |
                                (immediate & 0xFFu));
    put_u16le(output, first);
    put_u16le(output + 2, second);
}

static int add_patch(SceUID modid, const CodePatch *patch) {
    SceUID injection;
    const uint8_t *address = (const uint8_t *)(rc3_base + patch->offset);

    if (memcmp(address, patch->expected, patch->size) != 0)
        return -1;
    injection = taiInjectData(modid, 0, patch->offset,
                              patch->replacement, patch->size);
    if (injection < 0)
        return -1;
    injections[injection_count++] = injection;
    return 0;
}

static int add_data_patch(SceUID modid, uint32_t offset,
                          const uint8_t *expected,
                          const uint8_t *replacement, uint32_t size) {
    SceUID injection;
    const uint8_t *address = (const uint8_t *)(rc3_data_base + offset);

    if (memcmp(address, expected, size) != 0)
        return -1;
    injection = taiInjectData(modid, 1, offset, replacement, size);
    if (injection < 0)
        return -1;
    injections[injection_count++] = injection;
    return 0;
}

static int add_table_reference_patch(SceUID modid, uint32_t offset,
                                     int movt, unsigned int rd,
                                     const uint8_t expected[4],
                                     uintptr_t table_address) {
    CodePatch patch;
    patch.offset = offset;
    patch.size = 4;
    memcpy(patch.expected, expected, 4);
    encode_thumb_mov(patch.replacement, movt, rd,
                     (uint16_t)(movt ? table_address >> 16 : table_address));
    return add_patch(modid, &patch);
}

static int add_two_byte_patch(SceUID modid, uint32_t offset,
                              uint8_t old0, uint8_t old1,
                              uint8_t new0, uint8_t new1) {
    CodePatch patch;
    memset(&patch, 0, sizeof(patch));
    patch.offset = offset;
    patch.size = 2;
    patch.expected[0] = old0;
    patch.expected[1] = old1;
    patch.replacement[0] = new0;
    patch.replacement[1] = new1;
    return add_patch(modid, &patch);
}

static int add_four_byte_patch(SceUID modid, uint32_t offset,
                               const uint8_t expected[4],
                               const uint8_t replacement[4]) {
    CodePatch patch;
    patch.offset = offset;
    patch.size = 4;
    memcpy(patch.expected, expected, 4);
    memcpy(patch.replacement, replacement, 4);
    return add_patch(modid, &patch);
}

#ifdef RC3_LOBBY_TEST
static int count_patch_start = -1;

static int install_menu_count_patches(SceUID modid, int count) {
    static const uint8_t old_last[4] = {0x5F, 0xF0, 0x02, 0x0A};
    static const uint8_t old_touch[4] = {0xB8, 0xF1, 0x03, 0x0F};
    uint8_t new_last[4] = {0x5F, 0xF0, (uint8_t)(count - 1), 0x0A};
    uint8_t new_touch[4] = {0xB8, 0xF1, (uint8_t)count, 0x0F};

    count_patch_start = injection_count;
    if (add_two_byte_patch(modid, MENU_INIT_COUNT_OFFSET,
                           0x03, 0x2F, (uint8_t)count, 0x2F) < 0 ||
        add_two_byte_patch(modid, MENU_LAYOUT_COUNT_OFFSET,
                           0x03, 0x24, (uint8_t)count, 0x24) < 0 ||
        add_two_byte_patch(modid, MENU_DIVIDER_COUNT_OFFSET,
                           0x02, 0x24, (uint8_t)(count - 1), 0x24) < 0 ||
        add_four_byte_patch(modid, MENU_LAST_INDEX_OFFSET,
                            old_last, new_last) < 0 ||
        add_four_byte_patch(modid, MENU_TOUCH_COUNT_OFFSET,
                            old_touch, new_touch) < 0 ||
        add_two_byte_patch(modid, MENU_DOWN_COUNT_OFFSET,
                           0x03, 0x2A, (uint8_t)count, 0x2A) < 0 ||
        add_two_byte_patch(modid, MENU_DRAW_COUNT_OFFSET,
                           0x03, 0x2C, (uint8_t)count, 0x2C) < 0)
        return -1;
    return 0;
}

static int switch_menu_count(int count) {
    int index;
    if (count_patch_start < 0)
        return -1;
    for (index = injection_count - 1; index >= count_patch_start; --index) {
        if (injections[index] >= 0)
            taiInjectRelease(injections[index]);
        injections[index] = -1;
    }
    injection_count = count_patch_start;
    return install_menu_count_patches(rc3_modid, count);
}

static int title_update_hook(int context) {
    typedef int (*NativeInit)(void);
    typedef void (*NativeCleanup)(void);
    NativeInit native_init =
        (NativeInit)((rc3_base + MENU_NATIVE_INIT_OFFSET) | 1u);
    NativeCleanup native_cleanup =
        (NativeCleanup)((rc3_base + MENU_NATIVE_CLEANUP_OFFSET) | 1u);

    if (lobby_reopen_pending) {
        lobby_reopen_pending = 0;
        native_cleanup();
        lobby_ui_screen = LOBBY_GUI_SCREEN_MAIN;
        lobby_ui_selected = 0;
        lobby_ui_active = 1;
        lobby_native_rebuild_pending = 1;
        append_log("LobbyGUI manager: reused loaded native lobby resources.\n");
    } else if (lobby_menu_pending && lobby_screen != 1) {
        lobby_menu_pending = 0;
        native_cleanup();
        initialize_lobby_table();
        if (switch_menu_count(4) < 0) {
            append_log("Native lobby: could not switch renderer to four rows.\n");
            initialize_restored_table();
            if (switch_menu_count(5) == 0) {
                *(volatile int *)(rc3_data_base + MENU_SELECTION_DATA_OFFSET) = 2;
                native_init();
            }
            lobby_screen = 0;
            lobby_menu_active = 0;
        } else {
            *(volatile int *)(rc3_data_base + MENU_SELECTION_DATA_OFFSET) = 0;
            native_init();
            lobby_menu_active = 1;
            lobby_screen = 1;
            append_log("Native lobby: four-row title-context screen initialized.\n");
        }
    } else if (local_menu_pending) {
        local_menu_pending = 0;
        native_cleanup();
        initialize_local_players_table();
        if (switch_menu_count(5) < 0) {
            append_log("Native lobby: could not initialize Local Players screen.\n");
            initialize_lobby_table();
            if (switch_menu_count(4) == 0) {
                *(volatile int *)(rc3_data_base + MENU_SELECTION_DATA_OFFSET) = 0;
                native_init();
                lobby_screen = 1;
                lobby_menu_active = 1;
            } else {
                lobby_screen = 0;
                lobby_menu_active = 0;
            }
        } else {
            *(volatile int *)(rc3_data_base + MENU_SELECTION_DATA_OFFSET) = 0;
            native_init();
            lobby_menu_active = 1;
            lobby_screen = 2;
            append_log("Native lobby: Local Players controller screen initialized.\n");
        }
    } else if (title_menu_pending && lobby_menu_active) {
        title_menu_pending = 0;
        native_cleanup();
        initialize_restored_table();
        if (switch_menu_count(5) == 0) {
            *(volatile int *)(rc3_data_base + MENU_SELECTION_DATA_OFFSET) = 2;
            native_init();
            lobby_menu_active = 0;
            lobby_screen = 0;
            append_log("Native lobby: restored five-row title screen.\n");
        }
    }
    {
        int result;
        typedef int (*TitleUpdateFn)(int);
        struct _tai_hook_user *current =
            (struct _tai_hook_user *)title_update_ref;
        struct _tai_hook_user *next =
            (struct _tai_hook_user *)current->next;
        TitleUpdateFn function =
            (TitleUpdateFn)(next ? next->func : current->old);
        result = function(context);
        if (state39_request_pending) {
            SetLevelFn set_level =
                (SetLevelFn)((rc3_base + SET_LEVEL_OFFSET) | 1u);
            state39_request_pending = 0;
            state39_handoff_armed = 1;
            state39_update_seen = 0;
            state39_completion_seen = 0;
            state39_controller_started = 0;
            state39_activation_frames = 0;
            lobby_ui_active = 0;
            append_log("State39 diagnostic: requesting original Vita loader.\n");
            set_level(39);
            append_log("State39 diagnostic: level request returned.\n");
        }
        return result;
    }
}

static int state39_update_hook(int context) {
    typedef int (*State39UpdateFn)(int);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)state39_update_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    State39UpdateFn function =
        (State39UpdateFn)(next ? next->func : current->old);
    int result = function(context);

    if (state39_handoff_armed && !state39_update_seen) {
        state39_update_seen = 1;
        append_log("State39 diagnostic: native loader update reached.\n");
    }
    if (state39_handoff_armed && result && !state39_completion_seen) {
        append_log("State39 diagnostic: native loader signaled completion.\n");
        if (state39_arena_launch_pending) {
            state39_arena_launch_pending = 0;
            state39_handoff_armed = 0;
            state39_activation_frames = 0;
            lobby_ui_active = 0;
            lobby_scene_ready = 0;
            append_log("State39 handoff: requesting selected arena level.\n");
            request_test_arena();
        } else {
            typedef void (*ResetFrontendCallbacksFn)(void);
            ResetFrontendCallbacksFn reset_frontend_callbacks =
                (ResetFrontendCallbacksFn)
                    ((rc3_base + FRONTEND_CALLBACKS_RESET_OFFSET) | 1u);

            reset_frontend_callbacks();
            state39_activation_frames = 30;
            lobby_ui_screen = LOBBY_GUI_SCREEN_MAIN;
            lobby_ui_selected = 0;
            lobby_scene_ready = 1;
            append_log("LobbyGUI manager: retained frontend callbacks reset "
                       "after state-39 completion.\n");
            append_log("LobbyGUI manager: scheduling screen 0x18 over native "
                       "state-16 lobby audio.\n");
        }
        __sync_synchronize();
        state39_completion_seen = 1;
    }
    if (state39_handoff_armed && state39_completion_seen &&
        state39_arena_launch_pending) {
        state39_arena_launch_pending = 0;
        state39_handoff_armed = 0;
        state39_activation_frames = 0;
        lobby_ui_active = 0;
        append_log("State39 handoff: requesting selected arena at update boundary.\n");
        request_test_arena();
    }
    /* Returning completion is required to dismiss Vita's loading renderer.
     * The PS3 immediately creates its removed LobbyGUI owner at this boundary;
     * our replacement is activated just after the retail callback returns. */
    return result;
}

static int state16_update_hook(int context) {
    typedef int (*State16UpdateFn)(int);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)state16_update_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    State16UpdateFn function =
        (State16UpdateFn)(next ? next->func : current->old);
    int result = function(context);

    native_lobby_tick();
    return result;
}

static int native_ui_frame_owner_hook(int active) {
    typedef int (*NativeUiFrameOwnerFn)(int active);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)native_ui_frame_owner_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    NativeUiFrameOwnerFn function =
        (NativeUiFrameOwnerFn)(next ? next->func : current->old);
    int result = function(active);

    native_lobby_tick();
    return result;
}

static void general_game_init_hook(void) {
    typedef void (*GeneralGameInitFn)(void);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)general_game_init_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    GeneralGameInitFn function =
        (GeneralGameInitFn)(next ? next->func : current->old);
    uintptr_t context;
    uintptr_t hero;
    volatile int *presentation =
        (volatile int *)(rc3_base + 0x00B539F4u);
    volatile int *presentation_pending =
        (volatile int *)(rc3_base + PRESENTATION_PENDING_OFFSET);
    char line[144];

    append_log("Arena diagnostic: retained general game initializer entered.\n");
    function();
    append_log("Arena diagnostic: retained general game initializer returned.\n");
    context = *(volatile uintptr_t *)(rc3_data_base +
                                      MAIN_CONTEXT_PTR_DATA_OFFSET);
    hero = *(volatile uintptr_t *)(rc3_base + 0x009D4DC0u);
    if (*(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET) ==
            arena_target_level &&
        context && *(volatile uint8_t *)(context + 0xC8u) &&
        *(volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET) == 1 &&
        hero) {
        int previous = *presentation;
        int previous_pending = *presentation_pending;
        *presentation_pending = 0;
        __sync_synchronize();
        snprintf(line, sizeof(line),
                 "Arena owner: queued presentation handoff current=%d "
                 "pending=%d->0 after retained game initialization.\n",
                 previous, previous_pending);
        append_log(line);
    }
}

static void general_game_update_hook(int view) {
    typedef void (*GeneralGameUpdateFn)(int view);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)general_game_update_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    GeneralGameUpdateFn function =
        (GeneralGameUpdateFn)(next ? next->func : current->old);
    int verbose = arena_game_update_calls < 6;
    uintptr_t slot = rc3_base + 0x009D4B80u;
    uintptr_t hero = *(volatile uintptr_t *)(slot + 0x240u);
    char line[224];

    if (verbose) {
        snprintf(line, sizeof(line),
                 "Arena game update: enter call=%d view=%d views=%d "
                 "scheduler=%08X game_state=%d hero=%08X "
                 "pos=%08X,%08X,%08X.\n",
                 arena_game_update_calls, view,
                 *(volatile int *)(rc3_base + 0x0081558Cu),
                 (unsigned int)*(volatile uintptr_t *)(rc3_base + 0x009A2084u),
                 *(volatile int *)(rc3_base + 0x00B539F4u),
                 (unsigned int)hero,
                 hero ? (unsigned int)*(volatile uint32_t *)(hero + 0x10u) : 0u,
                 hero ? (unsigned int)*(volatile uint32_t *)(hero + 0x14u) : 0u,
                 hero ? (unsigned int)*(volatile uint32_t *)(hero + 0x18u) : 0u);
        append_log(line);
    }
    function(view);
    if (view == 0)
        sync_arena_player_to_hero();
    if (verbose)
        append_log("Arena game update: returned.\n");
    ++arena_game_update_calls;
}

static void log_player_controller_state(const char *phase, uintptr_t record,
                                        uintptr_t player_state) {
    uintptr_t slot = rc3_base + 0x009D4B80u;
    char line[320];

    snprintf(line, sizeof(line),
             "Arena controller %s call=%d record=%08X mode=%08X type=%08X "
             "rec30=%08X,%08X,%08X slot190=%08X,%08X,%08X "
             "state60=%08X,%08X,%08X state70=%08X,%08X,%08X "
             "state80=%08X,%08X,%08X state90=%08X,%08X,%08X.\n",
             phase, player_controller_update_calls, (unsigned int)record,
             (unsigned int)*(volatile uint32_t *)(record + 0x88u),
             (unsigned int)*(volatile uint32_t *)(record + 0x8Cu),
             (unsigned int)*(volatile uint32_t *)(record + 0x30u),
             (unsigned int)*(volatile uint32_t *)(record + 0x34u),
             (unsigned int)*(volatile uint32_t *)(record + 0x38u),
             (unsigned int)*(volatile uint32_t *)(slot + 0x190u),
             (unsigned int)*(volatile uint32_t *)(slot + 0x194u),
             (unsigned int)*(volatile uint32_t *)(slot + 0x198u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x60u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x64u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x68u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x70u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x74u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x78u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x80u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x84u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x88u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x90u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x94u),
             (unsigned int)*(volatile uint32_t *)(player_state + 0x98u));
    append_log(line);
}

static void player_controller_update_hook(uintptr_t record) {
    typedef void (*PlayerControllerUpdateFn)(uintptr_t record);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)player_controller_update_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    PlayerControllerUpdateFn function =
        (PlayerControllerUpdateFn)(next ? next->func : current->old);
    uintptr_t player_state =
        record ? *(volatile uintptr_t *)(record + 0x70u) : 0;
    int verbose = player_controller_update_calls < 4;

    if (verbose && player_state)
        log_player_controller_state("enter", record, player_state);
    function(record);
    if (player_state &&
        record == rc3_base + 0x009D6710u &&
        *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET) ==
            arena_target_level) {
        arena_controller_position[0] =
            *(volatile float *)(player_state + 0x60u);
        arena_controller_position[1] =
            *(volatile float *)(player_state + 0x64u);
        arena_controller_position[2] =
            *(volatile float *)(player_state + 0x68u);
        arena_controller_position[3] =
            *(volatile float *)(player_state + 0x6Cu);
        arena_controller_position_valid = 1;
    }
    if (verbose && player_state)
        log_player_controller_state("exit", record, player_state);
    ++player_controller_update_calls;
}

static void gameplay_frame_owner_hook(void) {
    typedef void (*GameplayFrameOwnerFn)(void);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)gameplay_frame_owner_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    GameplayFrameOwnerFn function =
        (GameplayFrameOwnerFn)(next ? next->func : current->old);
    int verbose = arena_frame_owner_calls < 6;
    char line[256];

    if (verbose) {
        uintptr_t context = *(volatile uintptr_t *)(rc3_data_base +
                                                    MAIN_CONTEXT_PTR_DATA_OFFSET);
        snprintf(line, sizeof(line),
                 "Arena frame owner: enter call=%d level=%d mp=%d "
                 "dispatch=%d presentation=%d busy=%u mode=%u views=%d.\n",
                 arena_frame_owner_calls,
                 *(volatile int *)(rc3_base + CURRENT_LEVEL_OFFSET),
                 context ? *(volatile uint8_t *)(context + 0xC8u) : -1,
                 *(volatile int *)(rc3_base + 0x00B539FCu),
                 *(volatile int *)(rc3_base + 0x00B539F4u),
                 (unsigned int)*(volatile uint8_t *)(rc3_base + 0x00B59409u),
                 (unsigned int)*(volatile uint8_t *)(rc3_base + 0x00A0F494u),
                 *(volatile int *)(rc3_base + 0x0081558Cu));
        append_log(line);
    }
    function();
    if (verbose)
        append_log("Arena frame owner: returned.\n");
    ++arena_frame_owner_calls;
}

static void common_level_loader_hook(void) {
    typedef void (*CommonLevelLoaderFn)(void);
    typedef uintptr_t (*GetMultiplayerStateFn)(void);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)common_level_loader_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    CommonLevelLoaderFn function =
        (CommonLevelLoaderFn)(next ? next->func : current->old);
    volatile uintptr_t *context_slot =
        (volatile uintptr_t *)(rc3_data_base + MAIN_CONTEXT_PTR_DATA_OFFSET);
    GetMultiplayerStateFn get_multiplayer_state =
        (GetMultiplayerStateFn)((rc3_base +
                                 MULTIPLAYER_STATE_GETTER_OFFSET) | 1u);
    uintptr_t multiplayer_state = get_multiplayer_state();
    volatile int *player_count =
        (volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET);
    volatile uintptr_t *player_list =
        (volatile uintptr_t *)(rc3_base + LOCAL_PLAYER_LIST_OFFSET);
    volatile uintptr_t *profile_source =
        (volatile uintptr_t *)(rc3_base + LOCAL_PROFILE_OFFSET);
    RegisterLocalPlayerFn register_local_player =
        (RegisterLocalPlayerFn)((rc3_base + REGISTER_LOCAL_PLAYER_OFFSET) | 1u);
    char line[224];
    unsigned int mode_c8 = 0;
    unsigned int mode_28c = 0;
    int roster_result = 0;

    if (*context_slot)
        mode_c8 = *(volatile uint8_t *)(*context_slot + 0xC8u);
    if (multiplayer_state)
        mode_28c = *(volatile uint8_t *)(multiplayer_state + 0x28Cu);
    if (arena_roster_repair_active && mode_c8 && mode_28c &&
        *player_count == 0) {
        roster_result = register_local_player(0, 1, 0, 0, 0);
        snprintf(line, sizeof(line),
                 "Arena roster repair: loader-boundary register=%d count=%d source=%08lX list0=%08lX.\n",
                 roster_result, *player_count,
                 (unsigned long)*profile_source,
                 (unsigned long)player_list[0]);
        append_log(line);
    }
    snprintf(line, sizeof(line),
             "Arena diagnostic: common loader entered; mode=%u/%u local_players=%d.\n",
             mode_c8, mode_28c, *player_count);
    append_log(line);
    function();
    append_log("Arena diagnostic: common loader returned.\n");
}

/* This is the asynchronous transition routine selected after SetLevel writes
 * the pending level globals.  It runs before the common level loader, so the
 * snapshot distinguishes a bad request/context from a later resource-owner
 * failure without changing the retail transition sequence. */
static void arena_transition_hook(void) {
    typedef void (*ArenaTransitionFn)(void);
    typedef uintptr_t (*GetMultiplayerStateFn)(void);
    struct _tai_hook_user *current =
        (struct _tai_hook_user *)arena_transition_ref;
    struct _tai_hook_user *next =
        (struct _tai_hook_user *)current->next;
    ArenaTransitionFn function =
        (ArenaTransitionFn)(next ? next->func : current->old);
    volatile int *transition = (volatile int *)(rc3_data_base +
                                                FRONTEND_TRANSITION_DATA_OFFSET);
    volatile int *request = (volatile int *)(rc3_data_base +
                                             FRONTEND_REQUEST_DATA_OFFSET);
    volatile uintptr_t *context_slot =
        (volatile uintptr_t *)(rc3_data_base + MAIN_CONTEXT_PTR_DATA_OFFSET);
    GetMultiplayerStateFn get_multiplayer_state =
        (GetMultiplayerStateFn)((rc3_base +
                                 MULTIPLAYER_STATE_GETTER_OFFSET) | 1u);
    uintptr_t multiplayer_state = get_multiplayer_state();
    volatile int *player_count =
        (volatile int *)(rc3_base + LOCAL_PLAYER_COUNT_OFFSET);
    volatile uintptr_t *owner_slots =
        (volatile uintptr_t *)(rc3_data_base +
                               TRANSITION_OWNER_SLOTS_DATA_OFFSET);
    char line[192];
    unsigned int mode_c8 = 0;
    unsigned int mode_28c = 0;

    if (*context_slot)
        mode_c8 = *(volatile uint8_t *)(*context_slot + 0xC8u);
    if (multiplayer_state)
        mode_28c = *(volatile uint8_t *)(multiplayer_state + 0x28Cu);
    snprintf(line, sizeof(line),
             "Arena diagnostic: transition entered; pending=%d request=%d mode=%u/%u local_players=%d.\n",
             *transition, *request, mode_c8, mode_28c, *player_count);
    append_log(line);
    snprintf(line, sizeof(line),
             "Arena diagnostic: owner slots=%08lX/%08lX/%08lX/%08lX/%08lX/%08lX.\n",
             (unsigned long)owner_slots[0], (unsigned long)owner_slots[1],
             (unsigned long)owner_slots[2], (unsigned long)owner_slots[3],
             (unsigned long)owner_slots[4], (unsigned long)owner_slots[5]);
    append_log(line);
    function();
    append_log("Arena diagnostic: transition returned.\n");
}
#endif

static void initialize_restored_table(void) {
    restored_menu[0].widget_id = 0x0C;
    restored_menu[0].group_id = 0x0D;
    restored_menu[0].text_id = TEXT_LOAD_GAME;
    restored_menu[0].enabled = 1;
    restored_menu[0].callback =
        (MenuCallback)((rc3_base + CALLBACK_LOAD_GAME_OFFSET) | 1u);

    restored_menu[1].widget_id = 0x0D;
    restored_menu[1].group_id = 0x0D;
    restored_menu[1].text_id = TEXT_NEW_GAME;
    restored_menu[1].enabled = 1;
    restored_menu[1].callback =
        (MenuCallback)((rc3_base + CALLBACK_NEW_GAME_OFFSET) | 1u);

    restored_menu[2].widget_id = 0x0E;
    restored_menu[2].group_id = 0x0D;
    restored_menu[2].text_id = TEXT_MULTIPLAYER;
    restored_menu[2].enabled = 1;
    restored_menu[2].callback = multiplayer_callback;

    restored_menu[3].widget_id = 0x0F;
    restored_menu[3].group_id = 0x0D;
    restored_menu[3].text_id = TEXT_MULTIPLAYER_TUTORIAL;
    restored_menu[3].enabled = 1;
    restored_menu[3].callback = multiplayer_tutorial_callback;

    restored_menu[4].widget_id = 0x10;
    restored_menu[4].group_id = 0x0D;
    restored_menu[4].text_id = TEXT_OPTIONS;
    restored_menu[4].enabled = 1;
    restored_menu[4].callback =
        (MenuCallback)((rc3_base + CALLBACK_OPTIONS_OFFSET) | 1u);
}

static int install_native_menu(SceUID modid) {
    static const uint8_t ref0w[4] = {0x47, 0xF2, 0xA8, 0x75};
    static const uint8_t ref0t[4] = {0xC8, 0xF2, 0x84, 0x15};
    static const uint8_t ref1w[4] = {0x47, 0xF2, 0xA8, 0x71};
    static const uint8_t ref1t[4] = {0xC8, 0xF2, 0x84, 0x11};
    static const uint8_t ref2w[4] = {0x47, 0xF2, 0xA8, 0x72};
    static const uint8_t ref2t[4] = {0xC8, 0xF2, 0x84, 0x12};
    static const uint8_t ref3w[4] = {0x47, 0xF2, 0xA8, 0x70};
    static const uint8_t ref3t[4] = {0xC8, 0xF2, 0x84, 0x10};
    static const uint8_t ref4w[4] = {0x47, 0xF2, 0xA8, 0x76};
    static const uint8_t ref4t[4] = {0xC8, 0xF2, 0x84, 0x16};
#ifndef RC3_LOBBY_TEST
    static const uint8_t old_last[4] = {0x5F, 0xF0, 0x02, 0x0A};
    static const uint8_t new_last[4] = {0x5F, 0xF0, 0x04, 0x0A};
    static const uint8_t old_touch[4] = {0xB8, 0xF1, 0x03, 0x0F};
    static const uint8_t new_touch[4] = {0xB8, 0xF1, 0x05, 0x0F};
#endif
    /* Vita scales the retail frame vertically to 0.47 for three rows.  The
     * PS3 five-row initializer uses 1.0 at this exact point. */
    static const uint8_t old_frame_w[4] = {0x4A, 0xF2, 0xD7, 0x30};
    static const uint8_t new_frame_w[4] = {0x40, 0xF2, 0x00, 0x00};
    static const uint8_t old_frame_t[4] = {0xC3, 0xF6, 0xF0, 0x60};
    static const uint8_t new_frame_t[4] = {0xC3, 0xF6, 0x80, 0x70};
    static const uint8_t old_ornament_y[8] = {
        0xA4, 0x70, 0xBD, 0x3E, /* 0.370 */
        0x86, 0xEB, 0x21, 0x3F  /* 0.6325 */
    };
    static const uint8_t new_ornament_y[8] = {
        0x66, 0x66, 0x66, 0x3E, /* PS3: 0.225 */
        0x9A, 0x99, 0x49, 0x3F  /* PS3: 0.7875 */
    };
    static const uint8_t old_divider_y[8] = {
        0x05, 0x34, 0xF1, 0x3E, /* Vita: 0.4711 */
        0x27, 0x31, 0x08, 0x3F  /* Vita: 0.5320 */
    };
    static const uint8_t new_divider_y[8] = {
        0xE6, 0xAE, 0xC5, 0x3E, /* PS3: 0.3861 */
        0x2F, 0xDD, 0xE4, 0x3E  /* PS3: 0.4470 */
    };
    static const uint8_t old_selector_y[12] = {
        0xAE, 0x47, 0xE1, 0x3E, /* Vita: 0.440 */
        0x12, 0x83, 0x00, 0x3F, /* Vita: 0.502 */
        0x3B, 0xDF, 0x0F, 0x3F  /* Vita: 0.562 */
    };
    static const uint8_t new_selector_y[12] = {
        0x8F, 0xC2, 0xB5, 0x3E, /* PS3: 0.355 */
        0x06, 0x81, 0xD5, 0x3E, /* PS3: 0.417 */
        0x58, 0x39, 0xF4, 0x3E  /* PS3: 0.477 */
    };
    static const uint8_t old_text_pad_w[4] = {0x41, 0xF2, 0x7B, 0x41};
    static const uint8_t new_text_pad_w[4] = {0x40, 0xF2, 0x00, 0x01};
    static const uint8_t old_text_pad_t[4] = {0xC3, 0xF6, 0xAE, 0x51};
    static const uint8_t new_text_pad_t[4] = {0xC0, 0xF2, 0x00, 0x01};
    uintptr_t table_address = (uintptr_t)restored_menu;

#ifdef RC3_LOBBY_TEST
    /* The Vita port retained the PS3 lobby runner, but its two mode stores
     * were changed from multiplayer (1) to single-player (0).  Use the
     * already-live r11 value (1) in place of r10 (0), matching the PS3
     * function without changing either register for the rest of the runner. */
    static const uint8_t old_mode_store_0[4] = {0x80, 0xF8, 0xC8, 0xA0};
    static const uint8_t new_mode_store_0[4] = {0x80, 0xF8, 0xC8, 0xB0};
    static const uint8_t old_mode_store_1[4] = {0x80, 0xF8, 0x8C, 0xA2};
    static const uint8_t new_mode_store_1[4] = {0x80, 0xF8, 0x8C, 0xB2};

    if (add_four_byte_patch(modid, MULTIPLAYER_MODE_STORE_0_OFFSET,
                            old_mode_store_0, new_mode_store_0) < 0 ||
        add_four_byte_patch(modid, MULTIPLAYER_MODE_STORE_1_OFFSET,
                            old_mode_store_1, new_mode_store_1) < 0)
        return -1;

#endif

    if (add_data_patch(modid, FRAME_ORNAMENT_Y_DATA_OFFSET,
                       old_ornament_y, new_ornament_y,
                       sizeof(old_ornament_y)) < 0 ||
        add_data_patch(modid, DIVIDER_Y_DATA_OFFSET,
                       old_divider_y, new_divider_y,
                       sizeof(old_divider_y)) < 0 ||
        add_data_patch(modid, SELECTOR_Y_DATA_OFFSET,
                       old_selector_y, new_selector_y,
                       sizeof(old_selector_y)) < 0 ||
        add_four_byte_patch(modid, MENU_TEXT_Y_PAD_MOVW_OFFSET,
                            old_text_pad_w, new_text_pad_w) < 0 ||
        add_four_byte_patch(modid, MENU_TEXT_Y_PAD_MOVT_OFFSET,
                            old_text_pad_t, new_text_pad_t) < 0)
        return -1;

    if (add_four_byte_patch(modid, MENU_FRAME_SCALE_MOVW_OFFSET,
                            old_frame_w, new_frame_w) < 0 ||
        add_four_byte_patch(modid, MENU_FRAME_SCALE_MOVT_OFFSET,
                            old_frame_t, new_frame_t) < 0)
        return -1;

    if (add_table_reference_patch(modid, MENU_TABLE_REF_0_MOVW, 0, 5,
                                  ref0w, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_0_MOVT, 1, 5,
                                  ref0t, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_1_MOVW, 0, 1,
                                  ref1w, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_1_MOVT, 1, 1,
                                  ref1t, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_2_MOVW, 0, 2,
                                  ref2w, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_2_MOVT, 1, 2,
                                  ref2t, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_3_MOVW, 0, 0,
                                  ref3w, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_3_MOVT, 1, 0,
                                  ref3t, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_4_MOVW, 0, 6,
                                  ref4w, table_address) < 0 ||
        add_table_reference_patch(modid, MENU_TABLE_REF_4_MOVT, 1, 6,
                                  ref4t, table_address) < 0)
        return -1;

    #ifndef RC3_LOBBY_TEST
    if (add_two_byte_patch(modid, MENU_INIT_COUNT_OFFSET,
                           0x03, 0x2F, 0x05, 0x2F) < 0 ||
        add_two_byte_patch(modid, MENU_LAYOUT_COUNT_OFFSET,
                           0x03, 0x24, 0x05, 0x24) < 0 ||
        add_two_byte_patch(modid, MENU_DIVIDER_COUNT_OFFSET,
                           0x02, 0x24, 0x04, 0x24) < 0 ||
        add_four_byte_patch(modid, MENU_LAST_INDEX_OFFSET,
                            old_last, new_last) < 0 ||
        add_four_byte_patch(modid, MENU_TOUCH_COUNT_OFFSET,
                            old_touch, new_touch) < 0 ||
        add_two_byte_patch(modid, MENU_DOWN_COUNT_OFFSET,
                           0x03, 0x2A, 0x05, 0x2A) < 0 ||
        add_two_byte_patch(modid, MENU_DRAW_COUNT_OFFSET,
                           0x03, 0x2C, 0x05, 0x2C) < 0)
        return -1;
    #else
    if (install_menu_count_patches(modid, 5) < 0)
        return -1;
    #endif

    return 0;
}

static void release_all(void) {
    while (injection_count > 0) {
        --injection_count;
        if (injections[injection_count] >= 0)
            taiInjectRelease(injections[injection_count]);
        injections[injection_count] = -1;
    }
}

int module_start(SceSize argc, const void *args) {
    tai_module_info_t tai_info;
    SceKernelModuleInfo kernel_info;
    const uint8_t *original_table;
    SceUID modid;
    int i;
    (void)argc;
    (void)args;

    for (i = 0; i < (int)(sizeof(injections) / sizeof(injections[0])); ++i)
        injections[i] = -1;

    memset(&tai_info, 0, sizeof(tai_info));
    tai_info.size = sizeof(tai_info);
    if (taiGetModuleInfo(TAI_MAIN_MODULE, &tai_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    modid = tai_info.modid;
#ifdef RC3_LOBBY_TEST
    rc3_modid = modid;
#endif
    memset(&kernel_info, 0, sizeof(kernel_info));
    kernel_info.size = sizeof(kernel_info);
    if (sceKernelGetModuleInfo(modid, &kernel_info) < 0)
        return SCE_KERNEL_START_SUCCESS;

    if (!kernel_info.segments[0].vaddr || !kernel_info.segments[1].vaddr ||
        kernel_info.segments[0].filesz != RC3_TEXT_FILESZ ||
        kernel_info.segments[0].memsz != RC3_TEXT_MEMSZ ||
        kernel_info.segments[1].filesz != RC3_DATA_FILESZ ||
        kernel_info.segments[1].memsz != RC3_DATA_MEMSZ)
        return SCE_KERNEL_START_SUCCESS;

    rc3_base = (uintptr_t)kernel_info.segments[0].vaddr;
    rc3_data_base = (uintptr_t)kernel_info.segments[1].vaddr;
    original_table = (const uint8_t *)kernel_info.segments[1].vaddr +
                     ORIGINAL_TABLE_DATA_OFFSET;
    if (memcmp(original_table, expected_original_table,
               sizeof(expected_original_table)) != 0) {
        append_log("Native menu: original Vita table validation failed.\n");
        return SCE_KERNEL_START_SUCCESS;
    }

    initialize_restored_table();
    if (install_native_menu(modid) < 0) {
        append_log("Native menu: code validation or injection failed.\n");
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }

#ifdef RC3_LOBBY_TEST
    title_update_hook_uid = taiHookFunctionOffset(
        &title_update_ref, modid, 0, MENU_NATIVE_UPDATE_OFFSET, 1,
        title_update_hook);
    if (title_update_hook_uid < 0) {
        append_log("Native lobby: title update hook failed.\n");
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    state39_update_hook_uid = taiHookFunctionOffset(
        &state39_update_ref, modid, 0, STATE39_NATIVE_UPDATE_OFFSET, 1,
        state39_update_hook);
    if (state39_update_hook_uid < 0) {
        append_log("State39 diagnostic: native update hook failed.\n");
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    state16_update_hook_uid = taiHookFunctionOffset(
        &state16_update_ref, modid, 0, STATE16_NATIVE_UPDATE_OFFSET, 1,
        state16_update_hook);
    if (state16_update_hook_uid < 0) {
        append_log("Native LobbyGUI: state-16 update hook failed.\n");
        taiHookRelease(state39_update_hook_uid, state39_update_ref);
        state39_update_hook_uid = -1;
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    native_ui_frame_owner_hook_uid = taiHookFunctionOffset(
        &native_ui_frame_owner_ref, modid, 0,
        NATIVE_UI_FRAME_OWNER_OFFSET, 1, native_ui_frame_owner_hook);
    if (native_ui_frame_owner_hook_uid < 0) {
        append_log("Native LobbyGUI: global UI frame-owner hook failed.\n");
        taiHookRelease(state16_update_hook_uid, state16_update_ref);
        state16_update_hook_uid = -1;
        taiHookRelease(state39_update_hook_uid, state39_update_ref);
        state39_update_hook_uid = -1;
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    general_game_init_hook_uid = taiHookFunctionOffset(
        &general_game_init_ref, modid, 0, GENERAL_GAME_INIT_OFFSET, 1,
        general_game_init_hook);
    if (general_game_init_hook_uid < 0) {
        append_log("Arena diagnostic: general initializer hook failed.\n");
        taiHookRelease(native_ui_frame_owner_hook_uid,
                       native_ui_frame_owner_ref);
        native_ui_frame_owner_hook_uid = -1;
        taiHookRelease(state16_update_hook_uid, state16_update_ref);
        state16_update_hook_uid = -1;
        taiHookRelease(state39_update_hook_uid, state39_update_ref);
        state39_update_hook_uid = -1;
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    common_level_loader_hook_uid = taiHookFunctionOffset(
        &common_level_loader_ref, modid, 0, COMMON_LEVEL_LOADER_OFFSET, 1,
        common_level_loader_hook);
    if (common_level_loader_hook_uid < 0) {
        append_log("Arena diagnostic: common loader hook failed.\n");
        taiHookRelease(general_game_init_hook_uid, general_game_init_ref);
        general_game_init_hook_uid = -1;
        taiHookRelease(native_ui_frame_owner_hook_uid,
                       native_ui_frame_owner_ref);
        native_ui_frame_owner_hook_uid = -1;
        taiHookRelease(state16_update_hook_uid, state16_update_ref);
        state16_update_hook_uid = -1;
        taiHookRelease(state39_update_hook_uid, state39_update_ref);
        state39_update_hook_uid = -1;
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
        release_all();
        return SCE_KERNEL_START_SUCCESS;
    }
    install_slot_probes();
    install_lobby_ui();
#endif

    append_log("Native five-entry RC3 retail menu installed.\n");
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;
#ifdef RC3_LOBBY_TEST
    release_lobby_ui();
    release_slot_probes();
    release_arena_stage_hooks();
    if (allocator_list_next_hook_uid >= 0) {
        taiHookRelease(allocator_list_next_hook_uid,
                       allocator_list_next_ref);
        allocator_list_next_hook_uid = -1;
    }
    if (player_position_setup_hook_uid >= 0) {
        taiHookRelease(player_position_setup_hook_uid,
                       player_position_setup_ref);
        player_position_setup_hook_uid = -1;
    }
    if (player_object_callback_hook_uid >= 0) {
        taiHookRelease(player_object_callback_hook_uid,
                       player_object_callback_ref);
        player_object_callback_hook_uid = -1;
    }
    if (reset_local_session_hook_uid >= 0) {
        taiHookRelease(reset_local_session_hook_uid,
                       reset_local_session_ref);
        reset_local_session_hook_uid = -1;
    }
    if (arena_transition_hook_uid >= 0) {
        taiHookRelease(arena_transition_hook_uid, arena_transition_ref);
        arena_transition_hook_uid = -1;
    }
    if (common_level_loader_hook_uid >= 0) {
        taiHookRelease(common_level_loader_hook_uid,
                       common_level_loader_ref);
        common_level_loader_hook_uid = -1;
    }
    if (general_game_update_hook_uid >= 0) {
        taiHookRelease(general_game_update_hook_uid,
                       general_game_update_ref);
        general_game_update_hook_uid = -1;
    }
    if (player_controller_update_hook_uid >= 0) {
        taiHookRelease(player_controller_update_hook_uid,
                       player_controller_update_ref);
        player_controller_update_hook_uid = -1;
    }
    if (gameplay_frame_owner_hook_uid >= 0) {
        taiHookRelease(gameplay_frame_owner_hook_uid,
                       gameplay_frame_owner_ref);
        gameplay_frame_owner_hook_uid = -1;
    }
    if (moby_draw_builder_hook_uid >= 0) {
        taiHookRelease(moby_draw_builder_hook_uid,
                       moby_draw_builder_ref);
        moby_draw_builder_hook_uid = -1;
    }
    if (general_game_init_hook_uid >= 0) {
        taiHookRelease(general_game_init_hook_uid, general_game_init_ref);
        general_game_init_hook_uid = -1;
    }
    if (state39_update_hook_uid >= 0) {
        taiHookRelease(state39_update_hook_uid, state39_update_ref);
        state39_update_hook_uid = -1;
    }
    if (state16_update_hook_uid >= 0) {
        taiHookRelease(state16_update_hook_uid, state16_update_ref);
        state16_update_hook_uid = -1;
    }
    if (native_ui_frame_owner_hook_uid >= 0) {
        taiHookRelease(native_ui_frame_owner_hook_uid,
                       native_ui_frame_owner_ref);
        native_ui_frame_owner_hook_uid = -1;
    }
    if (title_update_hook_uid >= 0) {
        taiHookRelease(title_update_hook_uid, title_update_ref);
        title_update_hook_uid = -1;
    }
#endif
    release_all();
    return SCE_KERNEL_STOP_SUCCESS;
}
