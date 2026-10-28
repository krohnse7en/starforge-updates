// Generic optional imported scenery. Game-specific data stays in local sidecars.
#include "StarForgeImportedScene.h"
#include <array>
#include <numeric>
#include <limits>
#include <cstring>
#include <set>
namespace SFI {
struct Vec {float x,y,z;};
static Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec mul(Vec a,float b){return {a.x*b,a.y*b,a.z*b};}
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Vertex {int16_t p[3];uint16_t flag;int16_t uv[2];uint8_t color[4];};
static_assert(sizeof(Vertex)==16,"Imported vertex ABI");
struct Chunk {SFI_Chunk view{};std::vector<Vertex> vertices;std::array<char,96> texture{};};
struct Tri {Vec a,b,c;};
struct Node {Vec lo,hi;int begin=0,count=0,left=-1,right=-1;};
static std::vector<Chunk> chunks;
static std::vector<Tri> triangles;
static std::vector<Vec> trees;
static std::vector<int> order;
static std::vector<Node> nodes;
static std::vector<std::array<float,3>> route,recoveryRoute;
static int generation=0,lastReport=-1;
static float length=0,height=650,worldScale=1,speed=1200,shipRadius=55,farDistance=16000;
static float camera[5]={400,50,20,1,45};
static bool ready=false,debug=false,nativeFlight=true;
template<class T>static T read(std::istream& f){T v{};if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("truncated imported scene");return v;}
static bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<2000000&&std::abs(p.y)<2000000&&std::abs(p.z)<2000000;}
static float axis(Vec p,int a){return a==0?p.x:a==1?p.y:p.z;}
static int build(int first,int count){
 Node n;n.begin=first;n.count=count;n.lo={1e30f,1e30f,1e30f};n.hi={-1e30f,-1e30f,-1e30f};
 for(int i=first;i<first+count;i++)for(Vec p:{triangles[order[i]].a,triangles[order[i]].b,triangles[order[i]].c}){n.lo={std::min(n.lo.x,p.x),std::min(n.lo.y,p.y),std::min(n.lo.z,p.z)};n.hi={std::max(n.hi.x,p.x),std::max(n.hi.y,p.y),std::max(n.hi.z,p.z)};}
 int index=(int)nodes.size();nodes.push_back(n);
 if(count>8){Vec d=sub(n.hi,n.lo);int a=d.x>d.y?(d.x>d.z?0:2):(d.y>d.z?1:2);int middle=first+count/2;
  std::nth_element(order.begin()+first,order.begin()+middle,order.begin()+first+count,[a](int x,int y){return axis(add(add(triangles[x].a,triangles[x].b),triangles[x].c),a)<axis(add(add(triangles[y].a,triangles[y].b),triangles[y].c),a);});
  int left=build(first,middle-first),right=build(middle,first+count-middle);nodes[index].left=left;nodes[index].right=right;nodes[index].count=0;
 }return index;
}
static Vec nearest(Vec p,const Tri&t){
 Vec ab=sub(t.b,t.a),ac=sub(t.c,t.a),ap=sub(p,t.a);float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return t.a;
 Vec bp=sub(p,t.b);float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return t.b;
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return add(t.a,mul(ab,d1/(d1-d3)));
 Vec cp=sub(p,t.c);float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return t.c;
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return add(t.a,mul(ac,d2/(d2-d6)));
 float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return add(t.b,mul(sub(t.c,t.b),(d4-d3)/((d4-d3)+(d5-d6))));
 float den=va+vb+vc;if(std::abs(den)<1e-12f)return t.a;return add(t.a,add(mul(ab,vb/den),mul(ac,vc/den)));
}
static void query(int i,Vec p,float r,float& best,Vec& correction){
 const Node&n=nodes[i];Vec q={std::max(n.lo.x,std::min(n.hi.x,p.x)),std::max(n.lo.y,std::min(n.hi.y,p.y)),std::max(n.lo.z,std::min(n.hi.z,p.z))};Vec delta=sub(p,q);if(dot(delta,delta)>r*r)return;
 if(n.left>=0){query(n.left,p,r,best,correction);query(n.right,p,r,best,correction);return;}
 for(int j=n.begin;j<n.begin+n.count;j++){Vec closest=nearest(p,triangles[order[j]]),d=sub(p,closest);float distance=std::sqrt(dot(d,d));if(distance<r&&r-distance>best){best=r-distance;correction=distance>.0001f?mul(d,(best+.5f)/distance):Vec{0,r+.5f,0};}}
}
}
#include "StarForgeImportedEdits.inc"
extern "C" void SFI_Load(const char* text){using namespace SFI;ready=false;chunks.clear();triangles.clear();trees.clear();nodes.clear();route.clear();recoveryRoute.clear();generation++;lastReport=-1;
 try{auto j=nlohmann::json::parse(text);if(!j.contains("importedScene")||!j["importedScene"].value("enabled",false))return;debug=j["importedScene"].value("debug",false);
  auto config=j["importedScene"];nativeFlight=config.value("nativeFlight",true);worldScale=config.value("worldScale",1.f);speed=config.value("speedUnitsPerSecond",1200.f);shipRadius=config.value("shipRadius",35.f);farDistance=config.value("farDistance",16000.f);
  if(!std::isfinite(worldScale)||worldScale<.1f||worldScale>10||!std::isfinite(speed)||speed<30||speed>6000||!std::isfinite(shipRadius)||shipRadius<1||shipRadius>200||!std::isfinite(farDistance)||farDistance<1000||farDistance>200000)throw std::runtime_error("invalid imported calibration");
  const char* cameraNames[]={"cameraDistance","cameraEyeHeight","cameraAimHeight","cameraLookAhead","cameraFov"};const float defaults[]={400,50,20,1,45};for(int i=0;i<5;i++){camera[i]=config.value(cameraNames[i],defaults[i]);if(!std::isfinite(camera[i])||camera[i]<0||camera[i]>10000)throw std::runtime_error("invalid imported camera");}
  std::ifstream f("StarForge/imported-scene.sfg",std::ios::binary);if(!f)throw std::runtime_error("imported-scene.sfg missing");std::array<char,4> magic=read<std::array<char,4>>(f);if(std::memcmp(magic.data(),"SFG1",4))throw std::runtime_error("imported scene version");
  uint32_t nc=read<uint32_t>(f),nt=read<uint32_t>(f),nk=read<uint32_t>(f);length=read<float>(f);height=read<float>(f);
  if(nc>8192||nt>500000||nk>4096||!std::isfinite(length)||length<100||length>720000||!std::isfinite(height)||std::abs(height)>30000)throw std::runtime_error("imported scene limits");chunks.resize(nc);size_t vertices=0;
  for(auto&c:chunks){for(float&v:c.view.origin)v=read<float>(f);c.view.radius=read<float>(f);c.view.vertexCount=read<uint32_t>(f);c.view.flags=read<uint32_t>(f);c.texture=read<std::array<char,96>>(f);c.texture[95]=0;
   if(!finite({c.view.origin[0],c.view.origin[1],c.view.origin[2]})||!std::isfinite(c.view.radius)||c.view.radius<0||c.view.radius>500000||c.view.vertexCount>60000||c.view.vertexCount%3||vertices+c.view.vertexCount>3000000)throw std::runtime_error("invalid imported chunk");
   std::string name(c.texture.data());if(name.rfind("__OTR__starforge/imported/",0)||name.find("..")!=std::string::npos)throw std::runtime_error("invalid imported texture reference");vertices+=c.view.vertexCount;c.vertices.resize(c.view.vertexCount);if(!f.read(reinterpret_cast<char*>(c.vertices.data()),c.vertices.size()*sizeof(Vertex)))throw std::runtime_error("truncated imported vertices");c.view.vertices=c.vertices.data();c.view.texture=c.texture.data();
  }
  triangles.resize(nt);for(auto&t:triangles){t=read<Tri>(f);if(!finite(t.a)||!finite(t.b)||!finite(t.c))throw std::runtime_error("invalid collision triangle");}for(uint32_t i=0;i<nk;i++){auto k=read<std::array<float,3>>(f);for(float v:k)if(!std::isfinite(v)||std::abs(v)>1000000)throw std::runtime_error("invalid route key");if(i&&k[0]<=route.back()[0])throw std::runtime_error("unordered route keys");route.push_back(k);}if(f.peek()!=EOF)throw std::runtime_error("unexpected imported scene data");
  for(auto&c:chunks){for(float&v:c.view.origin)v*=worldScale;c.view.radius*=worldScale;}for(auto&t:triangles){t.a=mul(t.a,worldScale);t.b=mul(t.b,worldScale);t.c=mul(t.c,worldScale);}length=config.value("length",length*worldScale);if(!std::isfinite(length)||length<100||length>720000)throw std::runtime_error("invalid calibrated length");height=config.value("initialHeight",height*worldScale);if(!std::isfinite(height)||std::abs(height)>30000)throw std::runtime_error("invalid imported height");
  for(auto&k:route)for(float&v:k)v*=worldScale;
  if(config.contains("route")){route.clear();for(auto&entry:config["route"]){if(route.size()>=4096||!entry.is_array()||entry.size()!=3)throw std::runtime_error("invalid calibrated route");std::array<float,3> k;for(int i=0;i<3;i++){k[i]=entry[i].get<float>();if(!std::isfinite(k[i])||std::abs(k[i])>1000000)throw std::runtime_error("invalid calibrated route key");}if(!route.empty()&&k[0]<=route.back()[0])throw std::runtime_error("unordered calibrated route");route.push_back(k);}if(route.empty()||route.front()[0]!=0||route.back()[0]<length)throw std::runtime_error("incomplete calibrated route");}
  if(config.contains("recoveryRoute")){for(auto&entry:config["recoveryRoute"]){if(recoveryRoute.size()>=4096||!entry.is_array()||entry.size()!=3)throw std::runtime_error("invalid recovery route");std::array<float,3> k;for(int i=0;i<3;i++){k[i]=entry[i].get<float>();if(!std::isfinite(k[i])||std::abs(k[i])>1000000)throw std::runtime_error("invalid recovery route key");}if(!recoveryRoute.empty()&&k[0]<=recoveryRoute.back()[0])throw std::runtime_error("unordered recovery route");recoveryRoute.push_back(k);}if(recoveryRoute.empty()||recoveryRoute.front()[0]!=0||recoveryRoute.back()[0]<length)throw std::runtime_error("incomplete recovery route");}
  if(config.contains("nativeTrees")){if(!config["nativeTrees"].is_array()||config["nativeTrees"].size()>6000)throw std::runtime_error("invalid native trees");for(auto&entry:config["nativeTrees"]){if(!entry.is_array()||entry.size()!=3)throw std::runtime_error("invalid native tree position");Vec p{entry[0].get<float>(),entry[1].get<float>(),entry[2].get<float>()};if(!finite(p)||std::abs(p.x)>=32767||std::abs(p.y)>=32767)throw std::runtime_error("invalid native tree position");trees.push_back(p);}}
  applyObjectEdits(config);nt=(uint32_t)triangles.size();std::stable_sort(trees.begin(),trees.end(),[](Vec a,Vec b){return a.z>b.z;});
  order.resize(nt);std::iota(order.begin(),order.end(),0);nodes.reserve(nt/2+1);if(nt)build(0,nt);ready=true;
  std::ofstream("StarForge/imported-scene-status.txt")<<"Imported scene ready: "<<nc<<" chunks, "<<vertices/3<<" rendered triangles, "<<nt<<" collision triangles.\n";
 }catch(const std::exception&e){chunks.clear();triangles.clear();nodes.clear();route.clear();std::ofstream("StarForge/imported-scene-status.txt")<<"Imported scene rejected: "<<e.what()<<"\n";}
}
extern "C" int SFI_Ready(void){return SFI::ready;}
extern "C" int SFI_Generation(void){return SFI::generation;}
extern "C" int SFI_Count(void){return (int)SFI::chunks.size();}
extern "C" const SFI_Chunk* SFI_Get(int i){return i>=0&&i<(int)SFI::chunks.size()?&SFI::chunks[i].view:nullptr;}
extern "C" float SFI_Length(void){return SFI::length;}
extern "C" float SFI_Height(void){return SFI::height;}
extern "C" float SFI_VertexScale(void){return SFI::worldScale;}
extern "C" float SFI_Speed(void){return SFI::speed;}
extern "C" float SFI_Radius(void){return SFI::shipRadius;}
extern "C" float SFI_Far(void){return SFI::farDistance;}
extern "C" float SFI_Camera(int i){return i>=0&&i<5?SFI::camera[i]:0;}
extern "C" int SFI_BootLevel(void){try{std::ifstream f("StarForge/project.json");nlohmann::json j;f>>j;if(j.contains("importedScene")&&j["importedScene"].value("enabled",false)&&j["importedScene"].value("autoStart",false)&&j.value("schema",std::string())=="starforge.level.v1")return 0;}catch(...){}return -1;}
extern "C" int SFI_Collide(const float*p,float radius,float*out){using namespace SFI;if(!ready||nodes.empty()||!p||!out||!std::isfinite(radius)||radius<=0||radius>200)return 0;Vec point{p[0],p[1],p[2]},correction{};if(!finite(point))return 0;float best=0;query(0,point,radius,best,correction);out[0]=correction.x;out[1]=correction.y;out[2]=correction.z;return best>0;}
extern "C" void SFI_Route(float progress,float*x,float*y){auto&r=SFI::route;if(r.empty()){*x=*y=0;return;}size_t i=0;while(i+1<r.size()&&r[i+1][0]<=progress)i++;float t=i+1<r.size()?std::max(0.f,std::min(1.f,(progress-r[i][0])/(r[i+1][0]-r[i][0]))):0;*x=r[i][1]+(i+1<r.size()?t*(r[i+1][1]-r[i][1]):0);*y=r[i][2]+(i+1<r.size()?t*(r[i+1][2]-r[i][2]):0);}
extern "C" void SFI_Report(float progress,int drawn,int collisions){using namespace SFI;if(!debug)return;int tick=(int)(progress/1200);if(tick==lastReport&&collisions==0)return;lastReport=tick;std::ofstream f("StarForge/imported-scene-trace.txt",std::ios::app);f<<"progress="<<progress<<" drawn="<<drawn<<" contacts="<<collisions<<"\n";}

namespace SFI {
static void floorQuery(int index,float x,float z,float ceiling,float& best){
 const Node&n=nodes[index];if(x<n.lo.x||x>n.hi.x||z<n.lo.z||z>n.hi.z||n.lo.y>ceiling||n.hi.y<best)return;
 if(!n.count){floorQuery(n.left,x,z,ceiling,best);floorQuery(n.right,x,z,ceiling,best);return;}
 for(int i=n.begin;i<n.begin+n.count;i++){const Tri&t=triangles[order[i]];Vec u=sub(t.b,t.a),v=sub(t.c,t.a);float ny=u.z*v.x-u.x*v.z,nx=u.y*v.z-u.z*v.y,nz=u.x*v.y-u.y*v.x;
  if(ny*ny<.09f*(nx*nx+ny*ny+nz*nz)||fabsf(ny)<.00001f)continue;
  float d=u.x*v.z-u.z*v.x,a=((x-t.a.x)*v.z-(z-t.a.z)*v.x)/d,b=(u.x*(z-t.a.z)-u.z*(x-t.a.x))/d;
  if(a<-.00001f||b<-.00001f||a+b>1.00001f)continue;float y=t.a.y+a*u.y+b*v.y;if(y<=ceiling&&y>best)best=y;
 }
}
}
extern "C" int SFI_Floor(float x,float z,float ceiling,float* height){
 if(!SFI::ready||SFI::nodes.empty()||!height||!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(ceiling))return 0;
 float best=-2000001;SFI::floorQuery(0,x,z,ceiling,best);if(best==-2000001)return 0;*height=best;return 1;
}

extern "C" void SFI_RecoveryRoute(float progress,float*x,float*y){auto&r=SFI::recoveryRoute;if(r.empty()){SFI_Route(progress,x,y);return;}size_t i=0;while(i+1<r.size()&&r[i+1][0]<=progress)i++;float t=i+1<r.size()?std::max(0.f,std::min(1.f,(progress-r[i][0])/(r[i+1][0]-r[i][0]))):0;*x=r[i][1]+(i+1<r.size()?t*(r[i+1][1]-r[i][1]):0);*y=r[i][2]+(i+1<r.size()?t*(r[i+1][2]-r[i][2]):0);}

extern "C" int SFI_NativeFlight(void){return SFI::nativeFlight;}

extern "C" void SFI_FlightState(float progress,float x,float y,float z,float pathX,float pathY,float speed,float width,float height,int shields,int actors){if(!SFI::debug)return;static int last=-1;int tick=(int)(progress/1200);if(tick==last)return;last=tick;std::ofstream f("StarForge/imported-flight-state.txt",std::ios::app);f<<"progress="<<progress<<" position="<<x<<","<<y<<","<<z<<" path="<<pathX<<","<<pathY<<" speed="<<speed<<" bounds="<<width<<","<<height<<" shields="<<shields<<" actors="<<actors<<" native="<<SFI_NativeFlight()<<"\n";}

extern "C" int SFI_TreeCount(void){return SFI::ready?(int)SFI::trees.size():0;}
extern "C" int SFI_Tree(int i,float*p){if(!p||i<0||i>=SFI_TreeCount())return 0;auto v=SFI::trees[i];p[0]=v.x;p[1]=v.y;p[2]=v.z;return 1;}
