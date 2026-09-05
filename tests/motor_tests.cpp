// SPDX-License-Identifier: MPL-2.0
#include "core/motor.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace astrabot::core;
int main() {
    MovementIntent intent; intent.direction={1,0,0}; intent.speed=200;
    auto r=Motor::command(intent,{0,0,0},250,10000,true);
    assert(r && r.command->movement.forward==200 && r.command->movement.side==0 && r.command->msec==10);
    r=Motor::command(intent,{0,90,0},100,16667,true);
    assert(r && std::abs(r.command->movement.forward)<0.001 && r.command->movement.side==100 && r.command->msec==17);
    intent.direction={0,1,0};
    r=Motor::command(intent,{0,0,0},400,1000,true);
    assert(r && r.command->movement.side==-200); // GoldSrc right vector at yaw=0 is -Y.
    intent.direction={0.6,0.8,0}; intent.speed=400; intent.lateralCorrection=1;
    r=Motor::command(intent,{0,0,0},120,1000,true); assert(r);
    assert(std::hypot(r.command->movement.forward,r.command->movement.side)<=120.001);
    intent.direction={1,0,0}; intent.speed=100;
    r=Motor::command(intent,{},250,1000,true); assert(r);
    assert(std::hypot(r.command->movement.forward,r.command->movement.side)<=100.001);
    intent={}; intent.view=IntentVector{100,450,100};
    intent.duck=ActionRequest::Hold; intent.jump=ActionRequest::Press; intent.use=ActionRequest::Press;
    r=Motor::command(intent,{},250,1000,true); assert(r);
    assert(r.command->view.pitch==89 && r.command->view.yaw==90 && r.command->view.roll==50);
    assert(r.command->buttons==(Button::Duck|Button::Jump|static_cast<ButtonMask>(Button::Use)));
    r=Motor::command(intent,{},250,1000,false); assert(r && r.command->buttons==static_cast<ButtonMask>(Button::Duck));
    intent.duck=ActionRequest::Release;
    r=Motor::command(intent,{},250,1000,false); assert(r && r.command->buttons==0);
    assert(Motor::command({}, {},250,0,true).error==MotorError::NoElapsedTime);
    r=Motor::command({}, {},250,std::numeric_limits<std::uint64_t>::max(),true); assert(r && r.command->msec==255);
    r=Motor::command({}, {},0,1,true); assert(r && r.command->msec==1 && r.command->validate());
    intent.speed=std::numeric_limits<double>::quiet_NaN();
    assert(Motor::command(intent,{},250,1000,true).error==MotorError::InvalidIntent);
    assert(Motor::command({}, {},-1,1000,true).error==MotorError::InvalidObservation);
    assert(Motor::command({}, {0,std::numeric_limits<float>::infinity(),0},250,1000,true).error==MotorError::InvalidObservation);
}
