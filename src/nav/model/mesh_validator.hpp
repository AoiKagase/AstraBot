// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/io/decode_context.hpp"
#include "nav/model/area_records.hpp"
#include "nav/model/file_header.hpp"
namespace astrabot::nav::detail {
diagnostics::NavError validateMesh(const model::NavFileHeader &header,
                                   const std::vector<model::NavAreaRecord> &areas,
                                   const DecodeContext &context);
}
