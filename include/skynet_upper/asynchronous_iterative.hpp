#ifndef SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/pending_iterative.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace skynet
{
  namespace internal
  {
    /** \brief Sentinel class for iterators; separate from the AsynchronousValues
     * class as it doesn't need to be templated.
     */
    struct AsynchronousValuesSentinel {};
  }

  /** \brief Value for a single AsynchronousIterative value.
   *
   * It is separate from the AsynchronousValues class in order to support
   * structured bindings
   */
  template<typename ValueType>
  class AsynchronousElement {
  public:
    AsynchronousElement(const ValueType* value, bool was_updated) noexcept
      : value_{value}
      , was_updated_{was_updated}
    {}

    /** \brief Returns the value associated with the index
     */
    const ValueType& value() const noexcept { return *value_; }

    /** \brief Returns if the value associated with the index has been updated
     */
    bool was_updated() const noexcept { return was_updated_; }

    // Structured binding support
    template<std::size_t I>
    std::conditional_t<I == 0, const ValueType&, bool> get() const noexcept
    {
      static_assert(I == 0 || I == 1);
      if constexpr (I == 0) { return value();       }
      else                  { return was_updated(); }
    }

  private:
    const ValueType* value_;
    bool was_updated_;
  }; // class AsynchronousElement

  template<typename... TagValueTypes>
  class AsynchronousIterative;

  /** \brief Container for holding AsynchronousIterative values; essentially just
   * a vector of values + a vector of bools for if they are updated or not
   */
  template<typename ValueType>
  class AsynchronousValues
  {
  private:
    using Sentinel = internal::AsynchronousValuesSentinel;
    using Element = AsynchronousElement<ValueType>;

    template<typename... TagValueTypes>
    friend class AsynchronousIterative;

  public:
    /** \brief Iterator for begin/end
     */
    class Iterator {
    public:
      Iterator(const AsynchronousValues* owner, std::size_t index) noexcept
        : owner_{owner}
        , index_{index}
      {}

      // Dereference/arrow operator
      Element operator*() const noexcept
      {
        return Element{
          &owner_->values_[index_],
          owner_->updated_[index_]
        };
      }
      auto operator->() const noexcept
      {
        struct ElementWrapper {
          const Element* operator->() const noexcept
          {
            return &self_element;
          }

          Element self_element;
        };

        return ElementWrapper{*this};
      }

      // Iterator equality
      friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept
      {
        assert(lhs.owner_ == rhs.owner_);
        return lhs.index_ == rhs.index_;
      }

      // Sentinel equality
      friend bool operator==(const Iterator& it, Sentinel) noexcept
      {
        return it.index_ == it.owner_->size();
      }
      friend bool operator==(Sentinel s, const Iterator& it) noexcept
      {
        return it == s;
      }
      friend bool operator!=(const Iterator& it, Sentinel s) noexcept
      {
        return !(it == s);
      }
      friend bool operator!=(Sentinel s, const Iterator& it) noexcept
      {
        return !(it == s);
      }

      // Increment
      Iterator& operator++() noexcept
      {
        ++index_;
        return *this;
      }
      Iterator operator++(int) noexcept
      {
        const auto self = *this;
        ++*this;
        return self;
      }

      // Addition
      Iterator& operator+=(int offset) noexcept
      {
        index_ += offset;
        return *this;
      }
      friend Iterator operator+(Iterator it, int offset) noexcept
      {
        return it += offset;
      }
      friend Iterator operator+(int offset, Iterator it) noexcept
      {
        return it += offset;
      }

      // Decrement
      Iterator& operator--() noexcept
      {
        --index_;
        return *this;
      }
      Iterator operator--(int) noexcept
      {
        const auto self = *this;
        --*this;
        return self;
      }

      // Subtraction
      Iterator& operator-=(int offset) noexcept
      {
        index_ -= offset;
        return *this;
      }
      friend Iterator operator-(Iterator it, int offset) noexcept
      {
        return it -= offset;
      }
      friend Iterator operator-(int offset, Iterator it) noexcept
      {
        return it -= offset;
      }

    private:
      const AsynchronousValues* owner_;
      std::size_t index_;
    }; // class AsynchronousValues::Iterator

    const std::vector<ValueType>& values() const noexcept { return values_; }
    const std::vector<bool>& updated() const noexcept { return updated_; }

    /** \brief Returns true if all values are updated
     */
    bool all_updated() const noexcept
    {
      // Can't use algorithms due to std::vector<bool>
      for (auto val : updated_)
      {
        if (!val) { return false; }
      }
      return true;
    }

    /** \brief Returns true if any values have been updated
     */
    bool any_updated() const noexcept
    {
      for (auto val : updated_)
      {
        if (val) { return true; }
      }
      return false;
    }

    // Container-like functions
    Iterator begin() const noexcept { return Iterator{this, 0}; }
    Iterator cbegin() const noexcept { return begin(); }
    Sentinel end() const noexcept { return Sentinel{}; }
    Sentinel cend() const noexcept { return end(); }
    std::size_t size() const noexcept { return values_.size(); }
    Element operator[](const std::size_t offset) const noexcept { return *(begin() + offset); }

  private:
    explicit AsynchronousValues(const std::size_t num_tags) noexcept
      : values_(num_tags)
      , updated_(num_tags)
    {}

    std::vector<ValueType> values_;
    // TODO: std::vector<bool> is problematic, but we want the space savings
    std::vector<bool> updated_;
  }; // class AsynchronousValues

  /** \brief Method for receiving information from a specified set of tags
   * in an asynchronous fashion
   */
  template<typename... TagValueTypes>
  class AsynchronousIterative : public internal::IterativeBase<TagValueTypes...>
  {
  public:
    using ValueType = ValueOrTuple<TagValueTypes...>;
    using ValueReturnType = AsynchronousValues<ValueType>;

    /** \brief Returns values from all tags without submitting a value
     */
    auto values() noexcept
      -> std::pair<const ValueReturnType&, const std::vector<PublishTag<TagValueTypes...>>&>
    {
      auto updated_iter = values_.updated_.begin();
      auto value_iter = values_.values_.begin();
      auto tag_iter = this->tags_.begin();
      const auto mark_current_as_dead = [&]() {
        this->dead_tags_.push_back(std::move(*tag_iter));
        value_iter = values_.values_.erase(value_iter);
        updated_iter = values_.updated_.erase(updated_iter);
        tag_iter = this->tags_.erase(tag_iter);
      };
      while (tag_iter != this->tags_.cend())
      {
        const auto& tag = *tag_iter;
        if (!this->job_->tag_has_active_publisher(tag))
        {
          mark_current_as_dead();
          continue;
        }
        else if (this->job_->has_data(tag))
        {
          const auto value_opt = this->job_->get_waiter(tag).get();
          assert(value_opt);
          *updated_iter = true;
          *value_iter = *value_opt;
        }
        else
        {
          *updated_iter = false;
        }
        ++updated_iter;
        ++value_iter;
        ++tag_iter;
      }
      return {values_, this->tags_};
    }

    /** \brief Returns values from all tags while submitting a value
     */
    template<typename... ArgTypes>
    auto values(ArgTypes&&... values_to_submit) noexcept
      -> std::pair<const ValueReturnType&, const std::vector<PublishTag<TagValueTypes...>>&>
    {
      submit_values(std::forward<ArgTypes>(values_to_submit)...);
      return values();
    }

    /** \brief Submit a value
     */
    template<typename... ArgTypes>
    auto submit_values(ArgTypes&&... values_to_submit) noexcept
    {
      this->job_->publish(this->produced_tag_, std::forward<ArgTypes>(values_to_submit)...);
    }

  private:
    friend class PendingIterativeMethod<AsynchronousIterative>;

    AsynchronousIterative(
      Job& job,
      const PublishTag<TagValueTypes...>& produced_tag,
      const std::vector<PublishTag<TagValueTypes...>>& tags
    ) noexcept
      : internal::IterativeBase<TagValueTypes...>{job, produced_tag, tags}
      , values_{tags.size()}
    {}

    AsynchronousValues<ValueType> values_;
  }; // class AsynchronousIterative

  template<typename... Args>
  auto create_asynchronous_iterative(Args&&... args) noexcept
  {
    return internal::create_iterative<AsynchronousIterative>(std::forward<Args>(args)...);
  }
} // namespace skynet

// Support needed for structured bindings
template<typename T>
class std::tuple_size<skynet::AsynchronousElement<T>>
  : public std::integral_constant<std::size_t, 2> {};

template<std::size_t I, typename T>
class std::tuple_element<I, skynet::AsynchronousElement<T>>
{
  static_assert(I == 0 || I == 1);
  public:
    using type = decltype(std::declval<skynet::AsynchronousElement<T>>().template get<I>());
};

#endif // SKYNET_UPPER_ASYNCHRONOUS_ITERATIVE_HPP
