#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "z64interface.h"

#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
#include "z64rumble.h"
#include "overlays/actors/ovl_Eff_Dust/z_eff_dust.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

#define QUICKSPIN_MAGIC_USAGE 4

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define ENMTHUNDER_TYPE_MAX 4

extern ActorProfile En_M_Thunder_Profile;

// // The originals are never written to, so a duplicating is fine for now:
// static ColliderCylinderInit sCylinderInit = {
//     {
//         COL_MATERIAL_NONE,
//         AT_ON | AT_TYPE_PLAYER,
//         AC_NONE,
//         OC1_NONE,
//         OC2_TYPE_1,
//         COLSHAPE_CYLINDER,
//     },
//     {
//         ELEM_MATERIAL_UNK2,
//         { 0x01000000, 0x00, 0x00 },
//         { 0xF7CFFFFF, 0x00, 0x00 },
//         ATELEM_ON | ATELEM_SFX_NONE,
//         ACELEM_ON,
//         OCELEM_ON,
//     },
//     { 200, 200, 0, { 0, 0, 0 } },
// };

extern ColliderCylinderInit En_M_Thunder_CylinderInit;
ColliderCylinderInit* En_M_Thunder_CylinderInit2 = (ColliderCylinderInit*)0x8053AD30;

// static u8 sDamages[] = {
//     1, 2, 3, 4, // Regular
//     1, 2, 3, 4, // Great Spin
// };

extern u8 En_M_Thunder_Damages[8];
u8* En_M_Thunder_Damages2 = (u8*)0x8053AD5C;

typedef enum {
    /* 0 */ ENMTHUNDER_SUBTYPE_SPIN_GREAT,
    /* 1 */ ENMTHUNDER_SUBTYPE_SPIN_REGULAR,
    /* 2 */ ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT,
    /* 3 */ ENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR,
    /* 4 */ ENMTHUNDER_SUBTYPE_MAX
} EnMThunderSubType;


void EnMThunder_UnkType_Setup(EnMThunder* this, PlayState* play);
void EnMThunder_Init(Actor* thisx, PlayState* play);
void EnMThunder_Destroy(Actor* thisx, PlayState* play);
void EnMThunder_Update(Actor* thisx, PlayState* play);
void EnMThunder_Draw(Actor* thisx, PlayState* play2);
void EnMThunder_UnkType_Update(Actor* thisx, PlayState* play);
void EnMThunder_AdjustLights(PlayState* play, f32 arg1);
void EnMThunder_Charge(EnMThunder* this, PlayState* play);
void EnMThunder_Spin_Attack(EnMThunder* this, PlayState* play);
void EnMThunder_Spin_AttackNoMagic(EnMThunder* this, PlayState* play);
void EnMThunder_SwordBeam_Attack(EnMThunder* this, PlayState* play);
void EnMThunder_UnkType_Attack(EnMThunder* this, PlayState* play);
void Magic_Update(PlayState* play);
s32 Player_CanSpinAttack(Player* this);

static bool isMagicQuickspin = false;
static bool lightsOn = false;
void FixMagicCost(Actor* thisx, PlayState* play) {
    EnMThunder* this = (EnMThunder*)thisx;
    s16 cost = ENMTHUNDER_GET_MAGIC_COST(&this->actor);
    // if (cost == 0 && Player_CanSpinAttack(player)) {
    if (cost == 0) {
        s16 new_cost = QUICKSPIN_MAGIC_USAGE;
        s16 old_params = (&this->actor)->params & 0x00FF;

        (&this->actor)->params = old_params | (new_cost << 8);
        recomp_printf("New Magic Cost %i - (&this->actor)->params = 0x%04X\n", ENMTHUNDER_GET_MAGIC_COST(&this->actor), (&this->actor)->params);
    }
}

#define SEARCH_SIZE 100
RECOMP_PATCH void EnMThunder_Init(Actor* thisx, PlayState* play) {
    
    /*
    void* search_pointer = &En_M_Thunder_Profile;
    for (int i = 0; i < SEARCH_SIZE; i++) {
        recomp_printf("Memory at Address %04X: %01X -> %i", search_pointer, *((u8*)search_pointer), *((u8*)search_pointer));
        if (search_pointer == &En_M_Thunder_Damages) {
            recomp_printf(" (Target Hit)");
        }
        recomp_printf("\n");
        search_pointer++;
    }
    recomp_printf("Values of En_M_Thunder_Damage (%04X):", &En_M_Thunder_Damages);
    for (int i = 0; i < 8; i++) {
        recomp_printf(" %01X,", En_M_Thunder_Damages[i]);
    }
    recomp_printf("\n");

    recomp_printf("Values of En_M_Thunder_Damage2 (%04X):", En_M_Thunder_Damages2);
    for (int i = 0; i < 8; i++) {
        recomp_printf(" %01X,", En_M_Thunder_Damages2[i]);
    }
    recomp_printf("\n");

    recomp_printf("Values of En_M_Thunder_CylinderInit (%04X):", &En_M_Thunder_CylinderInit);
    recomp_printf(" %08X,", En_M_Thunder_CylinderInit);
    recomp_printf("\n");

    recomp_printf("Values of En_M_Thunder_CylinderInit2 (%04X):", En_M_Thunder_CylinderInit2);
    recomp_printf(" %08X,", *En_M_Thunder_CylinderInit2);
    recomp_printf("\n");*/

    s32 pad;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &En_M_Thunder_CylinderInit);
    this->type = ENMTHUNDER_GET_TYPE(&this->actor);
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, 255, 255, 255, 0);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);

    if (this->type == ENMTHUNDER_TYPE_UNK) {
        EnMThunder_UnkType_Setup(this, play);
        return;
    }

    this->collider.dim.radius = 0;
    this->collider.dim.height = 40;
    this->collider.dim.yShift = -20;
    this->timer = 8;
    this->scroll = 0.0f;
    this->actor.world.pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    this->lightColorFrac = 0.0f;
    this->adjustLightsArg1 = 0.0f;
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.shape.rot.x = -this->actor.world.rot.x;
    this->actor.room = -1;
    Actor_SetScale(&this->actor, 0.1f);
    this->isCharging = false;

    if (player->stateFlags2 & PLAYER_STATE2_20000) {
        if (
            !gSaveContext.save.saveInfo.playerData.isMagicAcquired 
            || (gSaveContext.magicState != MAGIC_STATE_IDLE) 
            || (
                (ENMTHUNDER_GET_MAGIC_COST(&this->actor) != 0) 
                && !Magic_Consume(play, ENMTHUNDER_GET_MAGIC_COST(&this->actor), MAGIC_CONSUME_NOW))
            ) {
            AudioSfx_PlaySfx(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            AudioSfx_PlaySfx(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Actor_Kill(&this->actor);
            return;
        }

        player->stateFlags2 &= ~PLAYER_STATE2_20000;
        this->isCharging = false;

        if (CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK)) {
            player->unk_B08 = 1.0f;
            this->collider.elem.atDmgInfo.damage = En_M_Thunder_Damages[this->type + ENMTHUNDER_TYPE_MAX];
            this->subtype = ENMTHUNDER_SUBTYPE_SPIN_GREAT;
            if (this->type == ENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 6;
            } else if (this->type == ENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 4;
            } else {
                this->scaleTarget = 3;
            }
        } else {
            player->unk_B08 = 0.5f;
            this->collider.elem.atDmgInfo.damage = En_M_Thunder_Damages[this->type];
            this->subtype = ENMTHUNDER_SUBTYPE_SPIN_REGULAR;
            if (this->type == ENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 4;
            } else if (this->type == ENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 3;
            } else {
                this->scaleTarget = 2;
            }
        }

        if (player->meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H) {
            this->subtype += ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT;
            this->actionFunc = EnMThunder_SwordBeam_Attack;
            this->timer = 1;
            this->scaleTarget = 12;
            this->collider.elem.atDmgInfo.dmgFlags = DMG_SWORD_BEAM;
            this->collider.elem.atDmgInfo.damage = 3;
        } else {
            // This is a quickspin. This shouldn't be free.
            FixMagicCost(thisx, play);

            if (Magic_Consume(play, ENMTHUNDER_GET_MAGIC_COST(&this->actor), MAGIC_CONSUME_NOW)) {
                isMagicQuickspin = true;
                this->actionFunc = EnMThunder_Spin_Attack;
                this->timer = 8;
                EnMThunder_AdjustLights(play, 0.0f);
            } else {
                // this->actionFunc = EnMThunder_Spin_AttackNoMagic;
                // this->timer = 8;
                AudioSfx_PlaySfx(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                AudioSfx_PlaySfx(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                Actor_Kill(&this->actor);
                return;
            }
        }

        AudioSfx_PlaySfx(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

        this->lightColorFrac = 1.0f;
    } else {
        this->actionFunc = EnMThunder_Charge;
    }

    this->actor.child = NULL;
}

RECOMP_HOOK("EnMThunder_Destroy") void pre_EnMThunder_Destroy(Actor* thisx, PlayState* play) {
    if (isMagicQuickspin) {
        isMagicQuickspin = false;
        Magic_Reset(play);
    }
}

// RECOMP_PATCH void EnMThunder_Destroy(Actor* thisx, PlayState* play) {
//     EnMThunder* this = (EnMThunder*)thisx;

//     if (this->isCharging || isMagicQuickspin) {
//         isMagicQuickspin = false;
//         Magic_Reset(play);
//     }

//     Collider_DestroyCylinder(play, &this->collider);
//     EnMThunder_AdjustLights(play, 0.0f);
//     LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
// }
