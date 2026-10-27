#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float origin[3], radius;
    uint32_t vertexCount, flags;
    const void* vertices;
    const char* texture;
} SFI_Chunk;
void SFI_Load(const char* projectJson);
int SFI_Ready(void);
int SFI_Generation(void);
int SFI_Count(void);
int SFI_TreeCount(void);
int SFI_Tree(int index,float* position);
const SFI_Chunk* SFI_Get(int index);
float SFI_Length(void);
float SFI_Height(void);
float SFI_VertexScale(void);
float SFI_Speed(void);
int SFI_NativeFlight(void);
float SFI_Radius(void);
float SFI_Far(void);
float SFI_Camera(int field);
int SFI_BootLevel(void);
int SFI_Collide(const float* point,float radius,float* correction);
int SFI_Floor(float x,float z,float ceiling,float* height);
void SFI_Route(float progress,float* x,float* y);
void SFI_RecoveryRoute(float progress,float* x,float* y);
void SFI_FlightState(float progress,float x,float y,float z,float pathX,float pathY,float speed,float width,float height,int shields,int actors);
void SFI_Report(float progress,int drawn,int collisions);
int StarForge_ImportedActive(void);
void StarForge_ImportedDraw(void);
void StarForge_ImportedPreUpdate(void);
void StarForge_ImportedCollision(void* player);
int StarForge_ImportedCamera(void* player);
void StarForge_ImportedShot(void* shot,const float* previous);
#ifdef __cplusplus
}
#endif
