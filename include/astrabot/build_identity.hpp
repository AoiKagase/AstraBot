#pragma once

namespace astrabot
{
namespace build
{

static_assert(sizeof(void *) == 4, "AstraBot requires a 32-bit target");

unsigned pointerBits() noexcept;
unsigned cxxStandard() noexcept;
const char *architectureName() noexcept;
const char *version() noexcept;

} // namespace build
} // namespace astrabot
