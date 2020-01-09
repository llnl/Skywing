#ifndef SKYNET_INTERNAL_TAG_BUFFER_HPP
#define SKYNET_INTERNAL_TAG_BUFFER_HPP

#include "skynet/internal/utility/type_list.hpp"
#include "skynet/types.hpp"

#include <cassert>
#include <optional>
#include <vector>

namespace skynet::internal
{
  inline static constexpr VersionID tag_no_data = -1;

  namespace detail
  {

    // This information is the same for all instances of DiscardOldVersionTagBuffer,
    // so take it out of the template and into a normal structure
    class DiscardOldVersionTagBufferBase
    {
    protected:
      /** Returns true if data is present for the specified version, false if
       * it is not available.
       */
      bool has_data() const noexcept
      {
        return stored_version_ != tag_no_data
          && stored_version_ >= last_fetched_version_ + 1;
      }

      VersionID stored_version_ = tag_no_data;
      VersionID last_fetched_version_ = tag_no_data;
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
      if (version > this->stored_version_ || this->stored_version_ == tag_no_data)
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
    T get(const VersionID required_version) noexcept
    {
      while (true)
      {
        assert(!buffer_.empty());
        auto [data, version] = std::move(buffer_.front());
        buffer_.erase(buffer_.begin());
        if (version >= required_version)
        {
          last_fetched_version_ = version;
          return std::move(data);
        }
      }
    }

    /** Returns true if data can be retrieved
     */
    bool has_data(const VersionID required_version) const noexcept
    {
      return !buffer_.empty() &&
        buffer_.back().second >= required_version;
    }

    /** Adds data to the buffer if the version is newer than the last version
     */
    void add(T value, const VersionID version) noexcept
    {
      if (version > last_stored_version_ || last_stored_version_ == tag_no_data)
      {
        buffer_.emplace_back(std::move(value), version);
      }
    }

  private:
    std::vector<std::pair<T, VersionID>> buffer_;
    VersionID last_stored_version_ = tag_no_data;
    VersionID last_fetched_version_ = tag_no_data;
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_TAG_BUFFER_HPP
