#include "skynet/internal/utility/serialize.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iterator>

// Intrinsics information pulled from
// https://github.com/boostorg/endian/blob/develop/include/boost/endian/detail/intrinsic.hpp
// These SHOULD be in the anonymous namespace, but Clang will complain about them
// being unused functions if that's the case. (Maybe want to define them iff the machine
// if big endian?)
#ifdef _MSC_VER
  std::uint16_t byte_swap(std::uint16_t val) noexcept { return _byteswap_ushort(val); }
  std::uint32_t byte_swap(std::uint32_t val) noexcept { return _byteswap_ulong(val);  }
  std::uint64_t byte_swap(std::uint64_t val) noexcept { return _byteswap_uint64(val); }
  std::int16_t  byte_swap(std::int16_t val)  noexcept { return _byteswap_ushort(val); }
  std::int32_t  byte_swap(std::int32_t val)  noexcept { return _byteswap_ulong(val);  }
  std::int64_t  byte_swap(std::int64_t val)  noexcept { return _byteswap_uint64(val); }
#else
  std::uint16_t byte_swap(std::uint16_t val) noexcept { return __builtin_bswap16(val); }
  std::uint32_t byte_swap(std::uint32_t val) noexcept { return __builtin_bswap32(val); }
  std::uint64_t byte_swap(std::uint64_t val) noexcept { return __builtin_bswap64(val); }
  std::int16_t  byte_swap(std::int16_t val)  noexcept { return __builtin_bswap16(val); }
  std::int32_t  byte_swap(std::int32_t val)  noexcept { return __builtin_bswap32(val); }
  std::int64_t  byte_swap(std::int64_t val)  noexcept { return __builtin_bswap64(val); }
#endif

namespace
{
  // Determine if a type needs swapping
  template<typename T>
  constexpr bool needs_swapping()
  {
    // Determine if the machine is big or little endian, method from
    // https://en.cppreference.com/w/cpp/types/endian
    #ifdef _WIN32
      constexpr bool is_little_endian = true;
    #else
      constexpr bool is_little_endian = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
    #endif
    return !is_little_endian && std::is_integral_v<T> && sizeof(T) > 1;
  }

  // Converts a number to little endian, if needed
  template<typename T>
  T to_little_endian(T value) noexcept
  {
    // only matters for integral types larger than one and
    // if the machine is little endian it's a no-op
    if constexpr (needs_swapping<T>())
    {
      return byte_swap(value);
    }
    else
    {
      return value;
    }
  }

  // Lazy shortcut for const std::vector<type>
  template<typename T>
  using cvec = const std::vector<T>;

  // Lazy shortcut for std::vector<Type>
  template<typename T>
  using vec = std::vector<T>;

  // Turn a type into an array of bytes
  template<typename T>
  std::array<std::byte, sizeof(T)> to_bytes(const T& val) noexcept
  {
    std::array<std::byte, sizeof(T)> buffer;
    const T temp = to_little_endian(val);
    std::memcpy(buffer.data(), &temp, sizeof(T));
    return buffer;
  }

  // Get a type from some bytes
  template<typename T>
  T from_bytes(const std::byte* buffer) noexcept
  {
    T temp;
    std::memcpy(&temp, buffer, sizeof(T));
    return to_little_endian(temp);
  }
} // end anonymous namespace

namespace skynet::internal
{
  /////////////////////////////////////////
  // Serializer (aside from add)
  /////////////////////////////////////////
  class Serializer::Impl
  {
  public:
    // Object version
    template<typename T>
    void add(const T& val) noexcept
    {
      // copy the raw bytes to a buffer and then append that buffer
      const auto raw_buffer = to_bytes(val);
      std::copy(raw_buffer.cbegin(), raw_buffer.cend(), std::back_inserter(data_));
    }

    // Vector version
    template<typename T>
    void add(const std::vector<T>& vals) noexcept
    {
      // Start with the size
      add(vals.size());
      // Then serialize each member
      if constexpr (needs_swapping<T>())
      {
        // Must convert each value to the appropriate format
        for (const auto& val : vals)
        {
          add(val);
        }
      }
      else if (vals.size() > 0)
      {
        // Can just add copy over the raw bytes
        const auto old_size = data_.size();
        data_.resize(data_.size() + sizeof(T) * vals.size());
        std::memcpy(data_.data() + old_size, vals.data(), sizeof(T) * vals.size());
      }
    }

    // Get the bytes
    const std::vector<std::byte>& bytes() const& noexcept { return data_; }
    std::vector<std::byte> bytes() && noexcept { return std::move(data_); }

  private:
    std::vector<std::byte> data_;
  }; // class Serializer::Impl

  Serializer::Serializer() noexcept
    : impl_{std::make_unique<Impl>()}
  {}

  Serializer::~Serializer() = default;

  std::vector<std::byte> Serializer::bytes() const noexcept
  {
    return impl_->bytes();
  }

  /////////////////////////////////////////
  // Deserializer (aside from get)
  /////////////////////////////////////////

  class Deserializer::Impl
  {
  public:
    // Constructor
    Impl(const std::byte* const data, const std::size_t num_bytes) noexcept
      : data_{data}
      , num_bytes_{num_bytes}
    {}

    // Object version
    template<typename T>
    T get() noexcept
    {
      // Don't go off the end
      assert(loc_ + sizeof(T) <= num_bytes_);
      // Otherwise just read the data and advance the location
      const auto val = from_bytes<T>(data_ + loc_);
      loc_ += sizeof(T);
      return val;
    }

    // Vector version
    template<typename T>
    std::vector<T> get_vec() noexcept
    {
      // Start by reading the size
      const auto size = get<std::size_t>();
      std::vector<T> to_ret(size);
      // Then read each of the members
      if constexpr (needs_swapping<T>())
      {
        for (auto& val : to_ret)
        {
          val = get<T>();
        }
      }
      else if (size > 0)
      {
        // no need to convert, can just memcpy
        assert(loc_ + sizeof(T) * size <= num_bytes_);
        std::memcpy(to_ret.data(), data_ + loc_, size);
        loc_ += sizeof(T) * size;
      }
      return to_ret;
    }
  private:
    const std::byte* data_;
    std::size_t num_bytes_;
    std::size_t loc_{0};
  }; // class Deserializer::Impl

  Deserializer::Deserializer(const std::byte* const data, const std::size_t num_bytes)
    : impl_{std::make_unique<Impl>(data, num_bytes)}
  {}

  Deserializer::~Deserializer() = default;

  /////////////////////////////////////////////
  // Definitions for Add/Get
  /////////////////////////////////////////////

  // Macro to define a certain type for the add/get functions
  #define MAKE_ADD_GET(type) \
    Serializer& Serializer::add(type val)        noexcept { impl_->add(val); return *this; } \
    Serializer& Serializer::add(cvec<type>& val) noexcept { impl_->add(val); return *this; } \
    type& Deserializer::get(type& val)           noexcept { val = impl_->get<type>(); return val; } \
    vec<type>& Deserializer::get(vec<type>& val) noexcept { val = impl_->get_vec<type>(); return val; }

  MAKE_ADD_GET(short);
  MAKE_ADD_GET(int);
  MAKE_ADD_GET(long);
  MAKE_ADD_GET(long long);
  MAKE_ADD_GET(unsigned short);
  MAKE_ADD_GET(unsigned int);
  MAKE_ADD_GET(unsigned long);
  MAKE_ADD_GET(unsigned long long);
  MAKE_ADD_GET(float);
  MAKE_ADD_GET(double);
  MAKE_ADD_GET(std::uint8_t);

  // All the add and get functions have been defined, get rid of the macro
  // (Doesn't really matter since this is a source file, but just to be safe)
  #undef MAKE_ADD_GET
} // namespace skynet::internal
