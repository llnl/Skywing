#pragma once

#include <algorithm>
#include <any>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/internal/utility/logging.hpp"

namespace skywing::internal
{
/**
 * @brief Class that implements a thread-safe queue data buffer.
 */
template <typename... Ts>
class QueueBuffer
{
public:
    using locker = std::unique_lock<std::mutex>;
    using ValueType = ValueOrTuple<Ts...>;

    QueueBuffer() = default;

    /**
     * @brief QueueBuffer should be copyable.
     *
     * @details Copy ctor locks mutex in object to be copied.
     * Copy is intentionally inside ctor body to lock across
     * copy of the queue.
     */
    QueueBuffer(const QueueBuffer<Ts...>& other)
    {
        locker lock(other.mutex_);
        queue_ = other.queue_;
    }

    /**
     * @brief Assignment operator deleted for simplicity
     * and don't copy a mutex.
     */
    QueueBuffer<Ts...>& operator=(const QueueBuffer<Ts...>&) = delete;

    [[nodiscard]] bool empty() const
    {
        locker lock(mutex_);
        return queue_.empty();
    }

    size_t size() const
    {
        locker lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Pops and retrieves value from front of queue if available,
     * else returns false. Combines std::queue pop() and front()
     * into one member function for thread safety.
     *
     *
     * @param value reference to variable to receive the popped value if
     * available. Intention is to not pass pointers or references outside of
     * scope of lock inside this member function.
     * @return boolean returns true if value was popped and retrieved from
     * queue, else false.
     */
    [[nodiscard]] bool try_pop(ValueType& value)
    {
        locker lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        SKYWING_DEBUG_LOG("Popped value from Queue Buffer : {}", value);
        return true;
    }

    /**
     * @brief Pops and retrieves value from front of queue.
     * Variant of try_pop() but guarantees value by waiting
     * until the queue is not empty.
     *
     * @param value reference to variable to receive popped value.
     */
    void wait_and_pop(ValueType& value)
    {
        locker lock(mutex_);
        data_cond_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
        SKYWING_DEBUG_LOG("Popped value from Queue Buffer : {}", value);
    }

    void push(const ValueType& value)
    {
        locker lock(mutex_);
        queue_.push(value);
        SKYWING_DEBUG_LOG("Pushed value into Queue Buffer : {}", value);
        data_cond_.notify_one();
    }

    void swap(QueueBuffer<Ts...> other) noexcept
    {
        if (this == &other) {
            return;
        }
        std::scoped_lock lock(mutex_, other.mutex_);
        std::swap(queue_, other.queue_);

        // Notify all threads that state was changed.
        // i.e. If queue state has changed on an object other threads
        // need to wake back up and check.
        data_cond_.notify_all();
        other.data_cond_.notify_all();
    }

    friend void swap(QueueBuffer<Ts...>& curr,
                     QueueBuffer<Ts...>& other) noexcept
    {
        curr.swap(other);
    }

private:
    std::queue<ValueType> queue_;
    std::condition_variable data_cond_;
    /**
     * @brief The mutex to protect access to the queue.
     *
     * @details mutable because it is locked inside const member
     * functions empty() and copy ctor.
     */
    mutable std::mutex mutex_;
};

template <typename... Ts>
[[nodiscard]] bool has_data(const QueueBuffer<Ts...>& buffer) noexcept
{
    return !buffer.empty();
}

template <typename... Ts>
void reset(QueueBuffer<Ts...>& buffer) noexcept
{
    QueueBuffer<Ts...> empty;
    using std::swap;
    swap(buffer, empty);
}

template <typename... Ts>
void get(QueueBuffer<Ts...>& buffer, std::any& value)
{
    auto casted_value = detail::cast_to_value_or_tuple<Ts...>(value);
    buffer.wait_and_pop(casted_value);
    value = casted_value;
}

template <typename... Ts>
void add(QueueBuffer<Ts...>& buffer,
         std::span<const PublishValueVariant> value,
         [[maybe_unused]] const VersionID version) noexcept
{
    assert(
        detail::span_is_valid<Ts...>(value, std::index_sequence_for<Ts...>{}));
    auto new_value =
        detail::make_value<Ts...>(value, std::index_sequence_for<Ts...>{});
    buffer.push(new_value);
}

} // namespace skywing::internal