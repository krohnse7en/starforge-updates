#pragma once
#include "CorneriaRemixEncounters.h"
#ifdef __cplusplus
extern "C" {
#endif
void StarForge_Load(void);
void StarForge_EnvironmentLoad(int enabled);
int StarForge_EnvironmentActive(void);
int StarForge_EnvironmentLevel(int fallback,int backdrop,int enabled);
void StarForge_GroupLoad(const char* text);
void StarForge_GroupBind(void* object,const char* uid);
void StarForge_RemixGroupBind(void* object,int entry,int lane);
int StarForge_GroupDestroyed(void* object,int killed);
void StarForge_GroupRemoved(void* object);
int StarForge_RemixResolveScript(int current,int source);
float StarForge_SectionDuration(void);
int StarForge_IsDecorative(int entry);
int StarForge_FacesPath(int entry);
int StarForge_ReactiveInfo(int entry,int* target,float* speed,float* duration);
int StarForge_PathOrientation(int entry,float age,int lane,float* rotation);
int StarForge_ParentEntry(int entry);
float StarForge_ClipDuration(int entry);
unsigned short* StarForge_OriginalEnemyScript(int script);
void StarForge_OriginalSpawnRotation(int entry,float* rotation);
int StarForge_UsesOriginalMovement(int entry);
int StarForge_FormationOffset(int entry,int lane,float* point);
void StarForge_RemixTransform(int kind,int entry,void* object,float* rotation);
float StarForge_RemixScale(void* object);
const CR_Encounter* StarForge_Waves(int* count);
const CR_Setting* StarForge_Scenery(int* count);
void StarForge_Update(float time,float x,float y,float progress);
int StarForge_Path(int entry,float age,int lane,float* point);
void StarForge_ReactDamage(void* object);
void StarForge_ReactForget(void* object);
int StarForge_CruiserGuns(int entry,int* side);
float StarForge_ReactAge(void* object,int entry,float age,float z,float progress);
int StarForge_WaveReady(int entry);
float StarForge_SceneryDepth(int entry);
float StarForge_SceneryHeight(int entry,float fallback);
#ifdef __cplusplus
}
#endif

