#include "StarForgeRuntime.h"
#include "StarForgeNative.h"
#include "StarForgeFlightTiming.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <array>
#include <map>
static std::vector<CR_Encounter> waves;
static std::vector<std::vector<unsigned short>> originalEnemyScripts;
static std::unordered_map<int,std::vector<unsigned short>> originalAuxScripts;static std::unordered_map<int,std::unordered_map<int,int>> originalScopeTargets;static std::unordered_map<int,int> originalScriptScope;
static std::vector<CR_Setting> scenery;
static std::vector<std::string> names;
static nlohmann::json project;
static float sectionDuration=150;
static std::map<std::pair<int,int>,StarForgeFlight::Timing> pathTimingCache;
static std::map<int,StarForgeFlight::Actions> encounterActionCache;
static bool pathProgressReady=false;static float pathProgress=0,pathProgressOrigin=0;
static std::unordered_map<void*,float> remixScales;
static std::unordered_map<void*,std::array<float,3>> remixSkews;
static std::vector<bool> eventDone,itemDone;
static std::vector<float> triggerOrigin;
static std::vector<std::vector<unsigned short>> customMessages;
extern "C" void StarForge_AddItem(int,float,float,float,int);
extern "C" void StarForge_PlayRadio(int,int,unsigned short*);
extern "C" void StarForge_PlayRadioVoice(int,int,unsigned short*,int);
extern "C" int StarForge_RadioBusy(void);
extern "C" void StarForge_Flyby(int,int,int,float,float,int);
extern "C" void StarForge_Supply(void);
using nlohmann::json;
extern "C" void StarForge_GroupLoad(const char*);
static float Number(const json& j,const char* key,float min,float max){float v=j.at(key).get<float>();if(!std::isfinite(v)||v<min||v>max)throw std::runtime_error(key);return v;}
// Environment choices are read at mission initialization, before the remix spawn clock starts.
// They must not depend on StarForge_Load(), which runs only after player control begins.
static int environmentFloor=-1,environmentBackdrop=-1;
static int EnvironmentId(const json& value){
 if(!value.is_string())return -1;
 static const std::map<std::string,int> ids={{"Corneria",0},{"Meteo",1},{"Sector X",2},{"Area 6",3},{"Sector Y",5},{"Venom",6},{"Venom 1",6},{"Solar",7},{"Zoness",8},{"Macbeth",11},{"Titania",12},{"Aquas",13},{"Fortuna",14},{"Katina",16},{"Bolse",17},{"Sector Z",18}};
 auto found=ids.find(value.get<std::string>());return found==ids.end()?-1:found->second;
}
extern "C" void StarForge_EnvironmentLoad(int enabled){
 environmentFloor=environmentBackdrop=-1;
 if(!enabled)return;
 try{std::ifstream file("StarForge/project.json");if(!file)return;auto j=json::parse(file);
  if(j.value("schema",std::string())!="starforge.level.v1"||!j.contains("environment"))return;
  auto& e=j.at("environment");int floor=EnvironmentId(e.at("floor")),backdrop=EnvironmentId(e.at("backdrop"));
  if(floor<0||backdrop<0)throw std::runtime_error("Unknown floor or backdrop");
  environmentFloor=floor;environmentBackdrop=backdrop;
  std::ofstream("StarForge/environment-status.txt")<<"Editor environment: floor "<<floor<<", backdrop "<<backdrop<<". Mission selection is unchanged.\n";
 }catch(const std::exception& e){std::ofstream("StarForge/environment-status.txt")<<"Environment rejected: "<<e.what()<<"\n";}
}
extern "C" int StarForge_EnvironmentActive(void){return environmentFloor>=0&&environmentBackdrop>=0;}
extern "C" int StarForge_EnvironmentLevel(int fallback,int backdrop,int enabled){return enabled&&StarForge_EnvironmentActive()?(backdrop?environmentBackdrop:environmentFloor):fallback;}
extern "C" void StarForge_Load(void){encounterActionCache.clear();pathTimingCache.clear();pathProgressReady=false;remixScales.clear();remixSkews.clear();
 waves.clear();scenery.clear();names.clear();originalEnemyScripts.clear();originalAuxScripts.clear();originalScopeTargets.clear();originalScriptScope.clear();project=json::object();sectionDuration=150;
 try{
  std::ifstream file("StarForge/project.json");if(!file)throw std::runtime_error("Project file missing");
  auto j=json::parse(file);StarForge_GroupLoad(j.dump().c_str());if(j.at("schema")!="starforge.level.v1"||j.at("stage")!="Corneria")throw std::runtime_error("Unsupported project format");
  sectionDuration=j.contains("duration")?Number(j,"duration",1,600):150;
  if(j["waves"].size()>500||j["scenery"].size()>500)throw std::runtime_error("object count");
  std::unordered_map<std::string,int> scopes;int nextAux=1000;
  originalEnemyScripts.resize(j["waves"].size());names.reserve(j["waves"].size());waves.reserve(j["waves"].size());
  for(auto& w:j["waves"]){
   if(w.contains("nativeObject")){auto& n=w["nativeObject"];int id=n.at("id").get<int>();const int allowed[]={176,177,178,180,181,183,184,185,186,187,192,193,201,202,215,216,217,218,225,226,227,229,230,231,232,233,235,238,240,242,243,244,246,247,250,251,252,259,261,262,263,266,267,269,270,273,275,276,277,280,281,282,283};if(std::find(std::begin(allowed),std::end(allowed),id)==std::end(allowed))throw std::runtime_error("Unsupported native actor");if(w.value("script",0)!=200+id && !(id==235&&w.value("script",0)==279))throw std::runtime_error("Native actor identity mismatch");if(n.contains("parameters")&&n["parameters"].contains("birdSpeed"))Number(n["parameters"],"birdSpeed",0,100);}
   names.push_back(w.at("name").get<std::string>());if(names.back().size()>400)throw std::runtime_error("name length");
   CR_Encounter e={};e.time=Number(w,"time",0,sectionDuration);e.script=Number(w,"script",140,493);if(e.script>307&&!w.contains("nativeObject")&&!(e.script==400&&w.contains("nativeMovement")))throw std::runtime_error("Unsupported extended enemy script");if((e.script>=164&&e.script<200)||e.script==143||e.script==144||e.script==145)throw std::runtime_error("enemy script");
   e.mode=Number(w,"mode",0,29);e.count=Number(w,"count",1,12);e.x=Number(w,"x",-32768,32767);e.y=Number(w,"y",-32768,32767);e.gap=Number(w,"gap",150,700);e.depth=Number(w,"depth",-30000,30000);e.speed=Number(w,"speed",-3000,3000);e.setPiece=Number(w,"setPiece",0,3);e.name=names.back().c_str();if(w.contains("formationOffsets")){auto& offsets=w["formationOffsets"];if(!offsets.is_array()||offsets.size()>12)throw std::runtime_error("formation count");for(auto& p:offsets){Number(p,"x",-60000,60000);Number(p,"y",-60000,60000);Number(p,"z",-60000,60000);}}
   if(w.contains("nativeSpawnRotation")){auto& r=w["nativeSpawnRotation"];if(!r.is_array()||r.size()!=3)throw std::runtime_error("spawn rotation");for(auto& v:r){float a=v.get<float>();if(!std::isfinite(a)||std::abs(a)>360)throw std::runtime_error("spawn rotation");}}
   if(w.contains("nativeMovement")&&!w.contains("path")){auto& commands=w["nativeMovement"]["commands"];if(!commands.is_array()||commands.size()>2000)throw std::runtime_error("movement script length");auto& buffer=originalEnemyScripts[waves.size()];if(w["nativeMovement"].contains("scripts")){auto& book=w["nativeMovement"]["scripts"];std::string key=book.dump();int scope;if(scopes.count(key))scope=scopes[key];else{scope=(int)scopes.size();scopes[key]=scope;for(auto& pair:book.items()){int token=nextAux++;if(token>32767)throw std::runtime_error("movement script capacity");auto& target=originalAuxScripts[token];for(auto& c:pair.value()["commands"])for(auto& word:c.at("raw"))target.push_back(word.get<unsigned short>());originalScopeTargets[scope][std::stoi(pair.key())]=token;originalScriptScope[token]=scope;}}originalScriptScope[400+(int)waves.size()]=scope;}for(auto& command:commands){auto& raw=command.at("raw");if(raw.size()!=2)throw std::runtime_error("movement command");for(auto& word:raw){int value=word.get<int>();if(value<0||value>65535)throw std::runtime_error("movement word");buffer.push_back(value);}}}
   e.overrides=1;e.hp=w.contains("hp")?Number(w,"hp",0,511):10;e.points=w.contains("points")?Number(w,"points",0,100):1;waves.push_back(e);
  }
  for(auto& p:j["scenery"]){if(p.contains("depth"))Number(p,"depth",-30000,30000);if(p.contains("y"))Number(p,"y",-32768,32767);CR_Setting s={};s.time=Number(p,"time",0,sectionDuration);s.left=Number(p,"left",-1,160);s.right=Number(p,"right",-1,160);s.xLeft=Number(p,"xLeft",-32767,32767);s.xRight=Number(p,"xRight",-32767,32767);s.yawLeft=Number(p,"yawLeft",0,360);s.yawRight=Number(p,"yawRight",0,360);scenery.push_back(s);}
  std::stable_sort(waves.begin(),waves.end(),[](const auto&a,const auto&b){return a.time<b.time;});std::stable_sort(scenery.begin(),scenery.end(),[](const auto&a,const auto&b){return a.time<b.time;});
  project=j;project["events"]=j.value("events",json::array());project["items"]=j.value("items",json::array());
  if(project["events"].size()>500||project["items"].size()>500)throw std::runtime_error("event count");
  const int portraits[]={0,10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,210,220,230};
  for(auto& e:project["events"]){Number(e,"time",0,sectionDuration);auto type=e.value("type",std::string("dialogue"));if(type!="dialogue"&&type!="supply"&&type!="flyby"&&type!="trigger")throw std::runtime_error("event type");
   int speaker=e.value("speaker",30);if(std::find(std::begin(portraits),std::end(portraits),speaker)==std::end(portraits))throw std::runtime_error("portrait");
   if(e.value("text",std::string()).size()>200)throw std::runtime_error("dialogue length");
   for(const char* k:{"x","y","z","width","height","duration"})if(e.contains(k))Number(e,k,k[0]=='x'?-4000:0,k[0]=='z'?12000:4000);
   if(e.contains("ship"))Number(e,"ship",1,5);
  }
  for(auto& p:project["items"]){Number(p,"time",0,sectionDuration);Number(p,"x",-32768,32767);Number(p,"y",50,1600);Number(p,"z",1000,12000);int id=p.at("item").get<int>();if(id!=322&&id!=323&&id!=324&&id!=327&&id!=335&&id!=336&&id!=337)throw std::runtime_error("item type");}
  for(const char* list:{"waves","events"})for(auto& o:project[list])if(o.contains("path")){if(!o["path"].is_array()||o["path"].size()>4096)throw std::runtime_error("path count");for(auto& p:o["path"]){Number(p,"x",-1000000,1000000);Number(p,"y",-1000000,1000000);Number(p,"z",-1000000,1000000);}StarForgeFlight::Clock(0,std::max(.1f,o.value("duration",8.0f)),o["path"].size(),StarForgeFlight::Parse(o.value("pathTiming",json()),o["path"].size()));if(o.contains("pathTiming")&&o.contains("memberPaths"))for(const auto& member:o["memberPaths"])StarForgeFlight::Clock(0,std::max(.1f,o.value("duration",8.0f)),member.size(),StarForgeFlight::Parse(o["pathTiming"],member.size()));}
  for(auto& w:project["waves"])if(w.contains("memberPaths")){auto& paths=w["memberPaths"];if(!paths.is_array()||paths.size()!=w.at("count").get<int>())throw std::runtime_error("member path count");for(auto& nodes:paths){if(!nodes.is_array()||nodes.size()<2||nodes.size()>4096)throw std::runtime_error("member path points");for(auto& p:nodes){Number(p,"x",-1000000,1000000);Number(p,"y",-1000000,1000000);Number(p,"z",-1000000,1000000);}}}
  for(const char* list:{"waves","events"})for(auto& o:project[list])if(o.contains("encounterActions"))StarForgeFlight::ParseActions(o["encounterActions"],o.value("duration",8.0f));
  eventDone.assign(project["events"].size(),false);itemDone.assign(project["items"].size(),false);triggerOrigin.assign(eventDone.size(),-1);customMessages.clear();customMessages.resize(eventDone.size());
  std::ofstream("StarForge/load-status.txt")<<"Project loaded: "<<waves.size()<<" waves, "<<scenery.size()<<" scenery pairs.\n";
 }catch(const std::exception& e){waves.clear();scenery.clear();names.clear();originalEnemyScripts.clear();originalAuxScripts.clear();originalScopeTargets.clear();originalScriptScope.clear();project=json::object();sectionDuration=150;project["schema"]="starforge.level.v1";std::ofstream("StarForge/load-status.txt")<<"Project rejected: "<<e.what()<<". Playtest stopped; return to the editor and fix the project.\n";}
}
extern "C" const CR_Encounter* StarForge_Waves(int* count){*count=waves.size();static const CR_Encounter empty={};return waves.empty()?(project.contains("schema")?&empty:nullptr):waves.data();}
extern "C" const CR_Setting* StarForge_Scenery(int* count){*count=scenery.size();static const CR_Setting empty={};return scenery.empty()?(project.contains("schema")?&empty:nullptr):scenery.data();}
static void SampleRemixPath(const json& nodes,float u,float* point){float t=std::max(0.0f,std::min(1.0f,u))*(nodes.size()-1);int n=std::min((int)t,(int)nodes.size()-2);float f=t-n;const char* keys[]={"x","y","z"};for(int k=0;k<3;k++){float p0=nodes[std::max(0,n-1)].at(keys[k]).get<float>(),p1=nodes[n].at(keys[k]).get<float>(),p2=nodes[n+1].at(keys[k]).get<float>(),p3=nodes[std::min((int)nodes.size()-1,n+2)].at(keys[k]).get<float>();point[k]=.5f*(2*p1+(-p0+p2)*f+(2*p0-5*p1+4*p2-p3)*f*f+(-p0+3*p1-3*p2+p3)*f*f*f);}}
extern "C" int StarForge_EncounterFire(int entry,float age,float step){try{auto it=encounterActionCache.find(entry);if(it==encounterActionCache.end()){auto& o=entry>=10000?project.at("events").at(entry-10000):project.at("waves").at(entry);it=encounterActionCache.emplace(entry,StarForgeFlight::ParseActions(o.value("encounterActions",json()),o.value("duration",8.0f))).first;}return StarForgeFlight::Fire(it->second,age,age-std::max(.0001f,step));}catch(...){return 0;}}
extern "C" int StarForge_Path(int entry,float age,int lane,float* point){
 try{const json* o=nullptr;if(entry>=10000)o=&project.at("events").at(entry-10000);else o=&project.at("waves").at(entry);
 if(!o->contains("path")||(*o)["path"].size()<2)return 0;auto& nodes=(o->contains("memberPaths")&&lane>=0&&lane<(*o)["memberPaths"].size())?(*o)["memberPaths"][lane]:(*o)["path"];float duration=std::max(0.1f,o->value("duration",8.0f));auto key=std::make_pair(entry,o->contains("memberPaths")?lane:0);auto cached=pathTimingCache.find(key);if(cached==pathTimingCache.end())cached=pathTimingCache.emplace(key,StarForgeFlight::Parse(o->value("pathTiming",json()),nodes.size())).first;const auto& timing=cached->second;auto clock=StarForgeFlight::Clock(age,duration,nodes.size(),timing);
 SampleRemixPath(nodes,clock.u,point);
 if(o->contains("flightBehavior")&&(*o)["flightBehavior"].value("type",std::string())=="react")point[2]+=o->value("time",0.0f)*1200-(pathProgress-pathProgressOrigin);else if(timing.enabled)point[2]+=o->value("time",0.0f)*1200-(pathProgress-pathProgressOrigin);else if(o->contains("pathFrameLock"))point[2]+=(*o)["pathFrameLock"].value("distance",0.0f)-(pathProgress-pathProgressOrigin);if(!o->contains("memberPaths"))point[0]+=lane*o->value("gap",230.0f);return clock.ended?(timing.hold?(point[2]<-1500?2:1):2):1;
 }catch(...){return 0;}}
extern "C" int StarForge_WaveReady(int entry){try{std::string id=project.at("waves").at(entry).at("id");for(size_t i=0;i<eventDone.size();i++){auto&e=project["events"][i];if(e.value("type","")=="trigger"&&e.value("targetWave","")==id)return eventDone[i];}}catch(...){}return 1;}
extern "C" void StarForge_Update(float time,float x,float y,float progress){pathProgress=progress;if(!pathProgressReady){pathProgressOrigin=progress-time*1200;pathProgressReady=true;}
 try{
 for(size_t i=0;i<itemDone.size();i++){auto&o=project["items"][i];if(!itemDone[i]&&time>=o.value("time",0.0f)){StarForge_AddItem(o.value("item",324),o.value("x",0.0f),o.value("y",350.0f),o.value("z",5000.0f),i);itemDone[i]=true;}}
 for(size_t i=0;i<eventDone.size();i++){auto&e=project["events"][i];if(eventDone[i]||time<e.value("time",0.0f))continue;std::string type=e.value("type","dialogue");
 if(type=="trigger"){if(triggerOrigin[i]<0)triggerOrigin[i]=progress+e.value("z",5000.0f);if(progress>=triggerOrigin[i]&&progress<triggerOrigin[i]+200&&std::fabs(x-e.value("x",0.0f))<e.value("width",600.0f)/2&&std::fabs(y-e.value("y",350.0f))<e.value("height",500.0f)/2)eventDone[i]=true;continue;}
 if(type=="flyby"){int shoot=e.contains("shoot")?(e["shoot"].is_boolean()?(int)e["shoot"].get<bool>():e["shoot"].get<int>()):0;StarForge_Flyby(i,10000+i,e.value("ship",1),e.value("x",0.0f),e.value("y",450.0f),shoot);eventDone[i]=true;continue;}
 if(StarForge_RadioBusy())continue;
 if(type=="supply")StarForge_Supply();
 if(e.value("line",-1)<0){auto& codes=customMessages[i];codes.clear();codes.push_back(8);int col=0;std::string text=e.value("text",std::string("Hello, Fox!"));if(text.size()>200)text.resize(200);for(size_t position=0;position<text.size();position++){char c=text[position];if(c=='\n'){codes.push_back(1);col=0;continue;}if(c==' '){size_t next=text.find_first_of(" \n",position+1);size_t word=(next==std::string::npos?text.size():next)-position-1;if(col&&col+1+word>18){codes.push_back(1);col=0;continue;}}if(col>=18){codes.push_back(1);col=0;}col++;unsigned short code=c>='A'&&c<='Z'?c-'A'+24:c>='a'&&c<='z'?c-'a'+50:c>='0'&&c<='9'?c-'0'+81:c=='!'?76:c=='?'?77:c=='-'?78:c==','?79:c=='.'?80:c=='\''?91:12;codes.push_back(code);}codes.push_back(0);StarForge_PlayRadioVoice(-1,e.value("speaker",30),codes.data(),e.value("voiceId",-1));}
 else StarForge_PlayRadioVoice(e.value("line",0),e.value("speaker",30),nullptr,e.value("voiceId",e.value("line",0)));
 eventDone[i]=true;
 }
 }catch(...){}
}

extern "C" void StarForge_RemixTransform(int kind,int entry,void* object,float* rotation){
 remixScales.erase(object);remixSkews.erase(object);try{const char* lists[]={"waves","scenery","events","items"};if(kind<0||kind>3||entry<0||!project.contains(lists[kind])||entry>=project[lists[kind]].size())return;auto& o=project[lists[kind]][entry];if(!o.contains("editorTransform"))return;auto&t=o["editorTransform"];float scale=t.value("sizePercent",100.0f)/100.0f;if(!std::isfinite(scale)||scale<0||scale>3)return;remixScales[object]=scale;if(t.contains("skewAngles")&&t["skewAngles"].is_array()&&t["skewAngles"].size()==3){std::array<float,3> shear={};for(int i=0;i<3;i++){float v=t["skewAngles"][i].get<float>();if(!std::isfinite(v)||std::abs(v)>60)return;shear[i]=std::tan(v*3.14159265f/180.0f);}remixSkews[object]=shear;}if(t.contains("rotation")&&t["rotation"].is_array()&&t["rotation"].size()==3)for(int i=0;i<3;i++){float angle=t["rotation"][i].get<float>();if(std::isfinite(angle))rotation[i]+=angle;}}catch(...){}
}
extern "C" float StarForge_RemixScale(void* object){auto it=remixScales.find(object);return it==remixScales.end()?1.0f:it->second;}

extern "C" int StarForge_RemixSkew(void* object,float* shear){auto it=remixSkews.find(object);if(it==remixSkews.end())return 0;for(int i=0;i<3;i++)shear[i]=it->second[i];return shear[0]!=0||shear[1]!=0||shear[2]!=0;}

extern "C" int StarForge_FormationOffset(int entry,int lane,float* point){try{auto& offsets=project.at("waves").at(entry).at("formationOffsets");if(lane<0||lane>=offsets.size())return 0;auto& p=offsets[lane];point[0]=p.at("x");point[1]=p.at("y");point[2]=p.at("z");return 1;}catch(...){return 0;}}

extern "C" unsigned short* StarForge_OriginalEnemyScript(int script){auto aux=originalAuxScripts.find(script);if(aux!=originalAuxScripts.end())return aux->second.data();int entry=script-400;return entry>=0&&entry<originalEnemyScripts.size()&&!originalEnemyScripts[entry].empty()?originalEnemyScripts[entry].data():nullptr;}
extern "C" int StarForge_UsesOriginalMovement(int entry){return !StarForge_RemixStatic(entry)&& entry>=0&&entry<originalEnemyScripts.size()&&!originalEnemyScripts[entry].empty()&&!project["waves"][entry].contains("flightBehavior")&&(!project["waves"][entry].contains("path")||project["waves"][entry]["path"].size()<2);}

extern "C" float StarForge_ClipDuration(int entry){try{return project.at("waves").at(entry).value("clipDuration",32.0f);}catch(...){return 32;}}

extern "C" float StarForge_SceneryDepth(int entry){try{return project.at("scenery").at(entry).value("depth",10000.0f);}catch(...){return 10000.0f;}}
extern "C" float StarForge_SceneryHeight(int entry,float fallback){try{return project.at("scenery").at(entry).value("y",fallback);}catch(...){return fallback;}}

extern "C" void StarForge_OriginalSpawnRotation(int entry,float* rotation){try{if(StarForge_RemixStatic(entry)&&!project.at("waves").at(entry).contains("nativeMovement"))return;auto& r=project.at("waves").at(entry).at("nativeSpawnRotation");for(int i=0;i<3;i++)rotation[i]=r.at(i).get<float>();}catch(...){}}

extern "C" float StarForge_SectionDuration(void){return sectionDuration;}

extern "C" int StarForge_IsDecorative(int entry){return entry>=0&&project.contains("waves")&&entry<(int)project["waves"].size()&&project["waves"][entry].value("decorative",false);}
extern "C" int StarForge_FacesPath(int entry){try{if(entry>=10000){auto& o=project.at("events").at(entry-10000);return o.value("type",std::string())=="flyby"&&o.contains("path")&&o["path"].size()>1;}return entry>=0&&entry<(int)project.at("waves").size()&&project["waves"][entry].value("facePath",false);}catch(...){return 0;}}

extern "C" int StarForge_ParentEntry(int entry){try{std::string id=project.at("waves").at(entry).value("parentId",std::string());if(id.empty())return -1;for(size_t i=0;i<project["waves"].size();i++)if(project["waves"][i].value("id",std::string())==id)return (int)i;}catch(...){}return -1;}

extern "C" int StarForge_PathOrientation(int entry,float age,int lane,float* rotation){try{auto& o=entry>=10000?project.at("events").at(entry-10000):project.at("waves").at(entry);auto& nodes=o.contains("memberPaths")&&lane>=0&&lane<o["memberPaths"].size()?o["memberPaths"][lane]:o["path"];if(nodes.size()<2)return 0;float duration=std::max(.1f,o.value("duration",8.0f));auto timing=StarForgeFlight::Parse(o.value("pathTiming",json()),nodes.size());auto clock=StarForgeFlight::Clock(age,duration,nodes.size(),timing);float p[3],q[3];SampleRemixPath(nodes,std::max(0.0f,clock.u-.001f),p);SampleRemixPath(nodes,std::min(1.0f,clock.u+.001f),q);float dx=q[0]-p[0],dy=q[1]-p[1],dz=p[2]-q[2];if(std::abs(dx)+std::abs(dy)+std::abs(dz)<=.00001f)return 0;rotation[0]=-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*57.2957795f;rotation[1]=std::atan2(dx,dz)*57.2957795f;rotation[2]=0;return 1;}catch(...){return 0;}}

extern "C" int StarForge_ReactiveInfo(int entry,int* target,float* speed,float* duration){try{auto& o=entry>=10000?project.at("events").at(entry-10000):project.at("waves").at(entry);if(!o.contains("flightBehavior"))return 0;auto& b=o["flightBehavior"];std::string type=b.value("type",std::string());if(type!="chase"&&type!="ram")return 0;*speed=Number(b,"speed",1,6000);*duration=Number(b,"duration",.1f,120);*target=-1;std::string id=b.value("targetId",std::string());for(int list=0;list<3;list++){const char* key=list==0?"waves":list==1?"events":"scenery";for(size_t i=0;i<project[key].size();i++)if(project[key][i].value("id",std::string())==id)*target=(int)i+list*10000;}return type=="ram"?2:1;}catch(...){return 0;}}

struct CombatGroupState{std::vector<std::string> members,controllers;std::unordered_map<std::string,bool> destroyed;int reward=0;bool escaped=false,awarded=false,inferred=false;};
struct CombatBinding{std::string group,member;bool controller=false;};
static std::unordered_map<std::string,CombatGroupState> combatGroups;
static std::unordered_map<void*,CombatBinding> combatBindings;
extern "C" void StarForge_GroupLoad(const char* text){combatGroups.clear();combatBindings.clear();try{auto doc=json::parse(text);auto add=[&](const json& g){CombatGroupState state;state.reward=g.value("reward",0);state.inferred=g.value("inferred",false);for(auto& id:g.at("members")){std::string member=id.get<std::string>();int count=1;if(doc.contains("waves"))for(auto& w:doc["waves"])if(w.value("id",std::string())==member)count=w.value("count",1);for(int i=0;i<count;i++)state.members.push_back(member+(i?":"+std::to_string(i):""));}for(auto& id:g.value("controllers",json::array()))state.controllers.push_back(id.get<std::string>());if(state.members.size()>120)throw std::runtime_error("group member count");if(state.reward!=0&&state.reward!=322&&state.reward!=323&&state.reward!=324&&state.reward!=327&&state.reward!=335&&state.reward!=336&&state.reward!=337)throw std::runtime_error("group reward");combatGroups[g.at("id").get<std::string>()]=state;};for(auto& g:doc.value("combatGroups",json::array()))add(g);if(doc.contains("waves"))for(auto& w:doc["waves"])if(!w.contains("editorGroup")&&w.contains("destructionReward")){json g={{"id","formation:"+w.value("id",std::string())},{"members",json::array({w.at("id")})},{"reward",w["destructionReward"]}};add(g);}}catch(...){combatGroups.clear();}}
extern "C" void StarForge_GroupBind(void* object,const char* uid){combatBindings.erase(object);if(!uid)return;for(auto& pair:combatGroups){auto& g=pair.second;for(auto& id:g.members)if(id==uid){combatBindings[object]={pair.first,id,false};return;}for(auto& id:g.controllers)if(id==uid){combatBindings[object]={pair.first,id,true};return;}}}
extern "C" void StarForge_RemixGroupBind(void* object,int entry,int lane){try{auto& w=project.at("waves").at(entry);std::string uid=w.at("id").get<std::string>()+(lane?":"+std::to_string(lane):"");StarForge_GroupBind(object,uid.c_str());}catch(...){}}
extern "C" int StarForge_GroupDestroyed(void* object,int killed){auto it=combatBindings.find(object);if(it==combatBindings.end())return -2;auto& binding=it->second;auto group=combatGroups.find(binding.group);if(group==combatGroups.end())return -2;auto& g=group->second;if(g.inferred&&g.reward==0)return -2;if(binding.controller)return -1;if(!killed){g.escaped=true;return -1;}g.destroyed[binding.member]=true;if(g.awarded||g.escaped||g.reward==0)return -1;for(auto& uid:g.members)if(!g.destroyed[uid])return -1;g.awarded=true;return g.reward;}
extern "C" void StarForge_GroupRemoved(void* object){auto it=combatBindings.find(object);if(it==combatBindings.end())return;auto group=combatGroups.find(it->second.group);if(group!=combatGroups.end()&&!it->second.controller&&!group->second.destroyed[it->second.member])group->second.escaped=true;combatBindings.erase(it);}

extern "C" int StarForge_RemixResolveScript(int current,int source){auto scope=originalScriptScope.find(current);if(scope==originalScriptScope.end())return source;auto target=originalScopeTargets[scope->second].find(source);return target==originalScopeTargets[scope->second].end()?source:target->second;}

struct ReactPreviewState{int entry=-1;float born=-1,start=-1;bool damaged=false;};
static std::map<void*,ReactPreviewState> reactStates;
extern "C" void StarForge_ReactDamage(void*object){reactStates[object].damaged=true;}
extern "C" float StarForge_ReactAge(void*object,int entry,float age,float z,float progress){try{auto&o=entry>=10000?project.at("events").at(entry-10000):project.at("waves").at(entry);if(!o.contains("flightBehavior")||o["flightBehavior"].value("type",std::string())!="react"||!o.contains("path"))return age;auto&b=o["flightBehavior"];auto&s=reactStates[object];if(s.entry!=entry||age<s.born){bool damage=s.damaged;s=ReactPreviewState{};s.entry=entry;s.damaged=damage;}s.born=age;if(s.start<0&&(b.value("trigger",std::string("damage"))=="damage"?s.damaged:std::abs(z+progress)<=b.value("distance",1000.0f)))s.start=age;return s.start<0?0:age-s.start;}catch(...){return age;}}

extern "C" void StarForge_ReactForget(void*object){reactStates.erase(object);}

extern "C" int StarForge_CruiserGuns(int entry,int* side){try{auto&w=project.at("waves").at(entry);int type=w.value("script",0)>=200?w.value("script",0)-200:w.value("script",0)==143?17:-1;if(w.contains("nativeMovement"))for(auto&c:w["nativeMovement"]["commands"])if(c.value("opcode",0)==104){type=c.value("value",-1);break;}if(type!=17||w.value("decorative",false))return 0;*side=w.value("cruiserGunSide",std::string("left"))=="right"?1:-1;return w.value("cruiserGuns",7)==5?5:7;}catch(...){return 0;}}

extern "C" int StarForge_RemixNativeId(int entry){try{return project.at("waves").at(entry).at("nativeObject").at("id").get<int>();}catch(...){return 0;}}
extern "C" float StarForge_RemixNativeSpeed(int entry,float original){try{return project.at("waves").at(entry).at("nativeObject").at("parameters").value("birdSpeed",original);}catch(...){return original;}}

extern "C" int StarForge_SwayEnabled(int entry){try{return project.at("waves").at(entry).value("sideToSide",std::string("sway"))!="static";}catch(...){return 1;}}

extern "C" int StarForge_RemixStatic(int entry){try{auto& o=project.at("waves").at(entry);int type=o.contains("nativeObject")?o["nativeObject"].value("id",-1):o.value("script",0)==142?61:o.value("script",0)-200;return o.value("staticScenery",false)||o.value("decorative",false)||StarForge_StaticActorType(type);}catch(...){return 0;}}
