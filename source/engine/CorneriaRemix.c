#include "StarForgeImportedScene.h"
#ifdef _MSC_VER
/* Rotation ABI mismatches are memory-corruption bugs, never harmless warnings. */
#pragma warning(error: 4133)
#endif
#include "StarForgeNative.h"
#include "global.h"
#include "sf64event.h"
#include "assets/ast_common.h"
#include "CorneriaRemix.h"
#include "CorneriaRemixEncounters.h"
#include "StarForgeRuntime.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { int used, mode, lane, parent, serial, parentSerial,entry,shoot,nativeMovement; float born,x,y,depth,speed; } CR_Track;
static CR_Track tracks[60];
static int running, finale, wave, sceneryBeat, serial;
static float clockTime;
static FILE* logFile;
static ObjectInit noObjects[]={{0}};
static const CR_Encounter* sequence=encounters;
static const CR_Setting* settingsSequence=settings;
static int encounterCount=ARRAY_COUNT(encounters),settingCount=ARRAY_COUNT(settings);
static unsigned char waveSpawned[500];
static int propEntries[ARRAY_COUNT(gScenery)];
static u16 friendScript[]={EVENT_INIT_ACTOR(EVID_TEAMMATE,511),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 billScript[]={EVENT_INIT_ACTOR(EVID_BILL,511),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 kattScript[]={EVENT_INIT_ACTOR(EVID_KATT,511),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 supplyScript[]={EVENT_INIT_ACTOR(EVID_SUPPLY_CRATE,10),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 fighter[]={EVENT_INIT_ACTOR(EVID_VENOM_FIGHTER_1,10),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 robot[]={EVENT_INIT_ACTOR(EVID_SY_ROBOT_2,10),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 rock[]={EVENT_INIT_ACTOR(EVID_ME_METEOR_4,20),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 cruiser[]={EVENT_INIT_ACTOR(EVID_ZERAM_CLASS_CRUISER,140),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 carrier[]={EVENT_INIT_ACTOR(EVID_SY_SARUZIN,255),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};
static u16 turret[]={EVENT_INIT_ACTOR(EVID_CRUISER_GUN,10),EVENT_STOP_ROTATE(),EVENT_PURSUE_PLAYER(10000,6),EVENT_STOP_SCRIPT()};
#define CR_SCRIPT(type,hp) {EVENT_INIT_ACTOR(type,hp),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()}
static u16 variety[][8]={
 CR_SCRIPT(EVID_VENOM_FIGHTER_2,10),CR_SCRIPT(EVID_GRANGA_FIGHTER_1,10),
 CR_SCRIPT(EVID_GRANGA_FIGHTER_2,10),CR_SCRIPT(EVID_ATTACKER_1,10),
 CR_SCRIPT(EVID_ATTACKER_2,10),CR_SCRIPT(EVID_ATTACKER_3,10),
 CR_SCRIPT(EVID_COMMANDER,10),CR_SCRIPT(EVID_SX_BORZOI_FIGHTER,10),
 CR_SCRIPT(EVID_TI_BOMBER,10),CR_SCRIPT(EVID_SY_ROBOT_1,10),
 CR_SCRIPT(EVID_SY_ROBOT_3,10),CR_SCRIPT(EVID_SPACE_MINE,10),
 CR_SCRIPT(EVID_SPY_EYE,10),CR_SCRIPT(EVID_SY_SHIP_1,255),
 CR_SCRIPT(EVID_SY_SHIP_2,255),CR_SCRIPT(EVID_A6_HARLOCK_FRIGATE,140)
};
static int Enabled(void){return gGameState==GSTATE_PLAY&&gCurrentLevel==CVarGetInteger("gStarForgeRemixLevel",LEVEL_CORNERIA)&&CVarGetInteger("gCorneriaRemix",0);}
int CorneriaRemix_EnvironmentEnabled(void){return Enabled();}
int CorneriaRemix_SuppressObjects(void){return Enabled()&&running;}
int CorneriaRemix_ManagedActor(int i){return Enabled()&&running&&!finale&&i>=0&&i<60&&
    (tracks[i].used||(gActors[i].obj.id==OBJ_ACTOR_EVENT&&gActors[i].aiType>=140&&gActors[i].aiType<=165)||(gActors[i].obj.id==OBJ_ACTOR_EVENT&&gActors[i].aiType>=200&&gActors[i].aiType<=307)||(gActors[i].obj.id==OBJ_ACTOR_EVENT&&gActors[i].aiType>=400));}
uint16_t* CorneriaRemix_Script(int script){
    if(!Enabled()||!running)return NULL;
    if(script>=400)return StarForge_OriginalEnemyScript(script);
    if(script>=200&&script<=307){static u16 individual[]={EVENT_INIT_ACTOR(0,10),EVENT_SET_ROTATE(),EVENT_SET_SPEED(0,EMZ_REST,10000),EVENT_STOP_SCRIPT()};individual[1]=script-200;return individual;}
    if(script<140||script>165)return NULL;
    if(script==164)return billScript;if(script==165)return kattScript;if(script==162)return friendScript;if(script==163)return supplyScript;
    if(script>=146)return variety[script-146];
    switch(script){case 140:return fighter;case 141:return robot;case 142:return rock;case 143:return cruiser;case 144:return carrier;default:return turret;}
}
extern void Actor_Load(Actor*,ObjectInit*);
static int SpawnNativeActor(int entry,int lane,float x,float y,float depth){int i;ObjectInit init={0};float rotation[3]={0};for(i=0;i<60;i++)if(gActors[i].obj.status==OBJ_FREE)break;if(i==60)return -1;StarForge_OriginalSpawnRotation(entry,rotation);init.id=StarForge_RemixNativeId(entry);int required=init.id==250||init.id==232?3:init.id==227?2:1,freeSlots=0;for(int k=0;k<60;k++)if(gActors[k].obj.status==OBJ_FREE)freeSlots++;if(freeSlots<required)return -1;if(init.id==177||init.id==178){int freeScenery=0;for(int k=0;k<ARRAY_COUNT(gScenery);k++)if(gScenery[k].obj.status==OBJ_FREE)freeScenery++;if(!freeScenery)return -1;}if(init.id==250||init.id==251){rotation[0]=0;rotation[2]=0;}init.xPos=x;init.yPos=y;init.zPos1=gPathProgress;init.zPos2=3000-depth;init.rot.x=(s16)rotation[0];init.rot.y=(s16)rotation[1];init.rot.z=(s16)rotation[2];Actor_Load(&gActors[i],&init);gActors[i].index=i;memset(&tracks[i],0,sizeof(tracks[i]));tracks[i].used=1;tracks[i].entry=entry;tracks[i].lane=lane;tracks[i].mode=0;tracks[i].parent=-1;tracks[i].serial=++serial;tracks[i].born=clockTime;tracks[i].x=x;tracks[i].y=y;tracks[i].depth=depth;gActors[i].info.cullDistance=100000;return i;}
float StarForge_RemixBirdSpeed(void*object,float original){int i;if(!Enabled()||!running)return original;for(i=0;i<60;i++)if(object==&gActors[i].obj&&tracks[i].used)return StarForge_RemixNativeSpeed(tracks[i].entry,original);return original;}
static int SpawnWithDepth(int script,int mode,int lane,float x,float y,float depth){
    int i;ObjectInit init={0};
    for(i=0;i<60;i++)if(gActors[i].obj.status==OBJ_FREE)break;
    if(i==60){if(logFile)fprintf(logFile,"%.2f actor pool full\n",clockTime);return -1;}
    init.id=1000+script;init.zPos1=gPathProgress;init.zPos2=3000-depth;init.xPos=x;init.yPos=y;
    if(script>=400){
        /* ObjectInit uses 16-bit angles, while authored rotations are floats.
         * Never write float values directly into its packed rotation fields. */
        float rotation[3]={0.0f,0.0f,0.0f};
        StarForge_OriginalSpawnRotation(script-400,rotation);
        init.rot.x=(s16)rotation[0];init.rot.y=(s16)rotation[1];init.rot.z=(s16)rotation[2];
    }
    ActorEvent_Load(&gActors[i],&init,i);
    if(gActors[i].obj.status!=OBJ_ACTIVE)return -1;
    memset(&tracks[i],0,sizeof(tracks[i]));tracks[i].used=1;tracks[i].mode=mode;tracks[i].lane=lane;
    StarForge_ReactForget(&gActors[i].obj);tracks[i].born=clockTime;tracks[i].x=x;tracks[i].y=y;tracks[i].parent=-1;tracks[i].serial=++serial;
    tracks[i].depth=6500;tracks[i].speed=850;tracks[i].entry=-1;tracks[i].shoot=1;
    gActors[i].info.cullDistance=100000;
    if(mode==4){gActors[i].info.hitbox=LOAD_ASSET(gNoHitbox);gActors[i].info.bonus=0;gActors[i].info.damage=0;gActors[i].scale=1.5f;}
    return i;
}
static int Spawn(int script,int mode,int lane,float x,float y){return SpawnWithDepth(script,mode,lane,x,y,7000);}
static void Fleet(int setPiece){
    int hull=-1,j,gun;
    if(setPiece==1)hull=Spawn(143,5,0,-1500,180);
    if(hull>=0)for(j=0;j<7;j++){gun=Spawn(145,6,j,0,0);if(gun>=0){tracks[gun].parent=hull;tracks[gun].parentSerial=tracks[hull].serial;}}
    if(setPiece==2){Spawn(144,4,0,-3100,1100);Spawn(159,4,1,2900,900);}
    if(setPiece==3){Spawn(161,5,0,1700,250);Spawn(160,4,1,-3300,1300);}
}
static void Wave(void){
    const CR_Encounter* e=&sequence[wave];int j,slot;
    for(j=0;j<e->count;j++){
        float offset=(j-(e->count-1)*0.5f)*e->gap,member[3];int shaped=StarForge_FormationOffset(wave,j,member);
        int original=StarForge_UsesOriginalMovement(wave);
        slot=StarForge_RemixNativeId(wave)>0?SpawnNativeActor(wave,j,e->x+(shaped?member[0]:offset),e->y+(shaped?member[1]:0),e->depth+(shaped?member[2]:j*100)):SpawnWithDepth(original?400+wave:e->script,e->mode,j,e->x+(shaped?member[0]:offset),e->y+(shaped?member[1]:0),e->depth+(shaped?member[2]:j*100));
        if(slot>=0){tracks[slot].depth=e->depth+(shaped?member[2]:j*100);tracks[slot].speed=e->speed;tracks[slot].entry=wave;tracks[slot].nativeMovement=original;if(StarForge_RemixStatic(wave)){tracks[slot].shoot=0;float rotation[3]={0,0,0};StarForge_OriginalSpawnRotation(wave,rotation);gActors[slot].obj.rot.x=rotation[0];gActors[slot].obj.rot.y=rotation[1];gActors[slot].obj.rot.z=rotation[2];StarForge_RemixTransform(0,wave,&gActors[slot].obj,&gActors[slot].obj.rot.x);gActors[slot].rot_0F4=gActors[slot].obj.rot;StarForge_StaticBind(&gActors[slot].obj,&gActors[slot].obj.pos.x,&gActors[slot].obj.rot.x);if(gActors[slot].eventType>=EVID_WZ_PILLAR_1&&gActors[slot].eventType<=EVID_WZ_GATE)gActors[slot].fwork[15]=gActors[slot].fwork[16]=gActors[slot].fwork[17]=255;}StarForge_RemixGroupBind(&gActors[slot].obj,wave,j);int guns,side=0,k,gun;guns=StarForge_CruiserGuns(wave,&side);for(k=0;k<guns;k++){gun=Spawn(145,6,k,0,0);if(gun>=0){tracks[gun].parent=slot;tracks[gun].parentSerial=tracks[slot].serial;tracks[gun].x=guns==5?side:0;}}int parentEntry=StarForge_ParentEntry(wave);if(parentEntry>=0){int parent;for(parent=0;parent<60;parent++)if(tracks[parent].used&&tracks[parent].entry==parentEntry){tracks[slot].parent=parent;tracks[slot].parentSerial=tracks[parent].serial;break;}}
            if(e->overrides){gActors[slot].health=e->hp;gActors[slot].info.bonus=e->points;}if(StarForge_IsDecorative(wave)){tracks[slot].shoot=0;gActors[slot].info.hitbox=LOAD_ASSET(gNoHitbox);gActors[slot].info.damage=0;gActors[slot].info.bonus=0;}}
    }
    if(e->setPiece)Fleet(e->setPiece);
    if(logFile){fprintf(logFile,"%.2f encounter %d: %s\n",clockTime,wave+1,e->name);fflush(logFile);}
    wave++;
}
int StarForge_RadioBusy(void){return !StarForge_PlaytestWarming()&&gRadioState!=0;}
void StarForge_PlayRadio(int id,int speaker,unsigned short* custom){u16* msg=custom?custom:Message_PtrFromId(id);if(msg)Radio_PlayMessage(msg,speaker);}
void StarForge_AddItem(int id,float x,float y,float z,int entry){int i;ObjectInit init={0};for(i=0;i<ARRAY_COUNT(gItems);i++)if(gItems[i].obj.status==OBJ_FREE)break;if(i==ARRAY_COUNT(gItems))return;init.id=id;init.xPos=x;init.yPos=y;init.zPos1=gPathProgress;init.zPos2=3000-z;Item_Load(&gItems[i],&init);StarForge_RemixTransform(3,entry,&gItems[i].obj,&gItems[i].obj.rot.x);}
void StarForge_Flyby(int eventIndex,int entry,int ship,float x,float y,int shoot){int slot=Spawn(ship==5?164:ship==4?165:162,30,0,x,y);if(slot<0)return;tracks[slot].entry=entry;tracks[slot].shoot=shoot;gActors[slot].iwork[12]=ship;gActors[slot].info.bonus=0;gActors[slot].info.damage=0;}
void StarForge_Supply(void){int slot=Spawn(163,31,0,0,350);if(slot>=0){tracks[slot].depth=5000;tracks[slot].speed=450;tracks[slot].shoot=0;}}
static void Prop(int id,float x,float yaw,int entry){
    if(id<0)return;
    int i;ObjectInit init={0};
    for(i=0;i<ARRAY_COUNT(gScenery);i++)if(gScenery[i].obj.status==OBJ_FREE)break;
    if(i==ARRAY_COUNT(gScenery))return;
    init.id=id;init.zPos1=gPathProgress;init.zPos2=3000-StarForge_SceneryDepth(entry);init.xPos=x;init.yPos=StarForge_SceneryHeight(entry,gGroundHeight);init.rot.y=yaw;
    Scenery_Load(&gScenery[i],&init);propEntries[i]=entry;StarForge_RemixTransform(1,entry,&gScenery[i].obj,&gScenery[i].obj.rot.x);StarForge_StaticBind(&gScenery[i].obj,&gScenery[i].obj.pos.x,&gScenery[i].obj.rot.x);
}
void CorneriaRemix_Reset(void){
    StarForge_StaticReset();
    running=finale=0;clockTime=0;wave=sceneryBeat=serial=0;
    memset(tracks,0,sizeof(tracks));memset(waveSpawned,0,sizeof(waveSpawned));memset(propEntries,-1,sizeof(propEntries));
    sequence=encounters;encounterCount=ARRAY_COUNT(encounters);
    settingsSequence=settings;settingCount=ARRAY_COUNT(settings);
    if(logFile){fclose(logFile);logFile=NULL;}
}
void CorneriaRemix_PreUpdate(void){
    int i;
    StarForge_ApplyEnvironment();
    if(!Enabled()||gPlayState<=PLAY_INIT){running=finale=0;return;}
    if(gPlayer[0].state==PLAYERSTATE_LEVEL_INTRO){running=finale=0;return;}
    if(gPlayer[0].state!=PLAYERSTATE_ACTIVE||finale)return;
    if(!running){
        running=1;clockTime=0;wave=sceneryBeat=serial=0;memset(tracks,0,sizeof(tracks));
        memset(waveSpawned,0,sizeof(waveSpawned));for(i=0;i<2;i++)gMeMoraStatus[i]=0;Titania_80188F30();StarForge_Load();int n;const CR_Encounter* edited=StarForge_Waves(&n);
        sequence=edited?edited:encounters;encounterCount=edited?n:ARRAY_COUNT(encounters);
        const CR_Setting* editedSettings=StarForge_Scenery(&n);
        settingsSequence=editedSettings?editedSettings:settings;settingCount=editedSettings?n:ARRAY_COUNT(settings);
        if(logFile)fclose(logFile);logFile=fopen("CorneriaRemix.txt","w");
        for(i=0;i<60;i++)if(gActors[i].obj.status!=OBJ_FREE)Object_Kill(&gActors[i].obj,gActors[i].sfxSource);
        for(i=0;i<ARRAY_COUNT(gScenery);i++)if(gScenery[i].obj.status!=OBJ_FREE)Object_Kill(&gScenery[i].obj,gScenery[i].sfxSource);
        for(i=0;i<ARRAY_COUNT(gBosses);i++)if(gBosses[i].obj.status!=OBJ_FREE)Object_Kill(&gBosses[i].obj,gBosses[i].sfxSource);
        if(gCurrentLevel==LEVEL_CORNERIA&&!StarForge_HasEnvironment())gGroundSurface=SURFACE_GRASS;
    }
    gLevelObjects=noObjects;gObjectLoadIndex=0;
}
void CorneriaRemix_Update(void){
    if(StarForge_ImportedActive())return;
    int i,j;float age,x,y,z,phase;Player* p=&gPlayer[0];
    if(!Enabled()||!running||finale||p->state!=PLAYERSTATE_ACTIVE)return;
    StarForge_Update(clockTime,p->pos.x,p->pos.y,gPathProgress);
    for(j=0;j<encounterCount;j++)if(!waveSpawned[j]&&clockTime>=sequence[j].time&&StarForge_WaveReady(j)){wave=j;Wave();waveSpawned[j]=1;}
    if(sceneryBeat<settingCount&&clockTime>=settingsSequence[sceneryBeat].time){
        const CR_Setting* s=&settingsSequence[sceneryBeat];
        Prop(s->left,s->xLeft,s->yawLeft,sceneryBeat);Prop(s->right,s->xRight,s->yawRight,sceneryBeat);
        if(settingsSequence==settings&&gCurrentLevel==LEVEL_CORNERIA&&sceneryBeat==7)Prop(OBJ_SCENERY_CO_STONE_ARCH,0,0,-1);
        sceneryBeat++;
    }
    for(i=0;i<60;i++)if(tracks[i].used){
        CR_Track* t=&tracks[i];Actor* a=&gActors[i];
        if(a->obj.status==OBJ_INIT)continue;if(a->obj.status!=OBJ_ACTIVE){t->used=0;continue;}if(t->parent>=0&&(!tracks[t->parent].used||tracks[t->parent].serial!=t->parentSerial||gActors[t->parent].obj.status!=OBJ_ACTIVE)){Object_Kill(&a->obj,a->sfxSource);t->used=0;continue;}
        if(StarForge_IsStatic(&a->obj)){StarForge_StaticApply(&a->obj,&a->obj.pos.x,&a->obj.rot.x);a->vel.x=a->vel.y=a->vel.z=0;a->rot_0F4=a->obj.rot;if(a->obj.pos.z+gPathProgress>1500){Object_Kill(&a->obj,a->sfxSource);t->used=0;}continue;}
        age=clockTime-t->born;age=StarForge_ReactAge(&a->obj,t->entry,age,a->obj.pos.z,gPathProgress);{int targetEntry=-1;float flightSpeed=1200,flightDuration=16;int behavior=StarForge_ReactiveInfo(t->entry,&targetEntry,&flightSpeed,&flightDuration);if(behavior){if(age>flightDuration){Object_Kill(&a->obj,a->sfxSource);t->used=0;continue;}Vec3f target={p->pos.x,p->pos.y,p->trueZpos};int found=behavior==2,k;if(behavior==1){for(k=0;k<60;k++)if(k!=i&&tracks[k].used&&tracks[k].entry==targetEntry&&gActors[k].obj.status==OBJ_ACTIVE){target=gActors[k].obj.pos;found=1;break;}if(!found&&targetEntry>=20000)for(k=0;k<ARRAY_COUNT(gScenery);k++)if(propEntries[k]==targetEntry-20000&&gScenery[k].obj.status==OBJ_ACTIVE){target=gScenery[k].obj.pos;found=1;break;}}if(found){float dx=target.x-a->obj.pos.x,dy=target.y-a->obj.pos.y,dz=target.z-a->obj.pos.z,length=sqrtf(dx*dx+dy*dy+dz*dz),step=flightSpeed*(gVIsPerFrame>0?gVIsPerFrame:2)/60.0f;if(length>.00001f){if(step>length)step=length;a->obj.pos.x+=dx/length*step;a->obj.pos.y+=dy/length*step;a->obj.pos.z+=dz/length*step;a->obj.rot.y=atan2f(dx,dz)*M_RTOD;a->obj.rot.x=-atan2f(dy,sqrtf(dx*dx+dz*dz))*M_RTOD;}}a->vel.x=a->vel.y=a->vel.z=0;StarForge_RemixTransform(t->mode==30?2:0,t->entry,&a->obj,&a->obj.rot.x);a->rot_0F4=a->obj.rot;continue;}}if(t->nativeMovement){if(age>120||(a->obj.pos.z+gPathProgress)>3000){if(a->obj.id==OBJ_ACTOR_TI_DESERT_CRAWLER)Titania_8018E3B0(a);if(a->obj.id==OBJ_ACTOR_ME_MORA||(a->obj.id==OBJ_ACTOR_EVENT&&a->eventType>=200&&a->eventType<300))if(a->work_046>=0&&a->work_046<2)gMeMoraStatus[a->work_046]=0;Object_Kill(&a->obj,a->sfxSource);t->used=0;}else StarForge_RemixTransform(0,t->entry,&a->obj,&a->obj.rot.x);continue;}x=t->x;y=t->y;z=t->depth-age*t->speed;phase=age*1.4f+t->lane*0.785398f;
        switch(t->mode){
            case 0:x=t->x+(StarForge_SwayEnabled(t->entry)?300*sinf(age):0);break;
            case 1:x=500*cosf(phase);y=420+300*sinf(phase);break;
            case 2:x=(t->lane%2?1:-1)*(1600-age*330);y=230+t->lane*100;break;
            case 3:x=t->x+200*sinf(age*2);y=500+150*cosf(phase);break;
            case 4:x=t->x;y=t->y;z=14000-age*500;break;
            case 5:z=13000-age*550;break;
            case 7:y+=fabsf(x)*0.22f;x+=120*sinf(age*0.7f);break;
            case 8:y+=t->lane*100-130;x-=age*35;break;
            case 9:y=1200+t->lane*80-age*140;x+=100*sinf(age);break;
            case 10:x+=450*sinf(age*0.8f);y+=100*cosf(age*0.8f);break;
            case 11:x+=170*sinf(age*2+t->lane);break;
            case 12:x+=(x>=0?1:-1)*age*110;y+=age*15;break;
            case 13:x*=0.8f;y+=t->lane%2?90:-90;break;
            case 14:x+=180*cosf(phase);y+=200*sinf(phase);break;
            case 15:y=130+50*sinf(age+t->lane);x+=120*cosf(age);break;
            case 16:x+=(t->lane%2?1:-1)*900*expf(-age*0.4f);break;
            case 17:y+=t->lane%2?160:-110;break;
            case 18:x+=age*(t->lane-3)*35;y-=age*20;break;
            case 19:y+=190*sinf(age*0.9f+t->lane);break;
            case 20:x+=200*sinf(age+t->lane*0.65f);y+=80*cosf(age+t->lane);break;
            case 21:y+=t->lane*90;x+=age*30;break;
            case 22:x+=250*sinf(age*1.3f+(t->lane%2)*3.14159f);y+=t->lane%2?100:-70;break;
            case 23:x*=0.65f+0.35f*cosf(age*0.6f);y+=t->lane%2?170:-90;break;
            case 24:x+=130*cosf(phase);y+=180*sinf(phase);break;
            case 25:y+=fabsf(x)*0.3f;x*=0.9f;break;
            case 26:x+=240*sinf(age*0.55f);y+=120*sinf(age*1.1f+t->lane);break;
            case 27:x+=age*(t->lane-1)*50;y+=70*cosf(age);break;
            case 28:y+=t->lane*150+100*sinf(age*0.7f);x+=80*cosf(age);break;
            case 29:y=950-age*100+t->lane*100;x+=age*(t->lane-1)*55;break;
            case 30:x=t->x+age*150;y=t->y;z=3500-age*500;break;
            case 31:break;
            case 6:{
                int parent=t->parent;
                if(parent<0||!tracks[parent].used||tracks[parent].serial!=t->parentSerial){Object_Kill(&a->obj,a->sfxSource);t->used=0;continue;}
                Vec3f local={0,283,401+t->lane*300},offset;Actor* hull=&gActors[parent];float roll=0,scale=StarForge_RemixScale(&hull->obj);if(t->lane>=3){int sign=t->x!=0?(t->x>0?1:-1):(t->lane<5?-1:1);local.x=sign*235;local.y=165;local.z=891+(t->lane%2)*300;roll=sign>0?315:45;}local.x*=scale;local.y*=scale;local.z*=scale;Matrix_RotateY(gCalcMatrix,hull->obj.rot.y*M_DTOR,MTXF_NEW);Matrix_RotateX(gCalcMatrix,hull->obj.rot.x*M_DTOR,MTXF_APPLY);Matrix_RotateZ(gCalcMatrix,hull->obj.rot.z*M_DTOR,MTXF_APPLY);Matrix_MultVec3fNoTranslate(gCalcMatrix,&local,&offset);a->obj.pos.x=hull->obj.pos.x+offset.x;a->obj.pos.y=hull->obj.pos.y+offset.y;a->obj.pos.z=hull->obj.pos.z+offset.z;a->obj.rot.z=roll;if((gGameFrameCount+i*17)%110==0)a->work_048=EVACT_2;
                a->vwork[29]=gActors[parent].obj.rot;a->rot_0F4.z=0;a->vel.x=a->vel.y=a->vel.z=0;continue;
            }
        }
        int followsCustomPath=0;
        if(t->entry>=0){float point[3];int custom=StarForge_Path(t->entry,age,t->mode==30?0:t->lane,point);if(custom==2){Object_Kill(&a->obj,a->sfxSource);t->used=0;continue;}if(custom==1){followsCustomPath=1;x=point[0];y=point[1];z=point[2];}}
        if(!followsCustomPath&&((t->speed>0&&z<-1800)||(t->speed<0&&z>14000)||age>(t->entry>=0?StarForge_ClipDuration(t->entry):32))){Object_Kill(&a->obj,a->sfxSource);t->used=0;continue;}
        a->obj.pos.x=p->xPath+x;a->obj.pos.y=p->yPath+y;a->obj.pos.z=p->trueZpos-z;
        a->vel.x=a->vel.y=a->vel.z=0;
        a->obj.rot.x=0;a->obj.rot.y=0;a->obj.rot.z=t->mode==3?age*30:0;if(followsCustomPath&&StarForge_FacesPath(t->entry)){float orientation[3];if(StarForge_PathOrientation(t->entry,age,t->mode==30?0:t->lane,orientation)){a->obj.rot.x=orientation[0];a->obj.rot.y=orientation[1];a->obj.rot.z=orientation[2];}}StarForge_RemixTransform(t->mode==30?2:0,t->entry,&a->obj,&a->obj.rot.x);a->rot_0F4=a->obj.rot;
        int scheduledFire=t->entry>=0?StarForge_EncounterFire(t->entry,age,(gVIsPerFrame>0?gVIsPerFrame:2)/60.0f):-1;
        if(scheduledFire==1)Effect_EnemyLaser(OBJ_EFFECT_ENEMY_LASER_1,a->obj.pos.x,a->obj.pos.y,a->obj.pos.z,gEnemyShotSpeed);
        if(scheduledFire<0&&t->shoot&&t->mode!=4&&t->mode!=5&&a->eventType!=EVID_ME_METEOR_4&&a->eventType!=EVID_SPACE_MINE&&a->eventType!=EVID_WZ_PILLAR_1&&a->eventType!=EVID_WZ_PILLAR_2&&a->eventType!=EVID_WZ_METEOR_1&&a->eventType!=EVID_WZ_METEOR_2&&a->eventType!=EVID_WZ_GATE&&
           ((gGameFrameCount+i*17)%110==0))a->work_048=EVACT_2;
    }
    clockTime+=(gVIsPerFrame>0?gVIsPerFrame:2)/60.0f;
    if(clockTime>=StarForge_SectionDuration()){
        ObjectInit init={0};
        for(i=0;i<60;i++)if(tracks[i].used){Object_Kill(&gActors[i].obj,gActors[i].sfxSource);tracks[i].used=0;}
        switch(gCurrentLevel){case LEVEL_METEO:init.id=OBJ_BOSS_ME_CRUSHER;break;case LEVEL_SECTOR_X:init.id=OBJ_BOSS_SX_SPYBORG;break;case LEVEL_AREA_6:init.id=OBJ_BOSS_A6_GORGON;break;case LEVEL_SECTOR_Y:init.id=OBJ_BOSS_SY_SHOGUN;break;case LEVEL_ZONESS:init.id=OBJ_BOSS_ZO_SARUMARINE;break;case LEVEL_VENOM_1:init.id=OBJ_BOSS_VE1_GOLEMECH;break;default:init.id=OBJ_BOSS_CO_GRANGA;break;}init.zPos1=gPathProgress;init.zPos2=-1000;init.yPos=gLevelType==LEVELTYPE_SPACE?0:gGroundHeight;
        Boss_Load(&gBosses[0],&init);finale=1;
        if(logFile){fprintf(logFile,"Stage %d finale, hits %d\n",gCurrentLevel,gHitCount);fflush(logFile);}
    }
}

void StarForge_PlayRadioVoice(int id,int speaker,unsigned short* custom,int voice){
    static u16 privateMessage[1024];u16* source=custom?custom:Message_PtrFromId(id);if(!source)return;
    int i;for(i=0;i<1023;i++){privateMessage[i]=source[i];if(source[i]==0)break;}privateMessage[i<1023?i:1023]=0;
    StarForge_SetMessageVoice(privateMessage,voice);Radio_PlayMessage(privateMessage,speaker);
}

#include "StarForgeImportedSceneDraw.inc"
