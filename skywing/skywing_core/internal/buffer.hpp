#pragma once

#include <cassert>
#include <concepts>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <any>

#include "skywing_core/internal/most_recent_buffer.hpp"
#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"
#include "skywing_core/internal/utility/logging.hpp"

namespace skywing::internal
{

namespace detail
{

template <typename... Ts>
ValueOrTuple<Ts...> cast_to_value_or_tuple(const std::any& value)
{
    try {
        if (value.type() != typeid(ValueOrTuple<Ts...>)) {
            SKYWING_ERROR_LOG("Error: std::any has type {} expected type is {}",
                              value.type().name(),
                              typeid(ValueOrTuple<Ts...>).name());
        }
        auto casted = std::any_cast<ValueOrTuple<Ts...>>(value);
        return ValueOrTuple<Ts...>(casted);
    }
    catch (const std::bad_any_cast& e) {
        SKYWING_ERROR_LOG("Error: bad cast : std::any does not hold expected "
                          "buffer data type: {}",
                          e.what());
        throw;
    }
}

} // namespace detail

class Buffer
{
private:
    struct BufferInterface
    {
        virtual ~BufferInterface() {}
        virtual bool do_has_data() const noexcept = 0;
        virtual void do_get(std::any& value) noexcept = 0;
        virtual void do_add(std::span<const PublishValueVariant> value,
                            const VersionID version) noexcept = 0;
        virtual void do_reset() noexcept = 0;
        virtual std::unique_ptr<BufferInterface> clone() const = 0;
    };

    /**
     * @class BufferWrapper
     * 
     * @brief Stores a buffer object.
     * 
     * @details BufferWrapper class is a wrapper class around a concrete Buffer
     * class. It's a class template in order to generate a wrapper automatically
     * for any type that adheres to the interface imposed internally by
     * BufferInterface and externally by the C++ concept IsBuffer.
     * 
     * @tparam BufferType The type of buffer.
     * 
     * @tparam Ts A template parameter pack of data types for data stored in the
     * buffer.
     */
    template <template <typename...> typename BufferType, typename... Ts>
    struct BufferWrapper : BufferInterface
    {
        /**
         * @brief Constructs a BufferWrapper object.
         * 
         * @param buffer Any concrete buffer class that implements free
         * functions of the IsBuffer concept.
         */
        BufferWrapper(const BufferType<Ts...>& buffer) : buffer_{buffer} {}

        /** @name Wrapper functions
         * Enforces the interface from BufferInterface and forwards the
         * interface methods to the wrapped concrete type.
         */
        ///@{
        bool do_has_data() const noexcept override { return has_data(buffer_); }

        void do_get(std::any& value) noexcept override { get(buffer_, value); }

        void do_add(std::span<const PublishValueVariant> value,
                    const VersionID version) noexcept override
        {
            add(buffer_, value, version);
        }

        void do_reset() noexcept override { reset(buffer_); }
        ///@}

        /**
         * @brief Creates a copy for the Buffer copy constructor.
         * 
         * @details The prototype design pattern. Copying is done inside
         *          the BufferWrapper because it knows its own concrete type
         *          as copying the base_ptr is too abstract to copy.
         */
        std::unique_ptr<BufferInterface> clone() const override
        {
            return std::make_unique<BufferWrapper<BufferType, Ts...>>(*this);
        }

        BufferType<Ts...> buffer_;
    };

    friend bool has_data(const Buffer& buffer)
    {
        return buffer.base_ptr_->do_has_data();
    }

    friend void get(const Buffer& buffer, std::any& value) noexcept
    {
        buffer.base_ptr_->do_get(value);
    }

    friend void add(const Buffer& buffer,
                    std::span<const PublishValueVariant> value,
                    const VersionID version) noexcept
    {
        buffer.base_ptr_->do_add(value, version);
    }

    friend void reset(const Buffer& buffer) noexcept
    {
        buffer.base_ptr_->do_reset();
    }

    std::unique_ptr<BufferInterface> base_ptr_;

public:
    /**
     * @brief Creates BufferWrapper and type erases it by storing it as a
     * pointer to base class.
     * 
     * @tparam BufferType The type of buffer.
     * 
     * @tparam Ts A template parameter pack of data types for data stored in the
     * buffer.
     * 
     * @param buffer Any concrete buffer class that implements free functions of
     * the IsBuffer concept.
     */
    template <template <typename...> typename BufferType, typename... Ts>
    // TODO add concept IsBuffer to constrain custom Buffer classes
    // to free functions interface : get(), add(), reset(), has_data()
    //    requires IsBuffer<BufferType, Ts...>
    Buffer(const BufferType<Ts...>& buffer)
        : base_ptr_{std::make_unique<BufferWrapper<BufferType, Ts...>>(
              std::move(buffer))}
    {}

    /**
     * @brief A Buffer should be copyable.
     * 
     * @param other The buffer object to be copied.
     */
    Buffer(const Buffer& other) : base_ptr_(other.base_ptr_->clone()) {}

    /**
     * @brief A Buffer should be copyable.
     * 
     * @param other The buffer object to be copied.
     */
    Buffer& operator=(const Buffer& other)
    {
        Buffer tmp(other);
        std::swap(base_ptr_, tmp.base_ptr_);
        return *this;
    }
};

} // namespace skywing::internal
