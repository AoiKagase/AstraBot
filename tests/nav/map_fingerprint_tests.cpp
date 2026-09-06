// SPDX-License-Identifier: MPL-2.0
#include "nav/io/map_fingerprint.hpp"
#include <cassert>
#include <sstream>
#include <string>
#include <stdexcept>
using namespace astrabot::nav::io;
namespace {
std::string hex(const astrabot::nav::enrichment::NavMapFingerprint& f) {
    std::string result; for(auto b:f) { result+="0123456789abcdef"[b>>4]; result+="0123456789abcdef"[b&15]; } return result;
}
void check(const std::string& value,const char* expected) {
    for(auto limit:{static_cast<std::uint64_t>(value.size()),std::uint64_t{2000000}}) {
        std::istringstream input(value); const auto r=fingerprintMap(input,limit);
        assert(r && r.bytes==value.size() && hex(*r.fingerprint)==expected && input.str()==value);
    }
}
class FailingBuffer final : public std::stringbuf {
    unsigned reads{};
public:
    FailingBuffer():std::stringbuf(std::string(8192,'x')) {}
    std::streamsize xsgetn(char* data,std::streamsize size) override {
        if(reads++) throw std::runtime_error("synthetic input failure");
        return std::stringbuf::xsgetn(data,size);
    }
};
}
int main() {
    check("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check(std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    for(std::uint64_t cap:{0U,1U,4096U}) {
        std::istringstream input(std::string(4097,'a')); const auto r=fingerprintMap(input,cap);
        assert(!r && !r.fingerprint && r.reason==FingerprintReason::SizeLimit && r.bytes==cap);
    }
    std::istringstream bad("abc"); bad.setstate(std::ios::badbit);
    assert(fingerprintMap(bad,3).reason==FingerprintReason::InputFailure);
    std::istringstream limit("abc"); assert(fingerprintMap(limit,512ULL*1024*1024+1).reason==FingerprintReason::InvalidLimit);
    assert(limit.tellg()==0);
    FailingBuffer buffer; std::istream interrupted(&buffer);
    const auto failed=fingerprintMap(interrupted,8192);
    assert(!failed && !failed.fingerprint && failed.reason==FingerprintReason::InputFailure && failed.bytes==4096);
}
