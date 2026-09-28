/* Exercise the actual eligibility helper against real game state/enum types. */
#include <stdio.h>
#include <string.h>
#include "m_player.h"
#include "m_play.h"

#undef mEv_IsTitleDemo
static int title_demo;
static int mEv_IsTitleDemo(void) { return title_demo; }
#include "player_source.inc"

static PLAYER_ACTOR player;
static GAME_PLAY play;
static int checks, failures;
#define CHECK(test, label) do { ++checks; if (!(test)) { ++failures; \
    printf("FAIL %s (line %d)\n", label, __LINE__); } } while (0)

static void reset(void) {
    memset(&player, 0, sizeof(player));
    memset(&play, 0, sizeof(play));
    player.now_main_index = mPlayer_INDEX_WAIT;
    player.item_kind = mPlayer_ITEM_KIND_NONE;
    player.now_item_main_index = mPlayer_ITEM_MAIN_NONE;
    title_demo = 0;
}

static int visible(void) { return Player_actor_empty_hands_available(&player, &play); }

int main(void) {
    static const int ordinary[] = { mPlayer_INDEX_WAIT, mPlayer_INDEX_WALK, mPlayer_INDEX_RUN,
        mPlayer_INDEX_DASH, mPlayer_INDEX_TURN_DASH, mPlayer_INDEX_WADE };
    reset();
    CHECK(visible(), "ordinary empty-handed player can show floating hands");
    for (int state = -2; state <= mPlayer_INDEX_NUM + 1; ++state) {
        int expected = 0;
        for (unsigned i = 0; i < sizeof(ordinary)/sizeof(ordinary[0]); ++i)
            expected |= state == ordinary[i];
        reset();
        player.now_main_index = state;
        CHECK(visible() == expected, "all main states: only ordinary movement permits hands");
        reset();
        player.requested_main_index_changed = 1;
        player.requested_main_index = state;
        CHECK(visible() == expected, "pending item/interaction/demo/invalid actions hide hands immediately");
        player.requested_main_index_changed = 0;
        CHECK(visible(), "old applied request cannot keep empty hands hidden");
    }
    for (unsigned i = 0; i < sizeof(ordinary)/sizeof(ordinary[0]); ++i) {
        for (int item = -2; item <= mPlayer_ITEM_MAIN_NUM + 1; ++item) {
            reset();
            player.now_main_index = ordinary[i];
            player.now_item_main_index = item;
            CHECK(visible() == (item == mPlayer_ITEM_MAIN_NONE),
                  "every tool/item state (including putaway and invalid) suppresses floating hands");
        }
        for (int kind = -2; kind <= mPlayer_ITEM_KIND_NUM + 1; ++kind) {
            reset();
            player.now_main_index = ordinary[i];
            player.item_kind = kind;
            CHECK(visible() == (kind == mPlayer_ITEM_KIND_NONE),
                  "equipped item kind suppresses hands even with item-main NONE");
        }
        reset();
        player.now_main_index = ordinary[i];
        play.submenu.start_refuse = 1;
        CHECK(visible(), "Start refusal alone does not hide hands during ordinary turns/acres");
    }
    for (int menu = -1; menu <= mSM_OVL_NUM; ++menu) {
        reset();
        play.submenu.menu_type = menu;
        CHECK(visible() == (menu == mSM_OVL_NONE), "opening/requested menu prevents hands");
        play.submenu.menu_type = mSM_OVL_NONE;
        play.submenu.current_menu_type = menu;
        CHECK(visible() == (menu == mSM_OVL_NONE), "active menu prevents hands");
    }
    reset(); title_demo = 1;
    CHECK(!visible(), "title attract-mode actor cannot enable hands");
    reset(); player.now_main_index = mPlayer_INDEX_WASH_CAR;
    CHECK(!visible(), "NONE tool state while holding wash-car sponge is not empty");
    reset(); player.now_main_index = mPlayer_INDEX_PICKUP;
    CHECK(!visible(), "separately drawn pickup item cannot overlap floating hands");
    reset(); player.now_main_index = mPlayer_INDEX_PICKUP_JUMP;
    CHECK(!visible(), "separately drawn pickup-jump item cannot overlap floating hands");
    reset(); player.now_main_index = mPlayer_INDEX_GET_SCOOP;
    CHECK(!visible(), "fossil/find presentation item cannot overlap floating hands");
    reset(); player.now_main_index = mPlayer_INDEX_PUTAWAY_SCOOP;
    CHECK(!visible(), "find putaway presentation item cannot overlap floating hands");
    printf("Empty-hand player eligibility: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
