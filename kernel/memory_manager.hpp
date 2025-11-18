/**
 * @file memory_manager.hpp
 *
 * @brief Memory management utilities for the kernel.
 */

#pragma once

#include <array>
#include <limits>

#include "error.hpp"

namespace
{
    constexpr unsigned long long operator""_KiB(unsigned long long kib)
    {
        return kib * 1024;
    }

    constexpr unsigned long long operator""_MiB(unsigned long long mib)
    {
        return mib * 1024_KiB;
    }

    constexpr unsigned long long operator""_GiB(unsigned long long gib)
    {
        return gib * 1024_MiB;
    }
}

/** @brief Number of bytes per frame in physical memory */
static const auto kBytesPerFrame{4_KiB};

class FrameID
{
public:
    explicit FrameID(size_t id) : id_{id} {}
    size_t ID() const { return id_; }
    void *Frame() const
    {
        return reinterpret_cast<void *>(id_ * kBytesPerFrame);
    }

private:
    size_t id_;
};

static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

/** @brief Class for managing memory by unit using bitmap
 *
 * This class manages available frames by bitmap with corresponding a bit to a frame
 * each alloc_map indexes correspond to a frame, 0 means free, 1 means using.
 * The physical address of m indexed bit in alloc_map[n] is calculated as:
 * kFrameBytes * (n * kBitesPerMapLine + m)
 */
class BitmapMemoryManager
{
public:
    /** @brief The maximum physical memory size in bytes managed by the bitmap */
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    /** @brief The number of frames needed to manage physical memory to kMaxPhysicalMemoryBytes */
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

    /** @brief The type of element in bitmap array */
    using MapLineType = unsigned long;
    /** @brief The bit count of a element in bitmap array = frame count */
    static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};

    /** @brief Initialize the instance */
    BitmapMemoryManager();

    /** @brief Acquire demand frame resources, return the head of allocated frame ID */
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);
    void MarkAllocated(FrameID start_frame, size_t num_frames);

    /** @brief Set the manageable memory range
     * After calling this method, memory allocation by Allocate() is possible on the range
     *
     * @param range_begin The first frame ID of manageable memory range
     * @param range_end The last frame ID of manageable memory range, the next frame of the last frame
     */
    void SetMemoryRange(FrameID range_begin, FrameID range_end);

private:
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;

    /** @brief The start of memory range */
    FrameID range_begin_;
    /** @brief The end of memory range, the next frame of the last frame */
    FrameID range_end_;

    bool GetBit(FrameID frame) const;
    void SetBit(FrameID frame, bool allocated);
};

Error InitializeHeap(BitmapMemoryManager &memory_manager);