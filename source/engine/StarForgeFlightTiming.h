#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace StarForgeFlight {
struct Timing { bool enabled=false,hold=false;std::vector<float> pauses; };
inline Timing Parse(const nlohmann::json& value,size_t count){
 Timing timing;if(value.is_null())return timing;timing.enabled=true;if(count<2||count>4096)throw std::runtime_error("Invalid timing route");
 if(!value.is_object()||!value.contains("stops")||!value["stops"].is_array()||value["stops"].size()>128)throw std::runtime_error("Invalid waypoint pauses");
 std::string end=value.value("end",std::string());if(end!="hold"&&end!="despawn")throw std::runtime_error("Invalid flight endpoint behavior");timing.hold=end=="hold";timing.pauses.resize(count,0);
 for(const auto& stop:value["stops"]){if(!stop.contains("point")||!stop["point"].is_number_integer())throw std::runtime_error("Invalid pause waypoint");int point=stop["point"].get<int>();float seconds=stop.at("seconds").get<float>();if(point<0||point>=count||!std::isfinite(seconds)||seconds<0||seconds>120)throw std::runtime_error("Invalid waypoint pause");timing.pauses[point]+=seconds;if(timing.pauses[point]>120)throw std::runtime_error("Waypoint pause exceeds 120 seconds");}
 return timing;
}
struct Sample {float u,total;bool ended;};
inline Sample Clock(float age,float duration,size_t count,const Timing& timing){
 float elapsed=0,total=duration;for(float pause:timing.pauses)total+=pause;if(!std::isfinite(total)||duration<=0||total>600)throw std::runtime_error("Flight timing exceeds 600 seconds");age=std::max(0.0f,age);
 if(!timing.enabled)return{std::max(0.0f,std::min(1.0f,age/duration)),duration,age>duration};
 for(size_t point=0;point<count;point++){
  float pause=point<timing.pauses.size()?timing.pauses[point]:0;if(age<elapsed+pause)return{float(point)/(count-1),total,false};elapsed+=pause;
  if(point+1<count){float travel=duration/(count-1);if(age<elapsed+travel)return{(point+(age-elapsed)/travel)/(count-1),total,false};elapsed+=travel;}
 }
 return{1,total,age>=total};
}
struct FireWindow { float start,end,interval; };
struct Actions { bool enabled=false;std::vector<FireWindow> fire; };
inline Actions ParseActions(const nlohmann::json& value,float duration){
 Actions result;if(value.is_null())return result;result.enabled=true;if(!value.is_array()||value.size()>16)throw std::runtime_error("Invalid encounter actions");
 for(const auto& action:value){float start=action.at("start").get<float>(),end=action.at("end").get<float>(),interval=action.at("interval").get<float>();if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(interval)||start<0||end<start||end>duration+.01f||interval<.25f||interval>10)throw std::runtime_error("Invalid encounter firing window");result.fire.push_back({start,end,interval});}return result;
}
inline int Fire(const Actions& actions,float age,float previous){
 if(!actions.enabled)return -1;if(!std::isfinite(age)||age<0||age<=previous)return 0;
 for(const auto& action:actions.fire){if(age+1e-5f<action.start||previous>=action.end)continue;float until=std::min(age,action.end);int shot=(int)std::floor((until-action.start+1e-5f)/action.interval);if(shot>=0&&action.start+shot*action.interval>previous+1e-5f)return 1;}return 0;
}

}
