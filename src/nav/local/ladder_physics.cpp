// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ladder_physics.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot::nav::local {
namespace {
constexpr double radians=3.14159265358979323846/180;
constexpr auto directional=static_cast<core::ButtonMask>(core::Button::Forward)|static_cast<core::ButtonMask>(core::Button::Back);
bool commandValid(const core::BotCommand& c) noexcept {
    return c.msec>0 && c.msec<=120 && !c.impulse && !(c.buttons&~directional) &&
        std::isfinite(c.view.pitch) && std::abs(c.view.pitch)<=89 && std::isfinite(c.view.yaw) && c.view.roll==0 &&
        std::isfinite(c.movement.forward) && std::isfinite(c.movement.side) && c.movement.up==0;
}
bool positive(double v,double cap) noexcept { return std::isfinite(v) && v>0 && v<=cap; }
}
std::optional<model::NavVector3> ladderVelocity(const core::BotCommand& c,model::NavVector3 n,
    double maximum,bool floorSolid) noexcept {
    if(!commandValid(c) || !positive(maximum,2000) || !n.isFinite() || n.z!=0 ||
       std::abs(double(n.x)*n.x+double(n.y)*n.y-1)>0.001 || c.buttons==directional) return {};
    const bool forward=(c.buttons&static_cast<core::ButtonMask>(core::Button::Forward))!=0;
    const bool back=(c.buttons&static_cast<core::ButtonMask>(core::Button::Back))!=0;
    if(!forward && !back) return model::NavVector3{};
    const double speed=(forward ? 1:-1)*(std::min)(maximum,200.0);
    const double pitch=c.view.pitch*radians,yaw=c.view.yaw*radians;
    const double x=speed*std::cos(pitch)*std::cos(yaw),y=speed*std::cos(pitch)*std::sin(yaw);
    const double into=x*n.x+y*n.y,tangent=-x*n.y+y*n.x;
    const double kick=floorSolid && into>0 ? 200:0;
    return model::NavVector3{static_cast<float>(-n.y*tangent+n.x*kick),
        static_cast<float>(n.x*tangent+n.y*kick),static_cast<float>(-speed*std::sin(pitch)-into)};
}
std::optional<LadderAirStep> ladderAirStep(const core::BotCommand& c,model::NavVector3 v,LadderAirPhysics p) noexcept {
    if(!commandValid(c) || !v.isFinite() || !positive(p.gravity,4000) || !positive(p.airAcceleration,1000) ||
       !positive(p.friction,10) || !positive(p.maximumSpeed,2000) ||
       std::abs(v.x)>10000 || std::abs(v.y)>10000 || std::abs(v.z)>10000) return {};
    const double yaw=c.view.yaw*radians,dt=double(c.msec)/1000;
    const double x=c.movement.forward*std::cos(yaw)+c.movement.side*std::sin(yaw);
    const double y=c.movement.forward*std::sin(yaw)-c.movement.side*std::cos(yaw);
    const double magnitude=std::hypot(x,y),wish=(std::min)(magnitude,p.maximumSpeed);
    double vx=v.x,vy=v.y;
    if(magnitude>0) {
        const double nx=x/magnitude,ny=y/magnitude;
        const double available=(std::min)(wish,30.0)-(vx*nx+vy*ny);
        const double add=(std::max)(0.0,(std::min)(available,p.airAcceleration*wish*dt*p.friction));
        vx+=nx*add; vy+=ny*add;
    }
    LadderAirStep result;
    result.displacement={static_cast<float>(vx*dt),static_cast<float>(vy*dt),static_cast<float>((v.z-0.5*p.gravity*dt)*dt)};
    result.velocity={static_cast<float>(vx),static_cast<float>(vy),static_cast<float>(v.z-p.gravity*dt)};
    if(!result.displacement.isFinite() || !result.velocity.isFinite()) return {};
    return result;
}
}
