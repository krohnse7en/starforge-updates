// Apply per-object edits to both prepared drawing and physical geometry.
namespace SFI {
static Vec rotateObject(Vec p,Vec degrees){
 const float k=.017453292519943295f;float x=degrees.x*k,y=degrees.y*k,z=degrees.z*k;
 Vec a{p.x*cosf(z)-p.y*sinf(z),p.x*sinf(z)+p.y*cosf(z),p.z};
 Vec b{a.x,a.y*cosf(x)-a.z*sinf(x),a.y*sinf(x)+a.z*cosf(x)};
 return {b.x*cosf(y)+b.z*sinf(y),b.y,-b.x*sinf(y)+b.z*cosf(y)};
}
static void applyObjectEdits(const nlohmann::json&config){
 if(!config.contains("objects")){if(config.contains("objectEdits")&&!config["objectEdits"].empty())throw std::runtime_error("object edits require an editable import");return;}
 auto objects=config["objects"],edits=config.value("objectEdits",nlohmann::json::object());
 if(!objects.is_array()||objects.size()>8192||!edits.is_object()||edits.size()>objects.size())throw std::runtime_error("invalid imported object list");
 std::vector<bool> ownedChunks(chunks.size()),ownedTris(triangles.size()),ownedTrees(trees.size()),deletedTris(triangles.size()),deletedTrees(trees.size());std::set<std::string> ids;
 auto vec=[](const nlohmann::json&j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("invalid object transform");Vec v{j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};if(!finite(v))throw std::runtime_error("invalid object transform");return v;};
 for(auto&o:objects){std::string id=o.at("id").get<std::string>();if(id.empty()||id.size()>64||!ids.insert(id).second)throw std::runtime_error("duplicate imported object");
  Vec origin=vec(o.at("origin")),position=origin,rotation{};bool deleted=false;
  if(edits.contains(id)){auto e=edits[id];if(!e.is_object())throw std::runtime_error("invalid object edit");if(e.contains("position"))position=vec(e["position"]);if(e.contains("rotation"))rotation=vec(e["rotation"]);deleted=e.value("deleted",false);for(int k=0;k<3;k++)if(std::abs(axis(rotation,k))>36000)throw std::runtime_error("invalid object rotation");}
  auto transform=[&](Vec p){Vec v=add(position,rotateObject(sub(p,origin),rotation));if(!finite(v))throw std::runtime_error("object outside supported world");return v;};
  if(!o.at("chunks").is_array()||o["chunks"].size()>8192)throw std::runtime_error("invalid object chunks");
  for(auto&entry:o["chunks"]){if(!entry.is_number_integer())throw std::runtime_error("invalid object chunk index");int index=entry.get<int>();if(index<0||index>=(int)chunks.size()||ownedChunks[index])throw std::runtime_error("overlapping object chunks");ownedChunks[index]=true;auto&c=chunks[index];
   if(deleted){c.vertices.clear();c.view.vertexCount=0;c.view.radius=0;continue;}
   Vec p=transform({c.view.origin[0],c.view.origin[1],c.view.origin[2]});c.view.origin[0]=p.x;c.view.origin[1]=p.y;c.view.origin[2]=p.z;
   for(auto&v:c.vertices){Vec p=rotateObject({(float)v.p[0],(float)v.p[1],(float)v.p[2]},rotation);for(int k=0;k<3;k++){float n=roundf(axis(p,k));if(n< -32768||n>32767)throw std::runtime_error("rotated object chunk is too large");v.p[k]=(int16_t)n;}}
  }
  if(!o.at("collisionStart").is_number_integer()||!o.at("collisionCount").is_number_integer())throw std::runtime_error("invalid object collision range");int first=o["collisionStart"],count=o["collisionCount"];if(first<0||count<0||(size_t)first+count>triangles.size())throw std::runtime_error("invalid object collision range");
  for(int i=first;i<first+count;i++){if(ownedTris[i])throw std::runtime_error("overlapping object collision");ownedTris[i]=true;deletedTris[i]=deleted;if(!deleted){auto&t=triangles[i];t.a=transform(t.a);t.b=transform(t.b);t.c=transform(t.c);}}
  if(o.contains("treeIndex")){if(!o["treeIndex"].is_number_integer())throw std::runtime_error("invalid tree object index");int i=o["treeIndex"];if(i<0||i>=(int)trees.size()||ownedTrees[i])throw std::runtime_error("invalid tree object index");ownedTrees[i]=true;deletedTrees[i]=deleted;trees[i]=transform(trees[i]);if(std::abs(trees[i].x)>=32767||std::abs(trees[i].y)>=32767)throw std::runtime_error("tree outside native coordinate limits");}
 }
 for(auto it=edits.begin();it!=edits.end();++it)if(!ids.count(it.key()))throw std::runtime_error("unknown imported object edit");
 if(std::find(ownedChunks.begin(),ownedChunks.end(),false)!=ownedChunks.end()||std::find(ownedTris.begin(),ownedTris.end(),false)!=ownedTris.end()||std::find(ownedTrees.begin(),ownedTrees.end(),false)!=ownedTrees.end())throw std::runtime_error("incomplete imported object ownership");
 std::vector<Tri> kept;for(size_t i=0;i<triangles.size();i++)if(!deletedTris[i])kept.push_back(triangles[i]);triangles.swap(kept);
 std::vector<Vec> treeKept;for(size_t i=0;i<trees.size();i++)if(!deletedTrees[i])treeKept.push_back(trees[i]);trees.swap(treeKept);
 for(auto&c:chunks){c.view.vertices=c.vertices.data();c.view.texture=c.texture.data();}
}
}
