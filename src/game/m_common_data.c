#include "m_common_data.h"

#include "libultra/libultra.h"
#ifdef TARGET_PC
#include <string.h>
#endif

common_data_t common_data;


extern void common_data_reinit(){

    u8 state;
#ifdef TARGET_PC
    /* On PC, save data lives in common_data.save (loaded from disk).
     * Preserve it across reinit — on GC, saves live on the memory card
     * and get loaded later, but on PC we loaded it at boot. */
    extern int pc_save_loaded;
    static Save save_backup; /* static: Save is ~152KB, too large for stack */
    int had_save = pc_save_loaded;
    if (had_save) {
        memcpy(&save_backup, &common_data.save, sizeof(Save));
    }
#endif

    state = Common_Get(pad_connected);

    bzero(&common_data, sizeof(common_data));
    Common_Set(transition.wipe_type, -1);
    Common_Set(game_started,1);
    Common_Set(last_scene_no, -1);
    Common_Set(demo_profiles[0], mAc_PROFILE_NUM); /* cleared state */
    Common_Set(demo_profiles[1], mAc_PROFILE_NUM); /* cleared state */
    Common_Set(pad_connected, state);

#ifdef TARGET_PC
    if (had_save) {
        memcpy(&common_data.save, &save_backup, sizeof(Save));
    } else
#endif
    mFRm_ClearSaveCheckData(Save_GetPointer(save_check));
}

extern void common_data_init(){
    common_data_reinit();
}

extern void common_data_clear(){
    clip_clear();
}
