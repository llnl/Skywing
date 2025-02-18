#ifndef SKYWING_MID_IDEMPOTENT_PROCESSOR_HPP
#define SKYWING_MID_IDEMPOTENT_PROCESSOR_HPP

#include <algorithm>
#include <functional>

#include "skywing_mid/data_handler.hpp"
namespace skywing
{

/**
 * An idempotent operation is one that can be applied multiple times
 * without changing the result. This processor can be used to
 * collectively compute operations in which the local update is
 * idempotent.
 *
 * Some examples include maximum, minimum, unions, logical-AND,
 * logical-OR, and projections. This does not include operations such
 * as summation and averaging.
 *
 * The processor has a versioning feature, so that a user can
 * call the set_value() function to alter the value mid-iteration.
 * This allows for older-version values to be discarded to prevent
 * stale values from continuing to poison the results if a newer
 * result is present.
 *
 * To avoid any concerns of overflowing the counter (incremented
 * with each call to set_value()), the implementation uses two 64-bit integer
 * counters to provide the behavior of using a 128-bit integer counter
 * (without explicitly using a 128-bit integer for portability reasons).
 *
 * @tparam T The data type.
 * @tparam BinaryOperation The idempotent operation.
 */
template <typename T, typename BinaryOperation>
class IdempotentProcessor
{
public:
    using ValueType = std::tuple<T, std::uint64_t, std::uint64_t>;

    IdempotentProcessor(T starting_value)
        : curr_value_({starting_value, 0, 0}), my_value_(curr_value_){}

    IdempotentProcessor(BinaryOperation op, T starting_value)
        : op_(std::move(op)),
          curr_value_({starting_value, 0, 0}),
          my_value_(curr_value_)
    {}

    ValueType get_init_publish_values() { return curr_value_; }

    template <typename ValueType,typename IterMethod>
    void process_update(const DataHandler<ValueType>& data_handler,
                        const IterMethod&)
    {

        // Function to compare two values taking into account versions,
        // where the higher version dominates the binary operator
        auto version_op = [this](ValueType v1, ValueType v2) -> ValueType {
            return this->version_aware_binary_op_(v1, v2);
        };
            curr_value_ =
            version_op(curr_value_,
                       data_handler.template f_accumulate<ValueType>(
                           [](const ValueType& v) { return v; }, version_op));
      
        // Based on the version and wrap counters, determine my_value_,
        // which always has the most up to date version, as it is my
        // active contribution and set the correct version and wrap counter
        // values

        // Case where version wrap counter are equal, so set my_value_'s
        // version to be the most recent (highest of version counter values)
        if (getVersionWrapCount_(my_value_)
            == getVersionWrapCount_(curr_value_))
            my_value_ = std::make_tuple(
                getT_(my_value_),
                std::max(getVersion_(my_value_), getVersion_(curr_value_)),
                getVersionWrapCount_(my_value_));
        else {
            // Case where my_value_ version counter has wrapped (and curr_value_
            // has not), so use the version of my_value_ as it is the most
            // recent
            if (getVersionWrapCount_(my_value_)
                > getVersionWrapCount_(curr_value_))
                my_value_ = std::make_tuple(getT_(my_value_),
                                            getVersion_(my_value_),
                                            getVersionWrapCount_(my_value_));
            else // Case where curr_value_ is the most recent
                my_value_ = std::make_tuple(getT_(my_value_),
                                            getVersion_(curr_value_),
                                            getVersionWrapCount_(curr_value_));
        }

        // Determine the current value of the operator, based
        // on the version and wrap counters
        curr_value_ = version_op(curr_value_, my_value_);

    }

    ValueType prepare_for_publication(ValueType) { return curr_value_; }

    T get_value() const { return getT_(curr_value_); }

    /** @brief Return the version counter value.
     *
     *  This counter tracks the version number (which is allowed to overflow).
     *  In the case of overflow, this version counter value will wrap to 0 and
     *  the version wrap counter will be incremented.
     */
    std::uint64_t get_version() const { return getVersion_(curr_value_); }

    /** @brief Return the version wrap counter value.
     *
     *  This counter tracks the number of the times the version
     *  number has overflowed / wrapped.
     */
    std::uint64_t get_version_wrap_count() const
    {
        return getVersionWrapCount_(curr_value_);
    }

    /** @brief Update the value mid-iteration.
     *
     *  This allows for a value to be updated mid-iterations,
     *  so older-version values will be discarded to prevent
     *  stale values from continuing to poison the results if a newer
     *  result is present.
     *
     *  @param val Updated value to overwrite stale value
     */
    void set_value(T val)
    {
        // Handle case if we have overflow of the version counter,
        // so will increment both the version and wrap count
        if (getVersion_(my_value_) + 1 < getVersion_(my_value_))
            my_value_ = std::make_tuple(val,
                                        getVersion_(my_value_) + 1,
                                        getVersionWrapCount_(my_value_) + 1);
        else // Increment the version
            my_value_ = std::make_tuple(val,
                                        getVersion_(my_value_) + 1,
                                        getVersionWrapCount_(my_value_));
        curr_value_ = version_aware_binary_op_(curr_value_, my_value_);
    }

private:
    /** @brief Compare two values considering versions, so the newer-versioned
     * value is returned regardless of the binary operator result.
     *
     *  This allows for old-version values to be discarded to prevent
     *  stale values from continuing to poison the results if a newer
     *  result is present.
     */
    ValueType version_aware_binary_op_(ValueType v1, ValueType v2)
    {
        // Get the version counter values for each value
        const std::uint64_t& v1_ver = getVersion_(v1);
        const std::uint64_t& v2_ver = getVersion_(v2);

        // Get the wrap counter values if for each values - used to account for
        // potential overflow in version counter
        const std::uint64_t& wc1_ver = getVersionWrapCount_(v1);
        const std::uint64_t& wc2_ver = getVersionWrapCount_(v2);

        // If wrap counter values are different, return the value
        // associated with the larger wrap counter value
        if (wc1_ver != wc2_ver)
            return (wc1_ver > wc2_ver) ? v1 : v2;

        // Now handle cases where wrap counter values are the same.
        // If versions are different, return the value
        // with the higher version counter value (most recent version)
        if (v1_ver != v2_ver)
            return (v1_ver > v2_ver) ? v1 : v2;

        // Otherwise in the case where v1_ver == v2_ver,
        // return the value of the binary operator
        return std::make_tuple(op_(getT_(v1), getT_(v2)), v1_ver, wc1_ver);
    }

    T getT_(const ValueType& v) const { return std::get<0>(v); }

    /** @brief Get the version counter value.
     *
     *  This counter tracks the version number (which is allowed to overflow).
     *  In the case of overflow, this version counter value will wrap to 0 and
     *  the version wrap counter will be incremented.
     */
    std::uint64_t getVersion_(const ValueType& v) const
    {
        return std::get<1>(v);
    }

    /** @brief Get the version wrap counter value.
     *
     *  This counter tracks the number of the times the version
     *  number has overflowed / wrapped.
     */
    std::uint64_t getVersionWrapCount_(const ValueType& v) const
    {
        return std::get<2>(v);
    }

    ValueType curr_value_;
    ValueType my_value_;
    BinaryOperation op_;
};

template <typename T, typename Selector>
struct SelectionOp
{
    Selector selector_;
    T operator()(const T& t1, const T& t2)
    {
        return selector_(t1, t2) ? t1 : t2;
    }
}; // struct SelectionOp

template <typename T>
using MaxProcessor = IdempotentProcessor<T, SelectionOp<T, std::greater<T>>>;

template <typename T>
using MinProcessor = IdempotentProcessor<T, SelectionOp<T, std::less<T>>>;

template <typename T>
using LogicalAndProcessor = IdempotentProcessor<T, std::logical_and<T>>;

template <typename T>
using LogicalOrProcessor = IdempotentProcessor<T, std::logical_or<T>>;

} // namespace skywing
#endif // SKYWING_MID_IDEMPOTENT_PROCESSOR_HPP
