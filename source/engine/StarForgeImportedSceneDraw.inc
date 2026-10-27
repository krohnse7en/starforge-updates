#include "assets/ast_corneria.h"
#include "StarForgeImportedScene.h"
#include <stdlib.h>
#include "port/interpolation/FrameInterpolation.h"
static Gfx** sImportedLists;
static float sImportedPrevious[3],sImportedArm=415.7f;
static int sImportedCount,sImportedSerial=-1,sImportedStarted=-1,sImportedContacts;
int StarForge_ImportedActive(void){return SFI_Ready()&&gGameState==GSTATE_PLAY&&gCurrentLevel==LEVEL_CORNERIA;}
static void ImportedLists(void){
 int i,k,n,v,batch;Gfx* p;const SFI_Chunk*c;
 if(sImportedSerial==SFI_Generation())return;
 for(i=0;i<sImportedCount;i++)free(sImportedLists[i]);free(sImportedLists);sImportedCount=SFI_Count();sImportedLists=calloc(sImportedCount,sizeof(Gfx*));sImportedSerial=SFI_Generation();
 if(!sImportedLists){sImportedCount=0;return;}
 for(i=0;i<sImportedCount;i++){
  c=SFI_Get(i);n=c->vertexCount;sImportedLists[i]=malloc(sizeof(Gfx)*(n/3+n/30+4));if(!sImportedLists[i])continue;p=sImportedLists[i];
  for(k=0;k<n;k+=30){batch=n-k<30?n-k:30;gSPVertex(p++,((Vtx*)c->vertices)+k,batch,0);
   for(v=0;v<batch;v+=6){if(v+5<batch){gSP2Triangles(p++,v,v+1,v+2,0,v+3,v+4,v+5,0);}else{gSP1Triangle(p++,v,v+1,v+2,0);}}
  }gSPEndDisplayList(p++);
 }
}
void StarForge_ImportedDraw(void){
 int i,pass,drawn=0;const SFI_Chunk*c;float z;
 if(!StarForge_ImportedActive()||gReflectY<0)return;ImportedLists();
 for(pass=0;pass<2;pass++){
  RCP_SetupDL(&gMasterDisp,SETUPDL_29);gSPClearGeometryMode(gMasterDisp++,G_LIGHTING|G_CULL_BACK|G_CULL_FRONT|G_FOG|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR);gSPSetGeometryMode(gMasterDisp++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER);
  gDPSetCycleType(gMasterDisp++,G_CYC_1CYCLE);gDPSetCombineMode(gMasterDisp++,G_CC_MODULATEIA,G_CC_MODULATEIA);gDPSetTextureLUT(gMasterDisp++,G_TT_NONE);gDPSetTextureFilter(gMasterDisp++,G_TF_BILERP);gSPTexture(gMasterDisp++,65535,65535,0,G_TX_RENDERTILE,G_ON);
  if(pass){gDPSetRenderMode(gMasterDisp++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);}else{gDPSetRenderMode(gMasterDisp++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);}
  for(i=0;i<sImportedCount;i++){
   c=SFI_Get(i);if((int)(c->flags&1)!=pass||!sImportedLists[i])continue;z=c->origin[2]+gPathProgress;
   if(z-c->radius>2200*SFI_VertexScale()||z+c->radius< -SFI_Far()||fabsf(c->origin[0]-gPlayer[0].xPath)>6000*SFI_VertexScale()+c->radius)continue;
   if(c->flags&2){gDPLoadTextureBlock(gMasterDisp++,D_CO_6028A60,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,G_TX_WRAP|G_TX_NOMIRROR,G_TX_WRAP|G_TX_NOMIRROR,5,5,G_TX_NOLOD,G_TX_NOLOD);int scroll=(gGameFrameCount/2)&127;gDPSetTileSize(gMasterDisp++,G_TX_RENDERTILE,0,scroll,124,124+scroll);}else{gDPLoadTextureBlock(gMasterDisp++,c->texture,G_IM_FMT_RGBA,G_IM_SIZ_32b,32,32,0,G_TX_WRAP|G_TX_NOMIRROR,G_TX_WRAP|G_TX_NOMIRROR,5,5,G_TX_NOLOD,G_TX_NOLOD);}
   FrameInterpolation_RecordOpenChild(c,SFI_Generation());Matrix_Push(&gGfxMatrix);Matrix_Translate(gGfxMatrix,c->origin[0],c->origin[1],z,MTXF_APPLY);Matrix_Scale(gGfxMatrix,SFI_VertexScale(),SFI_VertexScale(),SFI_VertexScale(),MTXF_APPLY);Matrix_SetGfxMtx(&gMasterDisp);gSPDisplayList(gMasterDisp++,sImportedLists[i]);Matrix_Pop(&gGfxMatrix);FrameInterpolation_RecordCloseChild();drawn++;
  }
 }SFI_Report(gPathProgress,drawn,0);
}
/* Stream real Corneria sprites: native billboard draw, hitboxes, lasers and bombs. */
static unsigned char sImportedTreeState[6000];
static int sImportedTreeSlots[40],sImportedTreeGeneration=-1;
static float sImportedTreeProgress=-1;
static void ImportedTrees(void){
 int i,k,n=SFI_TreeCount();float pos[3],ahead;
 if(sImportedTreeGeneration!=SFI_Generation()||gPathProgress<sImportedTreeProgress){
  for(k=0;k<40;k++){if(sImportedTreeGeneration>=0&&sImportedTreeSlots[k]>=0&&gSprites[k].obj.id==OBJ_SPRITE_CO_TREE)gSprites[k].obj.status=OBJ_FREE;sImportedTreeSlots[k]=-1;}
  memset(sImportedTreeState,0,sizeof(sImportedTreeState));sImportedTreeGeneration=SFI_Generation();
 }
 sImportedTreeProgress=gPathProgress;
 for(k=0;k<40;k++)if(sImportedTreeSlots[k]>=0){
  i=sImportedTreeSlots[k];
  if(gSprites[k].obj.status==OBJ_FREE||gSprites[k].obj.id!=OBJ_SPRITE_CO_TREE){sImportedTreeState[i]=2;sImportedTreeSlots[k]=-1;}
  else if(gSprites[k].obj.pos.z+gPathProgress>1200){gSprites[k].obj.status=OBJ_FREE;sImportedTreeState[i]=2;sImportedTreeSlots[k]=-1;}
 }
 for(i=0;i<n;i++)if(!sImportedTreeState[i]){
  SFI_Tree(i,pos);ahead=-pos[2]-gPathProgress;
  if(ahead< -1200){sImportedTreeState[i]=2;continue;}
  if(ahead>6500)break;
  /* Match the narrow rail view; distant roadside groves need not fill the pool. */
  if(fabsf(pos[0]-gPlayer[0].cam.eye.x)>fmaxf(0,ahead)*.65f+1400)continue;
  for(k=0;k<40;k++)if(gSprites[k].obj.status==OBJ_FREE)break;
  if(k==40)break;
  {ObjectInit init={0};init.id=OBJ_SPRITE_CO_TREE;init.zPos1=-pos[2];init.zPos2=3000;init.xPos=(s16)pos[0];init.yPos=(s16)pos[1];Sprite_Load(&gSprites[k],&init);}
  sImportedTreeSlots[k]=i;sImportedTreeState[i]=1;
 }
}
void StarForge_ImportedPreUpdate(void){
 Player*p=&gPlayer[0];float x,y;
 if(!StarForge_ImportedActive()||p->state!=PLAYERSTATE_ACTIVE)return;
 if(sImportedStarted!=SFI_Generation()){sImportedStarted=SFI_Generation();sImportedContacts=0;sImportedArm=SFI_Camera(0);if(!SFI_NativeFlight()){p->pos.y=SFI_Height();SFI_Route(0,&x,&y);p->pos.x=x;p->pos.y=y;p->xPath=p->xPathTarget=x;p->yPath=p->yPathTarget=y;p->pathWidth=700;p->pathHeight=720;p->pathFloor=-800;}}
 ImportedTrees();
 if(gPathProgress>=SFI_Length()){gPlayState=PLAY_INIT;return;}
 {int live=0;for(int i=0;i<60;i++)if(gActors[i].obj.status!=OBJ_FREE)live++;SFI_FlightState(gPathProgress,p->pos.x,p->pos.y,p->trueZpos,p->xPath,p->yPath,p->baseSpeed,p->pathWidth,p->pathHeight,p->shields,live);}
 sImportedPrevious[0]=p->pos.x;sImportedPrevious[1]=p->pos.y;sImportedPrevious[2]=p->trueZpos;
 if(!SFI_NativeFlight()){SFI_Route(gPathProgress,&x,&y);p->pos.x+=x-p->xPath;p->pos.y+=y-p->yPath;p->xPath=p->xPathTarget=x;p->yPath=p->yPathTarget=y;p->baseSpeed=SFI_Speed()/30.f;gFovY=SFI_Camera(4);}gProjectNear=40;gProjectFar=SFI_Far();gGroundHeight=-10000;gGroundSurface=SURFACE_GRASS;
 {float floor;if(SFI_Floor(p->pos.x,p->trueZpos,p->pos.y+SFI_Radius(),&floor)){gGroundHeight=floor;p->pathFloor=floor+(SFI_NativeFlight()?40:50)-p->yPath;}else if(!SFI_NativeFlight())p->pathFloor=-800;}
}
void StarForge_ImportedCollision(void* object){
 Player*p=(Player*)object;float target[3],point[3],correction[3],delta[3],travel=0,floor,intendedZ;int i,k,step,steps,hit=0;
 if(!StarForge_ImportedActive()||p->state!=PLAYERSTATE_ACTIVE)return;
 target[0]=p->pos.x;target[1]=p->pos.y;target[2]=p->trueZpos;intendedZ=target[2];
 for(k=0;k<3;k++){delta[k]=target[k]-sImportedPrevious[k];travel+=delta[k]*delta[k];}steps=(int)(sqrtf(travel)/10)+1;if(steps>128)steps=128;
 for(step=1;step<=steps;step++){for(k=0;k<3;k++)point[k]=target[k]-delta[k]*(1-step/(float)steps);
  for(i=0;i<3;i++){if(!SFI_Collide(point,SFI_Radius(),correction))break;hit=1;for(k=0;k<3;k++){point[k]+=correction[k];target[k]+=correction[k];}}
 }
 /* A rail stage cannot wait behind a wall: recover sideways/upward into the clear route. */
 {
  int blocked=hit && (fabsf(target[2]-intendedZ)>1.0f || SFI_Collide(target,SFI_Radius(),correction));
  float routeX,routeY,candidate[3],safe[3],t,distance;int recovered=0,recoverySteps;
  SFI_RecoveryRoute(fmaxf(0,-intendedZ),&routeX,&routeY);
  distance=sqrtf((routeX-target[0])*(routeX-target[0])+(routeY-target[1])*(routeY-target[1]));
  recoverySteps=(int)(distance/10)+1;if(recoverySteps>256)recoverySteps=256;
  /* Walk from the known-clear corridor toward the hit, stopping before the first surface. */
  for(i=0;i<=recoverySteps;i++){
   t=i/(float)recoverySteps;candidate[0]=routeX+(target[0]-routeX)*t;candidate[1]=routeY+(target[1]-routeY)*t;candidate[2]=intendedZ;
   if(SFI_Floor(candidate[0],candidate[2],candidate[1]+SFI_Radius(),&floor))candidate[1]=fmaxf(candidate[1],floor+(SFI_NativeFlight()?40:50));
   if(SFI_Collide(candidate,SFI_Radius()+2,correction)){blocked=1;break;}
   for(k=0;k<3;k++)safe[k]=candidate[k];recovered=1;
  }
  if(blocked){
   if(recovered){for(k=0;k<3;k++)target[k]=safe[k];}
   else{target[0]=routeX;target[1]=routeY;target[2]=intendedZ;}
   hit=1;p->vel.z=0;p->knockback.z=0;
  }
 }
 if(SFI_Floor(target[0],target[2],fmaxf(sImportedPrevious[1],target[1])+SFI_Radius(),&floor)){
  gGroundHeight=floor;p->pathFloor=floor+(SFI_NativeFlight()?40:50)-p->yPath;
  if(target[1]<floor+(SFI_NativeFlight()?40:50)){target[1]=floor+(SFI_NativeFlight()?40:50);if(p->vel.y<0)p->vel.y=0;if(p->knockback.y<0)p->knockback.y=0;}
 }
 p->pos.x=target[0];p->pos.y=target[1];p->pos.z+=target[2]-p->trueZpos;p->trueZpos=target[2];
 if(hit){if(p->mercyTimer==0)Player_ApplyDamage(p,1,12);sImportedContacts++;SFI_Report(gPathProgress,-1,sImportedContacts);}
}

void StarForge_ImportedShot(void* object,const float* previous){
 PlayerShot*s=(PlayerShot*)object;float point[3],fix[3],dx,dy,dz,distance,t;int i,steps;
 if(!StarForge_ImportedActive()||s->obj.status!=SHOT_ACTIVE)return;
 if(s->obj.id!=PLAYERSHOT_SINGLE_LASER&&s->obj.id!=PLAYERSHOT_TWIN_LASER&&s->obj.id!=PLAYERSHOT_BOMB&&s->obj.id!=PLAYERSHOT_LOCK_ON)return;
 dx=s->obj.pos.x-previous[0];dy=s->obj.pos.y-previous[1];dz=s->obj.pos.z-previous[2];distance=sqrtf(dx*dx+dy*dy+dz*dz);
 steps=(int)(distance/20)+1;if(steps>256)return;
 for(i=1;i<=steps;i++){t=(float)i/steps;point[0]=previous[0]+dx*t;point[1]=previous[1]+dy*t;point[2]=previous[2]+dz*t;
  if(SFI_Collide(point,12,fix)){gShotHitPosX=point[0];gShotHitPosY=point[1];gShotHitPosZ=point[2];PlayerShot_Impact(s);break;}
 }
}

int StarForge_ImportedCamera(void* object){
 Player*p=(Player*)object;float ref,x,y,dx,dy,desiredArm,followY;
 if(!StarForge_ImportedActive()||SFI_NativeFlight()||p->state!=PLAYERSTATE_ACTIVE||p->somersault)return 0;
 ref=p->trueZpos+gPathProgress;dx=p->pos.x-p->xPath;dy=p->pos.y-p->yPath;SFI_Route(gPathProgress+SFI_Camera(3),&x,&y);
 /* Imported route keys explicitly define any camera panning. */
 followY=.815f-.035f*tanhf(dy/120.f);
 p->cam.eye.x=p->xPath+dx*.777f;p->cam.eye.y=p->yPath+dy*followY+SFI_Camera(1);p->cam.eye.z=ref+SFI_Camera(0);
 desiredArm=SFI_Camera(0);
 /* Shorten the camera arm when imported scenery blocks it. */
 {float desired[3]={p->cam.eye.x,p->cam.eye.y,p->cam.eye.z},point[3],correction[3],t,last=0;int step;
  for(step=1;step<=32;step++){t=step/32.f;point[0]=p->pos.x+(desired[0]-p->pos.x)*t;point[1]=p->pos.y+(desired[1]-p->pos.y)*t;point[2]=p->trueZpos+(desired[2]-ref)*t;
   if(SFI_Collide(point,12,correction)){t=fmaxf(.05f,last-.02f);desiredArm=SFI_Camera(0)*t;break;}last=t;
  }
 }
 if(desiredArm<sImportedArm)sImportedArm=desiredArm;else sImportedArm=fminf(desiredArm,sImportedArm+4);
 {float t=sImportedArm/SFI_Camera(0);p->cam.eye.x=p->pos.x+(p->cam.eye.x-p->pos.x)*t;p->cam.eye.y=p->pos.y+(p->cam.eye.y-p->pos.y)*t;p->cam.eye.z=ref+sImportedArm;}
 p->cam.at.x=x+dx*.777f;p->cam.at.y=y+dy*followY+SFI_Camera(2);p->cam.at.z=ref-SFI_Camera(3);
 Math_SmoothStepToF(&p->camRoll,0,.2f,2.f,0);return 1;
}
