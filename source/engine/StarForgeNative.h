#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void StarForge_NativeLoad(void);
int StarForge_StaticActorType(int type);
void StarForge_StaticForget(void* object);
void StarForge_StaticReset(void);
void StarForge_StaticBind(void* object,const float* position,const float* rotation);
int StarForge_IsStatic(void* object);
int StarForge_StaticApply(void* object,float* position,float* rotation);
int StarForge_RemixStatic(int entry);
unsigned short* StarForge_StaticScript(void* object);
void StarForge_NativeReactDamage(void* object);
void* StarForge_NativeAttachment(void* object,float* offset,float* roll);
void StarForge_PresentationReload(void);
const char* StarForge_LevelName(int level);
unsigned char* StarForge_LevelTextTexture(int level,int subtitle,int* width,int* height);
const char* StarForge_LevelSubtitle(int level);
int StarForge_MedalRequirement(int level,int fallback);
int StarForge_DrawLevelPresentation(int level,float center,float nameY,float subtitleY,float maxWidth);
int StarForge_ObjectSkew(void* object,float* shear);
int StarForge_NativeRouteCondition(int level,int condition,int fallback);
void* StarForge_NativeObjects(int level,int phase);
unsigned short* StarForge_NativeScript(int level,int script);
void StarForge_NativeOverride(int level,float z,int id,int x,int y,int* hp,int* points);
unsigned short* StarForge_NativeDialogue(int level,int script,int command,int* speaker,unsigned short* original);
int StarForge_NativeVoiceId(unsigned short* message);
void StarForge_NativeRegister(void* object,float z,int id,int x,int y,int z2,int frame,float progress,int level);
float StarForge_NativeScale(void* object);
const char* StarForge_NativeUid(void* object);
int StarForge_NativeBehavior(void* object,int frame,const char** target,float* speed);
int StarForge_NativeMove(void* object,float* x,float* y,float* z,float* rx,float* ry,int frame,float progress);
float StarForge_NativeBirdSpeed(void* object,float original);
float* StarForge_NativeHitbox(void* object,float* original);
int StarForge_NativeMusicSequence(int level,int sequence);
int StarForge_NativeMusicActive(int level);
void StarForge_SetMessageVoice(unsigned short* message,int voice);
#ifdef __cplusplus
}
#endif
