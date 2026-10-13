#include "StarForgeRuntime.h"
#include "StarForgeNative.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <libultraship/src/public/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <map>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <algorithm>
struct NativeObject{float z;int16_t z2,x,y,rx,ry,rz,id;};
static_assert(sizeof(NativeObject)==20,"ObjectInit ABI mismatch");
struct Override{float z;int id,x,y,hp,points;};
static std::map<int,std::vector<NativeObject>> sections;
static std::map<int,std::vector<unsigned short>> scripts;
static std::vector<Override> overrides;
struct DialogueOverride{int speaker,voice;std::string text;bool custom;std::vector<unsigned short> codes;};
static std::map<std::pair<int,int>,DialogueOverride> dialogueOverrides;
static std::map<unsigned short*,int> dialogueVoices;
struct PathPoint{float x,y,z;};
struct InstanceSettings{float z;int id,x,y,z2;float skew[3]={0,0,0};float scale=1,birdSpeed=-1,duration=8;bool movesWithStage=false;std::string uid,target,assemblyParent;float assemblyOffset[3]={0,0,0},assemblyRoll=0;bool react=false,reactDamage=true;float reactDistance=1000;int behavior=0;float flightSpeed=1200,flightDuration=16;std::vector<PathPoint> path;};
struct LiveInstance{const InstanceSettings* settings;int frame;float progress;int reactFrame=-1;bool reactDamaged=false;std::vector<float> hitbox;float* originalHitbox=nullptr;};
static std::vector<InstanceSettings> instanceSettings;
static std::map<void*,LiveInstance> liveInstances;
static int stage=-1,musicSequence=-1;
extern "C" void StarForge_MusicLoad(int enabled);
using nlohmann::json;
static int arrivalOrigin=-1;
static float Bound(const json& j,const char* key,float lo,float hi){float v=j.at(key).get<float>();if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(key);return v;}
extern "C" void StarForge_NativeLoad(void){StarForge_PresentationReload();sections.clear();scripts.clear();overrides.clear();dialogueOverrides.clear();dialogueVoices.clear();instanceSettings.clear();liveInstances.clear();stage=-1;arrivalOrigin=-1;musicSequence=-1;StarForge_MusicLoad(0);try{
 std::ifstream f("StarForge/project.json");if(!f)return;auto j=json::parse(f);StarForge_GroupLoad(j.dump().c_str());if(j.value("schema","")=="starforge.level.v1"){if(j.contains("music")){auto&m=j["music"];if(m.value("type",std::string())=="native"){musicSequence=m.at("sequence").get<int>();if(musicSequence<2||musicSequence>16)throw std::runtime_error("music sequence");if(musicSequence!=14)musicSequence|=32768;}else if(m.value("type",std::string())=="custom"){musicSequence=65535;StarForge_MusicLoad(1);}stage=0;}return;}if(j.value("schema","")!="starforge.native.v1")return;
 int id=j.at("levelId").get<int>();if(id==2&&j.contains("encounterOrigin")){const auto origin=j["encounterOrigin"].get<std::string>();arrivalOrigin=origin=="katina"?1:origin=="fortuna"?0:-1;}if(id!=0&&id!=1&&id!=2&&id!=3&&id!=5&&id!=6&&id!=7&&id!=8)throw std::runtime_error("stage");
 if(j["sections"].size()>2||j["sections"].empty())throw std::runtime_error("sections");
 for(auto&s:j["sections"]){int phase=s.at("phase").get<int>();if(phase<0||phase>1||s["placements"].size()>10000)throw std::runtime_error("phase or count");auto& target=sections[phase];
 for(auto&p:s["placements"]){NativeObject o={};o.z=Bound(p,"zPos1",-10000,1000000);o.z2=Bound(p,"zPos2",-32768,32767);o.x=Bound(p,"x",-32768,32767);o.y=Bound(p,"y",-32768,32767);o.rx=Bound(p,"rotX",-32768,32767);o.ry=Bound(p,"rotY",-32768,32767);o.rz=Bound(p,"rotZ",-32768,32767);o.id=Bound(p,"id",0,1199);target.push_back(o);
 int hp=p.value("hpOverride",-1),points=p.value("pointsOverride",-1);if(hp< -1||hp>511||points< -1||points>100)throw std::runtime_error("HP or points");if(hp>=0||points>=0)overrides.push_back({o.z,o.id,o.x,o.y,hp,points});InstanceSettings settings{};settings.z=o.z;settings.z2=o.z2;settings.id=o.id;settings.x=o.x;settings.y=o.y;settings.uid=p.value("uid",std::string());settings.assemblyParent=p.value("assemblyParentUid",std::string());if(p.contains("assemblyOffset")){for(int i=0;i<3;i++)settings.assemblyOffset[i]=p["assemblyOffset"].at(i).get<float>();settings.assemblyRoll=p.value("assemblyRoll",0.0f);}if(p.contains("flightBehavior")){auto& b=p["flightBehavior"];std::string type=b.value("type",std::string());settings.react=type=="react";settings.reactDamage=b.value("trigger",std::string("damage"))=="damage";settings.reactDistance=b.value("distance",1000.0f);settings.behavior=type=="chase"?1:type=="ram"?2:0;settings.target=b.value("targetId",std::string());if(settings.behavior){settings.flightSpeed=Bound(b,"speed",1,6000);settings.flightDuration=Bound(b,"duration",.1f,120);}}if(p.contains("skewAngles")){if(!p["skewAngles"].is_array()||p["skewAngles"].size()!=3)throw std::runtime_error("skew");for(int i=0;i<3;i++){float v=p["skewAngles"][i].get<float>();if(!std::isfinite(v)||std::abs(v)>60)throw std::runtime_error("skew");settings.skew[i]=std::tan(v*3.14159265f/180.0f);}}settings.scale=p.value("sizePercent",100.0f)/100.0f;if(!std::isfinite(settings.scale)||settings.scale<0||settings.scale>3)throw std::runtime_error("size");if(p.contains("nativeParameters"))settings.birdSpeed=p["nativeParameters"].value("birdSpeed",-1.0f);if(p.contains("customPath")){auto&path=p["customPath"];settings.duration=Bound(path,"duration",.1f,120);settings.movesWithStage=path.value("movesWithStage",false);if(path["points"].size()<2||path["points"].size()>4096)throw std::runtime_error("path points");for(auto&point:path["points"])settings.path.push_back({Bound(point,"x",-32768,32767),Bound(point,"y",-32768,32767),Bound(point,"z",-1000000,1000000)});}if(settings.skew[0]!=0||settings.skew[1]!=0||settings.skew[2]!=0||settings.scale!=1||!settings.path.empty()||settings.birdSpeed>=0||!settings.uid.empty())instanceSettings.push_back(std::move(settings));}
 NativeObject end={};end.id=-1;target.push_back(end);}
 if(j["scripts"].size()>512)throw std::runtime_error("scripts");for(auto it=j["scripts"].begin();it!=j["scripts"].end();++it){int key=std::stoi(it.key());if(key<0||key>=512)throw std::runtime_error("script id");auto& commands=it.value()["commands"];if(commands.size()>2000)throw std::runtime_error("script length");int command=0;for(auto&c:commands){auto raw=c.at("raw");if(raw.size()!=2)throw std::runtime_error("command");for(auto word:raw){int w=word.get<int>();if(w<0||w>65535)throw std::runtime_error("word");scripts[key].push_back(w);}if(c.contains("dialogue")){auto&d=c["dialogue"];DialogueOverride value{};value.speaker=d.value("speaker",int(raw[0])&511);value.voice=d.value("voiceId",int(raw[1]));value.text=d.value("text",std::string());value.custom=d.value("textMode",std::string("catalog"))=="custom";if(value.speaker<0||value.speaker>511||value.voice< -1||value.voice>65535||value.text.size()>200)throw std::runtime_error("dialogue");dialogueOverrides[{key,command}]=std::move(value);}command++;}}
 if(j.contains("music")){auto&m=j["music"];if(m.value("type",std::string())=="native"){musicSequence=m.at("sequence").get<int>();if(musicSequence<2||musicSequence>16)throw std::runtime_error("music sequence");if(musicSequence!=14)musicSequence|=32768;}else if(m.value("type",std::string())=="custom"){musicSequence=65535;StarForge_MusicLoad(1);}}stage=id;std::ofstream("StarForge/load-status.txt")<<"Original stage project loaded: "<<j.value("stage","")<<", "<<sections.size()<<" rail sections, "<<scripts.size()<<" preserved scripts.\n";
 }catch(const std::exception&e){sections.clear();scripts.clear();overrides.clear();dialogueOverrides.clear();dialogueVoices.clear();instanceSettings.clear();liveInstances.clear();stage=-1;std::ofstream("StarForge/load-status.txt")<<"Native project rejected: "<<e.what()<<". Original game data retained.\n";}}
extern "C" void* StarForge_NativeObjects(int level,int phase){auto it=sections.find(phase);return level==stage&&it!=sections.end()?it->second.data():nullptr;}
extern "C" unsigned short* StarForge_NativeScript(int level,int script){auto it=scripts.find(script);return level==stage&&it!=scripts.end()&&!it->second.empty()?it->second.data():nullptr;}
extern "C" void StarForge_NativeOverride(int level,float z,int id,int x,int y,int*hp,int*points){*hp=*points=-1;if(level!=stage)return;for(auto&o:overrides)if(o.z==z&&o.id==id&&o.x==x&&o.y==y){*hp=o.hp;*points=o.points;return;}}
extern "C" unsigned short* StarForge_NativeDialogue(int level,int script,int command,int* speaker,unsigned short* original){if(level!=stage)return original;auto it=dialogueOverrides.find({script,command});if(it==dialogueOverrides.end())return original;auto&d=it->second;*speaker=d.speaker;if(d.codes.empty()){if(d.custom){d.codes.push_back(8);int column=0;for(size_t i=0;i<d.text.size();i++){char ch=d.text[i];if(ch=='\n'){d.codes.push_back(1);column=0;continue;}if(ch==' '){size_t next=d.text.find_first_of(" \n",i+1);size_t word=(next==std::string::npos?d.text.size():next)-i-1;if(column&&column+1+word>18){d.codes.push_back(1);column=0;continue;}}if(column>=18){d.codes.push_back(1);column=0;}column++;unsigned short code=ch>='A'&&ch<='Z'?ch-'A'+24:ch>='a'&&ch<='z'?ch-'a'+50:ch>='0'&&ch<='9'?ch-'0'+81:ch=='!'?76:ch=='?'?77:ch=='-'?78:ch==','?79:ch=='.'?80:ch=='\''?91:12;d.codes.push_back(code);}d.codes.push_back(0);}else if(original){for(int i=0;i<512;i++){d.codes.push_back(original[i]);if(original[i]==0)break;}if(d.codes.back()!=0)d.codes.push_back(0);}else return nullptr;}auto*result=d.codes.data();dialogueVoices[result]=d.voice;return result;}
extern "C" int StarForge_NativeVoiceId(unsigned short* message){auto it=dialogueVoices.find(message);return it==dialogueVoices.end()?-2:it->second;}
extern "C" void StarForge_NativeRegister(void*object,float z,int id,int x,int y,int z2,int frame,float progress,int level){liveInstances.erase(object);if(level!=stage)return;for(auto&settings:instanceSettings)if(settings.z==z&&settings.id==id&&settings.x==x&&settings.y==y&&settings.z2==z2&&!std::any_of(liveInstances.begin(),liveInstances.end(),[&](const auto&entry){return entry.second.settings==&settings;})){liveInstances[object]={&settings,frame,progress};StarForge_GroupBind(object,settings.uid.c_str());return;}}
extern "C" float StarForge_RemixScale(void*object);
extern "C" float StarForge_NativeScale(void*object){auto it=liveInstances.find(object);return it==liveInstances.end()?(CVarGetInteger("gCorneriaRemix",0)?StarForge_RemixScale(object):1):it->second.settings->scale;}
extern "C" float StarForge_NativeBirdSpeed(void*object,float original){auto it=liveInstances.find(object);return it==liveInstances.end()||it->second.settings->birdSpeed<0?original:it->second.settings->birdSpeed;}
static PathPoint SamplePath(const std::vector<PathPoint>&points,float t){float value=std::max(0.0f,std::min(1.0f,t))*(points.size()-1);int i=std::min(int(points.size())-2,int(value));float u=value-i;auto p0=points[std::max(0,i-1)],p1=points[i],p2=points[i+1],p3=points[std::min(int(points.size())-1,i+2)];auto axis=[u](float a,float b,float c,float d){return .5f*((2*b)+(-a+c)*u+(2*a-5*b+4*c-d)*u*u+(-a+3*b-3*c+d)*u*u*u);};return {axis(p0.x,p1.x,p2.x,p3.x),axis(p0.y,p1.y,p2.y,p3.y),axis(p0.z,p1.z,p2.z,p3.z)};}
extern "C" int StarForge_NativeMove(void*object,float*x,float*y,float*z,float*rx,float*ry,int frame,float progress){auto it=liveInstances.find(object);if(it==liveInstances.end()||it->second.settings->path.empty())return 0;auto&instance=it->second;int start=instance.frame;if(instance.settings->react){if(instance.reactFrame<0&&(instance.settings->reactDamage?instance.reactDamaged:std::abs(*z+progress)<=instance.settings->reactDistance))instance.reactFrame=frame;start=instance.reactFrame<0?frame:instance.reactFrame;}float t=(frame-start)/(30.0f*instance.settings->duration);if(t>1)return 2;auto point=SamplePath(instance.settings->path,t),next=SamplePath(instance.settings->path,std::min(1.0f,t+.001f));*x=point.x;*y=point.y;*z=-((instance.settings->movesWithStage||instance.settings->react)?instance.progress:progress)-point.z;float dx=next.x-point.x,dy=next.y-point.y,dz=point.z-next.z;if(std::abs(dx)+std::abs(dy)+std::abs(dz)>.00001f){*ry=std::atan2(dx,dz)*57.2957795f;*rx=-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*57.2957795f;}return 1;}
extern "C" float* StarForge_NativeHitbox(void*object,float*original){auto it=liveInstances.find(object);if(it==liveInstances.end()||it->second.settings->scale==1||!original)return original;auto&instance=it->second;if(original==instance.hitbox.data())return original;if(instance.originalHitbox==original&&!instance.hitbox.empty())return instance.hitbox.data();int count=int(original[0]);if(count<0||count>100)return original;int length=1;for(int i=0;i<count;i++){if(original[length]>=300000)return original;if(original[length]==200000)length+=4;length+=6;if(length>1001)return original;}instance.originalHitbox=original;instance.hitbox.assign(original,original+length);int index=1;for(int i=0;i<count;i++){if(instance.hitbox[index]==200000)index+=4;for(int j=0;j<6;j++)instance.hitbox[index+j]*=instance.settings->scale;index+=6;}return instance.hitbox.data();}

extern "C" int StarForge_NativeMusicSequence(int level,int sequence){return level==stage&&musicSequence>=0&&(sequence&255)>=2&&(sequence&255)<=16?musicSequence:sequence;}
extern "C" int StarForge_NativeMusicActive(int level){return level==stage&&musicSequence==65535;}

extern "C" void StarForge_SetMessageVoice(unsigned short* message,int voice){dialogueVoices[message]=voice;}

extern "C" int StarForge_NativeRouteCondition(int level,int condition,int fallback){if(level!=2||stage!=2||arrivalOrigin<0)return fallback;if(condition==50)return arrivalOrigin==0;if(condition==55)return arrivalOrigin==1;return fallback;}

extern "C" int StarForge_RemixSkew(void* object,float* shear);
extern "C" int StarForge_ObjectSkew(void* object,float* shear){auto it=liveInstances.find(object);if(it==liveInstances.end())return CVarGetInteger("gCorneriaRemix",0)?StarForge_RemixSkew(object,shear):0;for(int i=0;i<3;i++)shear[i]=it->second.settings->skew[i];return shear[0]!=0||shear[1]!=0||shear[2]!=0;}

static bool presentationRead=false,venomCourseWasSelected=false;static int presentationLevel=-1,presentationMedal=-1;static std::string presentationName,presentationSubtitle,presentationNameCase,presentationSubtitleCase;
static std::string PresentationText(std::string value){std::string result;for(unsigned char c:value){if(c>='a'&&c<='z')c-=32;if((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c=='-')result.push_back(c);if(result.size()==40)break;}return result;}
static std::string PresentationCaseText(const std::string& value){std::string result;for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c=='-')result.push_back(c);if(result.size()==40)break;}return result;}
extern "C" void StarForge_PresentationReload(void){presentationRead=true;presentationLevel=presentationMedal=-1;presentationName.clear();presentationSubtitle.clear();presentationNameCase.clear();presentationSubtitleCase.clear();try{std::ifstream file("StarForge/project.json");if(!file)return;auto j=nlohmann::json::parse(file);if(!j.contains("presentation"))return;if(j.value("schema",std::string())=="starforge.native.v1")presentationLevel=j.at("levelId").get<int>();else if(j.value("schema",std::string())=="starforge.level.v1"){static const std::map<std::string,int> levels={{"Corneria",0},{"Meteo",1},{"Sector X",2},{"Area 6",3},{"Sector Y",5},{"Venom 1",6},{"Zoness",8}};auto it=levels.find(j.value("playtestStage",std::string("Corneria")));if(it==levels.end())return;presentationLevel=it->second;}else return;auto& p=j["presentation"];presentationNameCase=PresentationCaseText(p.value("name",std::string()));presentationSubtitleCase=PresentationCaseText(p.value("subtitle",std::string()));presentationName=PresentationText(presentationNameCase);presentationSubtitle=PresentationText(presentationSubtitleCase);if(p.contains("medalHits")&&p["medalHits"].is_number_integer()){int hits=p["medalHits"];if(hits>=0&&hits<=9999)presentationMedal=hits;}}catch(...){presentationLevel=presentationMedal=-1;presentationName.clear();presentationSubtitle.clear();presentationNameCase.clear();presentationSubtitleCase.clear();}}
extern "C" const char* StarForge_LevelName(int level){if(!presentationRead)StarForge_PresentationReload();if(level!=9)venomCourseWasSelected=level==6&&presentationLevel==6;return level==presentationLevel&&!presentationName.empty()?presentationName.c_str():nullptr;}
extern "C" const char* StarForge_LevelSubtitle(int level){if(!presentationRead)StarForge_PresentationReload();return level==presentationLevel&&!presentationSubtitle.empty()?presentationSubtitle.c_str():nullptr;}
extern "C" int StarForge_MedalRequirement(int level,int fallback){if(!presentationRead)StarForge_PresentationReload();return (level==presentationLevel||(level==9&&presentationLevel==6&&venomCourseWasSelected))&&presentationMedal>=0?presentationMedal:fallback;}

struct PresentationRaster{int width=0,height=0;std::vector<unsigned char> pixels;};
static std::map<std::string,PresentationRaster> presentationRasters;
extern "C" unsigned char* StarForge_LevelTextTexture(int level,int subtitle,int* width,int* height){
    if(!presentationRead)StarForge_PresentationReload();
    if(level!=presentationLevel)return nullptr;
    const std::string& text=subtitle?presentationSubtitleCase:presentationNameCase;
    if(text.empty())return nullptr;
#ifdef _WIN32
    auto existing=presentationRasters.find(text);
    if(existing==presentationRasters.end()){
        HDC dc=CreateCompatibleDC(nullptr);if(!dc)return nullptr;
        HFONT font=CreateFontA(-14,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial");
        if(!font){DeleteDC(dc);return nullptr;}
        HGDIOBJ oldFont=SelectObject(dc,font);SIZE size={};GetTextExtentPoint32A(dc,text.c_str(),(int)text.size(),&size);
        int w=(size.cx+15)&~7,h=size.cy+8;BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!bitmap||!bits){SelectObject(dc,oldFont);DeleteObject(font);DeleteDC(dc);return nullptr;}
        HGDIOBJ oldBitmap=SelectObject(dc,bitmap);memset(bits,0,w*h*4);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,255));TextOutA(dc,2,2,text.c_str(),(int)text.size());GdiFlush();
        auto rgb=(unsigned char*)bits;int left=w,top=h,right=-1,bottom=-1;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(rgb[(y*w+x)*4]>0){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);}
        PresentationRaster raster;if(right>=left){raster.width=((right-left+3)+7)&~7;raster.height=bottom-top+3;raster.pixels.resize(raster.width*raster.height);
            for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++){int alpha=(rgb[(y*w+x)*4]+8)/17;raster.pixels[(y-top+1)*raster.width+x-left+1]=(unsigned char)(0xF0|alpha);}}
        SelectObject(dc,oldBitmap);DeleteObject(bitmap);SelectObject(dc,oldFont);DeleteObject(font);DeleteDC(dc);
        existing=presentationRasters.emplace(text,std::move(raster)).first;
    }
    if(existing->second.pixels.empty())return nullptr;
    *width=existing->second.width;*height=existing->second.height;return existing->second.pixels.data();
#else
    return nullptr;
#endif
}

extern "C" const char* StarForge_NativeUid(void*object){auto it=liveInstances.find(object);return it==liveInstances.end()?nullptr:it->second.settings->uid.c_str();}
extern "C" int StarForge_NativeBehavior(void*object,int frame,const char**target,float*speed){auto it=liveInstances.find(object);if(it==liveInstances.end()||!it->second.settings->behavior)return 0;auto&s=*it->second.settings;if((frame-it->second.frame)/30.0f>s.flightDuration)return 3;*target=s.target.c_str();*speed=s.flightSpeed;return s.behavior;}

extern "C" void StarForge_NativeReactDamage(void*object){auto it=liveInstances.find(object);if(it!=liveInstances.end())it->second.reactDamaged=true;}

extern "C" void* StarForge_NativeAttachment(void*object,float*offset,float*roll){auto it=liveInstances.find(object);if(it==liveInstances.end()||it->second.settings->assemblyParent.empty())return nullptr;auto&s=*it->second.settings;for(auto&entry:liveInstances)if(entry.second.settings->uid==s.assemblyParent){auto&parent=*entry.second.settings;float x=s.assemblyOffset[0],y=s.assemblyOffset[1],z=s.assemblyOffset[2];offset[0]=(x+y*parent.skew[0])*parent.scale;offset[1]=(y+z*parent.skew[1])*parent.scale;offset[2]=(z+x*parent.skew[2])*parent.scale;*roll=s.assemblyRoll;return entry.first;}return object;}
