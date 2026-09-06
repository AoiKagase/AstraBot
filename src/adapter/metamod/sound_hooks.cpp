// SPDX-License-Identifier: MPL-2.0
#include "adapter/metamod/sound_hooks.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <cstring>

namespace astrabot::adapter::metamod {
namespace {
enum class Call { Precache, Emit, Ambient, Playback };
struct PendingCall { Call kind{}; std::uint64_t revision{}; const edict_t* entity{}; int serial{}; };
std::array<PendingCall,32> pending{};
std::size_t depth{};
bool emitted() noexcept { return gpMetaGlobals && gpMetaGlobals->status < MRES_SUPERCEDE; }
void ignored() noexcept { if (gpMetaGlobals) gpMetaGlobals->mres = MRES_IGNORED; }
void begin(Call kind,const edict_t* entity=nullptr) noexcept {
    if (depth < pending.size()) pending[depth] = {kind,lifecycleCoordinator().sound().revision(),entity,entity ? entity->serialnumber:0};
    ++depth; ignored();
}
bool finish(Call kind) noexcept {
    const auto reject = []() { lifecycleCoordinator().rejectSoundHook(); return false; };
    if (depth == 0) return reject();
    --depth;
    if (depth >= pending.size()) return reject();
    const auto call = pending[depth];
    if (call.kind != kind || call.revision != lifecycleCoordinator().sound().revision() ||
        (call.entity && (call.entity->free || call.entity->serialnumber != call.serial))) return reject();
    return true;
}
unsigned short precachePre(int,const char*) { begin(Call::Precache); return 0; }
void emitPre(edict_t* e,int,const char*,float,float,int,int) { begin(Call::Emit,e); }
void ambientPre(edict_t* e,float*,const char*,float,float,int,int) { begin(Call::Ambient,e); }
void playbackPre(int,const edict_t* e,unsigned short,float,float*,float*,float,float,int,int,int,int) { begin(Call::Playback,e); }
unsigned short precache(int type,const char* name) {
    if (finish(Call::Precache) && emitted() && gpMetaGlobals->orig_ret && gpMetaGlobals->status < MRES_OVERRIDE) {
        unsigned short index{}; std::memcpy(&index,gpMetaGlobals->orig_ret,sizeof(index));
        lifecycleCoordinator().soundPrecache(type,name,index);
    }
    ignored(); return 0;
}
void emit(edict_t* emitter,int channel,const char* sample,float volume,float attenuation,int flags,int pitch) {
    if (finish(Call::Emit) && emitted()) lifecycleCoordinator().emitSound(emitter,nullptr,channel,sample,volume,attenuation,flags,pitch,false);
    ignored();
}
void ambient(edict_t* emitter,float* origin,const char* sample,float volume,float attenuation,int flags,int pitch) {
    if (finish(Call::Ambient) && emitted()) lifecycleCoordinator().emitSound(emitter,origin,0,sample,volume,attenuation,flags,pitch,true);
    ignored();
}
void playback(int flags,const edict_t* invoker,unsigned short event,float delay,float* origin,float*,float,float,int,int,int,int) {
    if (finish(Call::Playback) && emitted()) lifecycleCoordinator().playbackEvent(flags,invoker,event,delay,origin);
    ignored();
}
}
void installSoundPreHooks(enginefuncs_t& table) noexcept {
    table.pfnPrecacheEvent = &precachePre; table.pfnEmitSound = &emitPre;
    table.pfnEmitAmbientSound = &ambientPre; table.pfnPlaybackEvent = &playbackPre;
}
int soundEngineFunctionsPost(enginefuncs_t* table,int* version) {
    if (!table || !version) return 0;
    if (*version != ENGINE_INTERFACE_VERSION) { *version = ENGINE_INTERFACE_VERSION; return 0; }
    *table = {}; table->pfnPrecacheEvent = &precache; table->pfnEmitSound = &emit;
    table->pfnEmitAmbientSound = &ambient; table->pfnPlaybackEvent = &playback; return 1;
}
} // namespace astrabot::adapter::metamod
