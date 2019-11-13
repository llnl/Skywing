#ifndef SKYNET_INTERNAL_TAG_BUFFER_HPP
#define SKYNET_INTERNAL_TAG_BUFFER_HPP

#include "skynet/internal/utility/type_list.hpp"
#include "skynet/types.hpp"

#include <cassert>
#include <optional>
#include <vector>

namespace skynet::internal
{
  /** \brief The value to pass to use default behavior for
   * versions when publishing and subscribing.
   */
  inline static constexpr VersionID tag_default_version = -1;

  namespace detail
  {
    constexpr VersionID update_version(const VersionID to_update, const VersionID new_version) noexcept
    {
      return
        new_version == tag_default_version
          ? to_update + 1
          : new_version;
    }

    // This information is the same for all instances of DiscardOldVersionTagBuffer,
    // so take it out of the template and into a normal structure
    class DiscardOldVersionTagBufferBase
    {
    protected:
      /** Returns true if data is present for the specified version, false if
       * it is not available.
       */
      bool has_data(const VersionID required_version = tag_default_version) const noexcept
      {
        return
          stored_version_ != tag_default_version &&
          stored_version_ >= update_version(last_fetched_version_, required_version);
      }

      VersionID stored_version_ = tag_default_version;
      VersionID last_fetched_version_ = tag_default_version;
    }; // class DiscardOldVersionTagBufferBase
  } // namespace skynet::internal::detail

  /** \brief Buffer for a tag that only keeps the latest version that has
   * been recieved.
   */
  template<typename T>
  class DiscardOldVersionTagBuffer : private detail::DiscardOldVersionTagBufferBase
  {
  public:
    using DiscardOldVersionTagBufferBase::has_data;

    /** Get the stored data
     *
     * \pre There is stored data
     */
    T& get() noexcept
    {
      assert(this->has_data());
      this->last_fetched_version_ = this->stored_version_;
      return value_;
    }

    /** Add data if the version is newer
     */
    void add(T value, const VersionID version) noexcept
    {
      if (version > this->stored_version_ || this->stored_version_ == tag_default_version)
      {
        this->stored_version_ = version;
        value_ = std::move(value);
      }
    }

  private:
    T value_;
  }; // class DiscardOldVersionTagBuffer

  /** \brief Buffer for a tag that keeps all new recieved versions, and returns
   * them in order.  Discards old or already recieved tags.
   */
  template<typename T>
  class FifoTagBuffer
  {
  public:
    /** Get the oldest stored data, removing it from the buffer
     *
     * \pre Data can be retrieved for the specified version
     */
    T get(const VersionID required_version = tag_default_version) noexcept
    {
      while (true)
      {
        assert(!buffer_.empty());
        auto [data, version] = std::move(buffer_.front());
        buffer_.erase(buffer_.begin());
        if (version >= detail::update_version(last_fetched_version_, required_version))
        {
          last_fetched_version_ = version;
          return std::move(data);
        }
      }
    }

    /** Returns true if data can be retrieved for the specified version, false
     * if it is not available.
     */
    bool has_data(const VersionID required_version = tag_default_version) const noexcept
    {
      return !buffer_.empty() &&
        buffer_.back().second >= detail::update_version(last_fetched_version_, required_version);
    }

    /** Adds data to the buffer if the version is newer than the last version
     */
    void add(T value, const VersionID version) noexcept
    {
      if (version > last_stored_version_ || last_stored_version_ == tag_default_version)
      {
        buffer_.emplace_back(std::move(value), version);
      }
    }

  private:
    std::vector<std::pair<T, VersionID>> buffer_;
    VersionID last_stored_version_ = tag_default_version;
    VersionID last_fetched_version_ = tag_default_version;
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_TAG_BUFFER_HPP
