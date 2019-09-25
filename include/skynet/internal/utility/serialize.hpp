#ifndef SKYNET_INTERNAL_UTILITY_SERIALIZE_HPP
#define SKYNET_INTERNAL_UTILITY_SERIALIZE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

namespace skynet::internal
{
  /** \brief A class for serialization of data
   */
  class Serializer
  {
  public:
    /** \brief Default constructor
     */
    Serializer() noexcept;

    // Must be declared because of PIMPL
    ~Serializer();

    // Yes, this is gross, but can't see another way of doing this...
    /** \brief Functions to add data the be serialized
     * Returns a reference to the calling object so adds can be chained, etc.
     */
    Serializer& add(short) noexcept;
    Serializer& add(const std::vector<short>&) noexcept;
    Serializer& add(int) noexcept;
    Serializer& add(const std::vector<int>&) noexcept;
    Serializer& add(long) noexcept;
    Serializer& add(const std::vector<long>&) noexcept;
    Serializer& add(long long) noexcept;
    Serializer& add(const std::vector<long long>&) noexcept;
    Serializer& add(unsigned short) noexcept;
    Serializer& add(const std::vector<unsigned short>&) noexcept;
    Serializer& add(unsigned int) noexcept;
    Serializer& add(const std::vector<unsigned int>&) noexcept;
    Serializer& add(unsigned long) noexcept;
    Serializer& add(const std::vector<unsigned long>&) noexcept;
    Serializer& add(unsigned long long) noexcept;
    Serializer& add(const std::vector<unsigned long long>&) noexcept;
    Serializer& add(float) noexcept;
    Serializer& add(const std::vector<float>&) noexcept;
    Serializer& add(double) noexcept;
    Serializer& add(const std::vector<double>&) noexcept;
    Serializer& add(std::uint8_t) noexcept;
    Serializer& add(const std::vector<std::uint8_t>&) noexcept;

    /** \brief Add many values to the serializer
     */
    // Only use this overload if more than one parameter is passed
    template<typename... Ts>
    std::enable_if_t<(sizeof...(Ts) > 1), Serializer&> add(const Ts&... values)
    {
      // Call add for each parameter
      (add(values), ...);
      return *this;
    }

    /** \brief Retrieves the serialized data
     */
    std::vector<std::byte> bytes() const noexcept;

  private:
    // PIMPL so that headers can be excluded
    class Impl;
    std::unique_ptr<Impl> impl_;
  }; // class Serializer

  /** \brief A class for deserialization of data
   */
  class Deserializer
  {
  public:
    /** \brief Create a deserializer from raw bytes
     */
    Deserializer(const std::byte* data, std::size_t count);

    /** \brief Create a deserializer from a continguous container of raw bytes
     */
    template<typename Container>
    explicit Deserializer(const Container& c)
      : Deserializer{c.data(), c.size()}
    {}

    // Must be declared because of PIMPL
    ~Deserializer();

    // Yes, gross yet again
    /** \brief Retrieve a value from the deserializer, putting it into the
     * passed parameter and also returning a reference to it
     */
    short& get(short&) noexcept;
    std::vector<short>& get(std::vector<short>&) noexcept;
    int& get(int&) noexcept;
    std::vector<int>& get(std::vector<int>&) noexcept;
    long& get(long&) noexcept;
    std::vector<long>& get(std::vector<long>&) noexcept;
    long long& get(long long&) noexcept;
    std::vector<long long>& get(std::vector<long long>&) noexcept;
    unsigned short& get(unsigned short&) noexcept;
    std::vector<unsigned short>& get(std::vector<unsigned short>&) noexcept;
    unsigned int& get(unsigned int&) noexcept;
    std::vector<unsigned int>& get(std::vector<unsigned int>&) noexcept;
    unsigned long& get(unsigned long&) noexcept;
    std::vector<unsigned long>& get(std::vector<unsigned long>&) noexcept;
    unsigned long long& get(unsigned long long&) noexcept;
    std::vector<unsigned long long>& get(std::vector<unsigned long long>&) noexcept;
    float& get(float&) noexcept;
    std::vector<float>& get(std::vector<float>&) noexcept;
    double& get(double&) noexcept;
    std::vector<double>& get(std::vector<double>&) noexcept;
    std::uint8_t& get(std::uint8_t&) noexcept;
    std::vector<std::uint8_t>& get(std::vector<std::uint8_t>&) noexcept;

    /** \brief Get many values from the serializer
     */
    // Only enable this overload if more than one parameter is passed
    template<typename... Ts>
    std::enable_if_t<(sizeof...(Ts) > 1)> get(Ts&... values)
    {
      // Call get for each parameter
      (get(values), ...);
    }

  private:
    // PIMPL to exclude headers
    class Impl;
    std::unique_ptr<Impl> impl_;
  }; // class Deserializer

  /** \brief Get a value from a stream of bytes
   */
  template<typename T>
  T deserialize(const std::vector<std::byte>& data) noexcept
  {
    T holder;
    return Deserializer{data}.get(holder);
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_SERIALIZE_HPP
