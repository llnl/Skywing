#ifndef SKYNET_INTERNAL_UTILITY_ALGORITHMS
#define SKYNET_INTERNAL_UTILITY_ALGORITHMS

#include <iterator>
#include <string_view>
#include <string>
#include <type_traits>
#include <vector>

namespace skynet::internal
{
  namespace detail
  {
    /** \brief Structure for selecting priorities for an overload set
     *
     * Add as a parameter, where higher numbered tags will attempt to be called
     * first before later ones.
     */
    template<std::size_t N>
    struct PriorityTag : PriorityTag<N - 1> {};

    template<>
    struct PriorityTag<0> {};

    /** \brief Merge is available - use it
     */
    template<typename T>
    auto merge_impl(T& lhs, T& rhs, PriorityTag<1>) noexcept -> decltype((void)lhs.merge(rhs))
    {
      lhs.merge(rhs);
    }

    /** \brief Merge is not available, use this as a workaround.
     */
    template<typename T>
    void merge_impl(T& lhs, T& rhs, PriorityTag<0>) noexcept
    {
      lhs.insert(rhs.begin(), rhs.end());
      rhs.clear();
    }
  } // namespace detail

  /** \brief Concatenates many containers into a single vector
   */
  template<typename... Ts>
  std::vector<std::common_type_t<typename Ts::value_type...>> concatenate(const Ts&... containers) noexcept
  {
    using ::std::size;
    using ::std::begin;
    using ::std::cbegin;
    using ::std::cend;
    // Allocate enough space for all the data at the start
    ::std::vector<::std::common_type_t<typename Ts::value_type...>> to_ret((size(containers) + ...));
    // Copy all of the data
    auto copy_loc = begin(to_ret);
    (..., (copy_loc = ::std::copy(cbegin(containers), cend(containers), copy_loc)));
    return to_ret;
  }

  /** \brief Merge two associative containers together
   *
   * This is only needed as a workaround for when the merge method isn't supported.
   */
  template<typename T>
  void merge_associative_containers(T& lhs, T& rhs) noexcept
  {
    return detail::merge_impl(lhs, rhs, detail::PriorityTag<1>{});
  }

  /** \brief Split a string based on a character
   */
  std::vector<std::string_view> split(const std::string& to_split, const char split_char) noexcept
  {
    std::vector<std::string_view> to_ret;
    auto start_pos = to_split.find(split_char);
    auto last_pos = 0;
    for (auto pos = start_pos; pos != std::string::npos; pos = to_split.find(split_char, pos + 1))
    {
      // to_ret.emplace_back(to_split.data() + last_pos, to_split.data() + pos);
      to_ret.emplace_back(to_split.data() + last_pos, pos - last_pos);
      last_pos = pos + 1;
    }
    // to_ret.emplace_back(to_split.data() + last_pos, to_split.data() + to_split.size());
    to_ret.emplace_back(to_split.data() + last_pos, to_split.size() - last_pos);
    return to_ret;
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_ALGORITHMS
