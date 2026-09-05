// SPDX-License-Identifier: MPL-2.0
#include "nav/io/mesh_loader.hpp"
#include "nav/model/mesh_validator.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace astrabot::nav::detail {
class NavMeshBuilder final {
  public:
    static std::shared_ptr<const model::NavMeshSnapshot>
    build(model::NavFileHeader header, std::vector<model::NavAreaRecord> areas) {
        return std::shared_ptr<const model::NavMeshSnapshot>(
            new model::NavMeshSnapshot(std::move(header), std::move(areas)));
    }
};
} // namespace astrabot::nav::detail
namespace astrabot::nav::io {
diagnostics::ReadResult<std::shared_ptr<const model::NavMeshSnapshot>>
NavMeshLoader::load(ByteView bytes, const NavMeshReadLimits &limits) noexcept {
    using namespace diagnostics;
    using Result = ReadResult<std::shared_ptr<const model::NavMeshSnapshot>>;
    NavError at{NavErrorKind::None, 0, NavRecord::RawInput, NavField::RawBytes};
    if (!bytes.isValid()) {
        at.kind = NavErrorKind::InvalidInput;
        return Result::failure(at);
    }
    if (bytes.size > limits.maxInputBytes) {
        at.kind = NavErrorKind::CountLimitExceeded;
        return Result::failure(at);
    }
    detail::DecodeContext context;
    context.maximum = limits.maxSnapshotBytes;
    const auto budget = context.charge(1, sizeof(model::NavMeshSnapshot), at);
    if (!budget.isNone())
        return Result::failure(budget);
    try {
        auto headerLimits = limits.header;
        headerLimits.maxAreas = std::min(headerLimits.maxAreas, limits.areas.maxAreas);
        auto header = NavFileReader::readTracked(bytes, headerLimits, &context);
        if (!header) {
            return Result::failure(header.error);
        }
        context.base = header.value->headerBytes;
        auto block = NavAreaReader::readTracked(
            {bytes.data + context.base, bytes.size - context.base}, header.value->version,
            header.value->areaCount, limits.areas, &context);
        if (!block) {
            block.error.offset += context.base;
            return Result::failure(block.error);
        }
        at = {NavErrorKind::AllocationFailure, context.base, NavRecord::Area, NavField::AreaId};
        auto validation = detail::validateMesh(*header.value, block.value->areas, context);
        if (!validation.isNone())
            return Result::failure(validation);
        const auto end = context.base + block.value->bytesConsumed;
        if (end != bytes.size)
            return Result::failure(
                {NavErrorKind::TrailingData, end, NavRecord::RawInput, NavField::RawBytes});
        at = {NavErrorKind::AllocationFailure, 0, NavRecord::RawInput, NavField::RawBytes};
        return Result::success(
            detail::NavMeshBuilder::build(std::move(*header.value), std::move(block.value->areas)));
    } catch (const std::bad_alloc &) {
        return Result::failure(at);
    } catch (const std::length_error &) {
        return Result::failure(at);
    }
}
} // namespace astrabot::nav::io
