#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace mt2mp::game::subscriber_motion {
// Portable pose fields, not native transform cache or object addresses.
struct Pose {
    int32_t region=-1;
    std::array<float,4> orientation{};
    std::array<float,3> position{},scale{};
    bool operator==(const Pose&)const=default;
};
inline bool valid(const Pose& pose,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(pose.region<0)return fail("Subscriber motion saved Region ID invalid");
    for(auto value:pose.orientation)if(!std::isfinite(value))return fail("Subscriber motion quaternion not finite");
    for(auto value:pose.position)if(!std::isfinite(value))return fail("Subscriber motion position not finite");
    for(auto value:pose.scale)if(!std::isfinite(value)||value<=0)return fail("Subscriber motion scale invalid");
    // Native vsQuaternion::NormaliseIfNeeded and Slerp retain this float norm range.
    const auto& q=pose.orientation;
    const float xz=q[0]*q[0]+q[2]*q[2],yw=q[1]*q[1]+q[3]*q[3];
    const float norm=yw+xz;
    if(norm<0.99f||norm>1.01f)return fail("Subscriber motion quaternion is not normalized");
    error.clear();return true;
}
}
