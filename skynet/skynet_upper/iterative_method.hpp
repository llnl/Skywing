#ifndef SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"

namespace skynet {

/** @brief Base class for iterative methods.
 * 
 * @param DeadNeighborPolicy Determines how this IterativeMethod
 * should respond to realizing a neighbor is dead.
 *
 * @tparam TagValueTypes... Values types this iterative method will
 * publish to neighbors.
 * 
 */
template<typename DeadNeighborPolicy, typename... TagValueTypes>
class IterativeMethod {
public:
  using TagType = PublishTag<TagValueTypes...>;
  using ValueTag = TagType;
  using ValueType = ValueOrTuple<TagValueTypes...>;

  template<typename T>
  using tag_map = std::unordered_map
    <TagType, T, skynet::internal::hash<TagType>>;

  const TagType& my_tag() const { return produced_tag_; }

  template<typename TagIter>
  TagIter handle_dead_neighbor(const TagIter& tag_iter) noexcept
  {
    dead_tags.push_back(std::move(*tag_iter));
    return tags_.erase(tag_iter);
  }
  
  /** @brief Rebuilds connections for dead tags
   */
  Waiter<void> rebuild_dead_tags() noexcept
  {
    auto to_ret = job_->rebuild_tags(dead_tags_);
    std::move(dead_tags_.begin(), dead_tags_.end(), std::back_inserter(tags_));
    dead_tags_.clear();
    return to_ret;
  }

  /** @brief Rebuilds the specified dead tags, ignoring any tags that aren't dead
   */
  template<typename Range>
  Waiter<void> rebuild_dead_tags_range(const Range& r) noexcept
  {
    std::vector<PublishTag<TagValueTypes...>> search_tags;
    for (const auto& tag : r) {
      auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) {
        search_tags.push_back(std::move(*iter));
        dead_tags_.erase(*iter);
      }
    }
    return job_->rebuild_tags(search_tags);
  }

  /** @brief Rebuilds the specified dead tags
   */
  template<typename... Tags>
  Waiter<void> rebuild_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
    return rebuild_dead_tags_range(tag_array);
  }

  /** @brief Drops tracking for dead tags
   */
  void drop_dead_tags() noexcept
  {
    // TODO: Actually unsubscribe when that's a thing that can happen
    dead_tags_.clear();
  }

  /** @brief Drops tracking for specific tags, does nothing if the tags aren't dead
   */
  template<typename Range>
  void drop_dead_tags(const Range& r) noexcept
  {
    for (const auto& tag : r) {
      const auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) { dead_tags_.erase(iter); }
    }
  }

  /** @brief Drops the specified dead tags
   */
  template<typename... Tags>
  void drop_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
  }

  /** @brief Returns all active tags
   */
  const std::vector<PublishTag<TagValueTypes...>>& tags() const noexcept { return tags_; }

  /** @brief Returns all dead tags
   */
  const std::vector<PublishTag<TagValueTypes...>>& dead_tags() const noexcept { return dead_tags_; }

  Job& get_job() const noexcept { return *job_; }



  /****************************************************
   * VALUES INTERFACE
   ***************************************************/

  const tag_map<ValueType>& get_values_unsafe() { return neighbor_values_; }

  template<typename R>
  R sum() {
    return f_sum<R>([](const ValueType& v) { return v; });
  }
  template<typename R>
  R f_sum(std::function<R(const ValueType&)> f) {
    return weighted_f_accumulate_<R, R, true, false, false>
      (std::move(f), 0, std::plus<R>(), nullptr);
  }
  template<typename R, typename S>
  R weighted_f_sum(std::function<R(const ValueType&)> f, tag_map<S> coeffs)
  {
    return weighted_f_accumulate_<R, S, true, true, false>
      (std::move(f), [&](const ValueTag& t){ return coeffs[t];}, std::plus<R>(), nullptr);
  }

  template<typename R>
  R average() {
    return sum<R>() / tags_.size();
  }
  template<typename R>
  R f_average(std::function<R(const ValueType&)> f) {
    return f_sum<R>(std::move(f)) / tags_.size();
  }
  template<typename R, typename S>
  R weighted_f_average(std::function<R(const ValueType&)> f, tag_map<S> coeffs)
  {
    R num =  weighted_f_sum<R, S>(std::move(f), coeffs);
    R denom = weighted_f_accumulate_<R, S, false, true, false>
      (0, [&](const ValueTag& t){ return coeffs[t];}, std::plus<R>(), nullptr);
    return num / denom;
  }

  template<typename R>
  R f_accumulate(std::function<R(const ValueType&)> f,
                 std::function<R(R, R)> binary_op)
  {
    return weighted_f_accumulate_<R, R, true, false, false>(std::move(f), std::move(binary_op));
  }
  template<typename R>
  R f_max(std::function<R(const ValueType&)> f)
  {
    return f_accumulate<R>(std::move(f), [](R x, R y){return std::max(x, y);});
  }
  template<typename R>
  R f_min(std::function<R(const ValueType&)> f)
  {
    return f_accumulate<R>(std::move(f), [](R x, R y){return std::min(x, y);});
  }


protected:

  /** @param job The job running this iterative method.
   *  @param produced_tag The tag this agent will publish during iteration.
   *  @param tags The set of tags consumed during iteration, possibly including @p produced_tag
   */
  IterativeMethod(
    Job& job,
    const PublishTag<TagValueTypes...>& produced_tag,
    const std::vector<PublishTag<TagValueTypes...>>& tags) noexcept
    : job_{&job}, produced_tag_{produced_tag}, tags_{tags}
  {
    for (auto tag_iter = tags_.begin(); tag_iter != tags_.end();) {
      if (!job_->tag_has_active_publisher(*tag_iter)) {
        dead_tags_.push_back(std::move(*tag_iter));
        tag_iter = tags_.erase(tag_iter);
      }
      else {
        ++tag_iter;
      }
    }
  }

  template<typename R, typename S,
           bool use_f, bool use_coef>
  std::function<R(const ValueTag&)>
  get_sum_contributor_(std::function<R(const ValueType&)>& f,
                       std::function<S(const ValueTag&)>& coef)
  {
    static_assert(use_f || use_coef);
    if constexpr (!use_f) {
        return [&](const ValueTag& t) { return coef(t); };
    }
    else if (!use_coef) {
      return [&](const ValueTag& t) { return f(this->neighbor_values_[t]); };
    }
    else return [&](const ValueTag& t) { return coef(t) * f(this->neighbor_values_[t]); };
  }

  template<typename R, typename S, bool use_f, bool use_coef, bool use_shift>
  R weighted_f_accumulate_(std::function<R(const ValueType&)> f,
                           std::function<S(const ValueTag&)> coef,
                           std::function<R(R, R)> binary_op,
                           R* shift)
  {
    auto contributor = get_sum_contributor_<R, S, use_f, use_coef>(f, coef);
    
    auto tag_iter = tags_.cbegin();
    R val = [&]{
      if constexpr (use_shift) return binary_op(*shift, contributor(*tag_iter));
      else return contributor(*tag_iter);
    }();
    ++tag_iter;
    
    for (; tag_iter != tags_.cend(); ++tag_iter)
      val = binary_op(std::move(val), std::move(contributor(*tag_iter)));

    return val;
  }

  Job* job_;
  PublishTag<TagValueTypes...> produced_tag_;
  std::vector<PublishTag<TagValueTypes...>> tags_;
  std::vector<PublishTag<TagValueTypes...>> dead_tags_;

private:
  tag_map<ValueType> neighbor_values_;
  
}; // class IterativeBase

} // namespace skynet

#endif // SKYNET_UPPER_INTERNAL_ITERATIVE_BASE_HPP
