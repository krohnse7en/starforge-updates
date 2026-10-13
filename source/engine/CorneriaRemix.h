#pragma once
#include <stdint.h>
void CorneriaRemix_Reset(void);
int CorneriaRemix_EnvironmentEnabled(void);
int StarForge_HasEnvironment(void);
int StarForge_FloorLevel(void);
int StarForge_BackdropLevel(void);
int StarForge_BackdropType(void);
int StarForge_FloorType(void);
void StarForge_ApplyEnvironment(void);
void CorneriaRemix_PreUpdate(void);
void CorneriaRemix_Update(void);
int CorneriaRemix_SuppressObjects(void);
int CorneriaRemix_ManagedActor(int index);
uint16_t* CorneriaRemix_Script(int script);
