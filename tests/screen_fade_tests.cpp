// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/messages.hpp"
#include <cassert>
namespace a = astrabot::adapter::cstrike;
int main() {
    a::MessageDecoder decoder;
    decoder.configure({11,12,13,14,15},nullptr,nullptr);
    const auto colors = [&]() { for (unsigned i=0;i<4;++i) decoder.writeByte(255); };
    decoder.begin(15,1); decoder.writeShort(4096); decoder.writeShort(8192); decoder.writeShort(0); colors(); decoder.end();
    assert(decoder.lastEvent().kind == a::MessageKind::ScreenFade);
    assert(decoder.lastEvent().recipientSlot == 1 && decoder.lastEvent().fadeTimesFlags[0] == 4096);
    assert(decoder.lastEvent().fadeTimesFlags[1] == 8192 && decoder.lastEvent().fadeColor[3] == 255);
    decoder.reset(); decoder.begin(15,2); decoder.writeShort(-1); decoder.writeShort(65535); decoder.writeShort(0); colors(); decoder.end();
    assert(decoder.lastEvent().fadeTimesFlags[0] == 65535 && decoder.lastEvent().fadeTimesFlags[1] == 65535);
    decoder.reset(); decoder.begin(15,1); decoder.writeByte(1); decoder.end();
    assert(decoder.lastEvent().kind == a::MessageKind::None && decoder.lastError() == a::MessageDecodeError::UnexpectedField);
    decoder.reset(); decoder.begin(15,1); decoder.writeShort(65536); decoder.end();
    assert(decoder.lastEvent().kind == a::MessageKind::None && decoder.lastError() == a::MessageDecodeError::InvalidShape);
    decoder.reset(); decoder.begin(15,1); decoder.writeShort(1); decoder.writeShort(1); decoder.writeShort(0); colors(); decoder.writeByte(1); decoder.end();
    assert(decoder.lastEvent().kind == a::MessageKind::None);
    decoder.reset(); decoder.begin(15,1); decoder.writeShort(1); decoder.writeShort(1); decoder.writeShort(0); decoder.writeByte(256); decoder.end();
    assert(decoder.lastEvent().kind == a::MessageKind::None);
    decoder.configure({11,12,13,14,0},nullptr,nullptr); decoder.begin(15,1);
    assert(!decoder.active() && decoder.lastError() == a::MessageDecodeError::UnknownMessage);
}
