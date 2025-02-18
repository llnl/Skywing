#pragma once

#include <cstdint>

#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/internal/most_recent_buffer.hpp"
#include "skywing_core/internal/queue_buffer.hpp"

namespace skywing
{

using namespace skywing;

/**
 * @class Buffer
 * @brief A class representing external API for setting buffer types.
 * 
 * @details
 * This class has static member functions to be used as factories with the
 * intent of letting users to create Buffers with common data types.
 * 
 * Users can set the buffer type easily with the syntax below
 * 
 *     // creates a queue buffer for data type double
 *     job.set_buffer( Buffer::DoubleQueue() );
 *     
 * For custom data types, such as multiple data types for a data stream,
 * users can use internal buffer classes MostRecentBuffer and QueueBuffer
 * template classes which are templated on data type. These classes are
 * located under skywing/skywing_core/internal directory.
 * 
 *     MostRecentBuffer<int, double> buffer();
 *     job.set_buffer(buffer);
 */
class Buffer
{
public:
    explicit Buffer(internal::Buffer& buf) : buffer_{buf} {}

    explicit Buffer(internal::Buffer&& buf) : buffer_{std::move(buf)} {}

    static Buffer IntQueue() { return Buffer(internal::QueueBuffer<int>()); }

    static Buffer DoubleQueue()
    {
        return Buffer(internal::QueueBuffer<double>());
    }

    static Buffer StringQueue()
    {
        return Buffer(internal::QueueBuffer<std::string>());
    }

    static Buffer Int32Queue()
    {
        return Buffer(internal::QueueBuffer<std::int32_t>());
    }

    static Buffer UInt32Queue()
    {
        return Buffer(internal::QueueBuffer<std::uint32_t>());
    }

    static Buffer Int64Queue()
    {
        return Buffer(internal::QueueBuffer<std::int64_t>());
    }

    static Buffer UInt64Queue()
    {
        return Buffer(internal::QueueBuffer<std::uint64_t>());
    }

    static Buffer IntMostRecent()
    {
        return Buffer(internal::MostRecentBuffer<int>());
    }

    static Buffer DoubleMostRecent()
    {
        return Buffer(internal::MostRecentBuffer<double>());
    }

    static Buffer StringMostRecent()
    {
        return Buffer(internal::MostRecentBuffer<std::string>());
    }

    static Buffer Int32MostRecent()
    {
        return Buffer(internal::MostRecentBuffer<std::int32_t>());
    }

    static Buffer UInt32MostRecent()
    {
        return Buffer(internal::MostRecentBuffer<std::uint32_t>());
    }

    static Buffer Int64MostRecent()
    {
        return Buffer(internal::MostRecentBuffer<std::int64_t>());
    }

    static Buffer UInt64MostRecent()
    {
        return Buffer(internal::MostRecentBuffer<std::uint64_t>());
    }

    internal::Buffer get_buffer() const { return buffer_; }

private:
    internal::Buffer buffer_;
};

} // namespace skywing