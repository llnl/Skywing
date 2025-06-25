#ifndef SKYWING_MID_INTERNAL_ITERATIVE_BASE_HPP
#define SKYWING_MID_INTERNAL_ITERATIVE_BASE_HPP

#include <typeindex>  // Add this line to include the necessary header
#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_core/tag.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/data_handler.hpp"

#include "skywing_mid/pubsub_converter.hpp"

namespace skywing
{

/** @brief Base class for iterative methods.
 *
 * @param ResiliencePolicy Determines how this IterativeMethod
 * should respond to problems such as dead neighbors.
 *
 * @tparam DataType The type of data that this iterative method will
 * send to its neighbors. Not necessarily in pubsub type; that
 * conversion will be handled by IterativeMethod.
 *
 */
template <typename ResiliencePolicy, typename DataType>
class IterativeMethod
{
public:
    using ThisT = IterativeMethod<ResiliencePolicy, DataType>;
    using TagValueType =
        typename PubSubConverter<DataType>::pubsub_type; // std::tuple<stuff...>
    using TagType = UnwrapAndApply_t<TagValueType, Tag>; // Tag<stuff...>;
    using DataT = DataType;
    using ValueType = DataType;

    /** @param job The job running this iterative method.
     *  @param produced_tag The tag this agent will publish during iteration.
     *  @param tags The set of tags consumed during iteration, possibly
     * including @p produced_tag
     *  @param resilience_policy The ResiliencePolicy object used in iteration.
     */
    IterativeMethod(Job& job,
                    const TagType& produced_tag,
                    const std::vector<TagType>& tags,
                    ResiliencePolicy resilience_policy) noexcept
        : job_{&job},
          produced_tag_{produced_tag},
          tags_{tags},
          resilience_policy_(resilience_policy)
    {
        for (auto tag_iter = tags_.begin(); tag_iter != tags_.end();) {
            if (!job_->tag_has_active_publisher(*tag_iter)) {
                dead_tags_.push_back(std::move(*tag_iter));
                tag_iter = tags_.erase(tag_iter);
            } else {
                ++tag_iter;
            }
        }
    }

    const TagType& my_tag() const { return produced_tag_; }

    /** @brief When a neighbor dies, record and react appropriately.
     *
     * The exact behavior here depends on the ResiliencePolicy.
     */
    template <typename TagIter>
    TagIter handle_dead_neighbor(const TagIter& tag_iter) noexcept
    {
        dead_tags_.push_back(std::move(*tag_iter));
        resilience_policy_.handle_dead_neighbor(*this, tag_iter);
        return tags_.erase(tag_iter);
    }

    /** @brief Rebuilds connections for dead tags
     */
    Waiter<void> rebuild_dead_tags() noexcept
    {
        auto to_ret = job_->rebuild_tags(dead_tags_);
        std::move(
            dead_tags_.begin(), dead_tags_.end(), std::back_inserter(tags_));
        dead_tags_.clear();
        return to_ret;
    }

    /** @brief Drops tracking for dead tags
     */
    void drop_dead_tags() noexcept
    {
        // TODO: Actually unsubscribe when that's a thing that can happen
        dead_tags_.clear();
    }

    /** @brief Gather any data that has been published by neighbors.
     *
     * If any neighbors have died, this is where that will be detected
     * and handled. This function does not worry about whether or not
     * all neighbors, or even any neighbors, have ready data. So in the
     * case of a SynchronousIterative method, for example, it is the
     * responsibility of that derived class to ensure it is time to call
     * this function before doing so.
     *
     * @returns true if any new data is received, false otherwise.
     */
    bool gather_values()
    {
        auto tag_iter = tags_.begin();
        size_t num_updated = 0;
        // Go through tags, detect any that have died and detect any that
        // have available data to read.
        while (tag_iter != tags_.cend()) {
            const auto& tag = *tag_iter;
            if (!job_->tag_has_active_publisher(tag)) {
                tag_iter = handle_dead_neighbor(tag_iter);
                continue;
            }
            if (job_->has_data(tag))
                num_updated++;
            ++tag_iter;
        }

        if (num_updated == 0)
            return false;

        // For the neighbors that are alive and have ready data, read in
        // the data. Record which tags have been updated.
        updated_tags_.clear();
        updated_tags_.reserve(num_updated);
        for (const auto& tag : tags_) {
            if (job_->has_data(tag)) {
                const auto value_opt = job_->get_waiter(tag).get();
                // an assert...doesn't seem like the right way to handle this
                assert(value_opt);
                // convert the pubsub_type back into the required DataType
                neighbor_values_[tag] =
                    PubSubConverter<DataType>::deconvert(*value_opt);
                updated_tags_.push_back(&tag);
            }
        }

        return true;
    }

    /** @brief Publish this agent's values for its neighbors.
     */
    auto submit_values(DataType value_to_submit) noexcept
    {
        TagValueType to_publish =
            PubSubConverter<DataType>::convert(std::move(value_to_submit));
        job_->publish_tuple(produced_tag_, to_publish);
    }

    /** @brief Returns all active tags
     */
    const std::vector<TagType>& tags() const noexcept { return tags_; }

    /** @brief Returns all dead tags
     */
    const std::vector<TagType>& dead_tags() const noexcept
    {
        return dead_tags_;
    }

    Job& get_job() const noexcept { return *job_; }


    template <typename InnerDataType>
    void addHandler(std::type_index type, std::unique_ptr<DataHandler<InnerDataType>> handler) {
        policy_data_handlers_.emplace(type, DataHandlerBaseWrapper(std::move(handler)));
    }

protected:
    template <typename Policy, typename IterMethod>
    void initialize_policy_data_handler() {
        using ret_t = typename Policy::ValueType;
        constexpr std::size_t ind = IndexInPublishers<Policy, IterMethod>::index;

        // Create a lambda to extract the specific element from ValueType
        auto extractor = [](const ValueType& v) { return std::get<ind>(v); };

        // Apply the extractor to transform the neighbor values
        std::unordered_map<std::string, ret_t> transformed_values;
        for (const auto& [tag, data] : neighbor_values_) {
            transformed_values[tag.id()] = extractor(data);
        }

        // Apply create strings for tags for the data handler
        std::vector<std::string> transformed_tags;
        for (const auto& tag : tags_) {
            transformed_tags.push_back(tag.id());
        }

        // Store the DataHandler in the map
        addHandler(std::type_index(typeid(Policy)),(std::make_unique<DataHandler<ret_t>>(transformed_tags, transformed_values)) ); 
        // policy_data_handlers_[std::type_index(typeid(Policy))] =  std::move(std::make_unique<DataHandler<ret_t>>(transformed_tags, transformed_values));
    }
    /** @brief Get the NeighborDataHandler for the Policy data.
     *
     * If the Policy does not define a ValueType, then attempting to
     * instantiate this function will induce a compile-time error.
     */
    template <typename Policy, typename IterMethod>
    DataHandler<typename Policy::ValueType> get_policy_data_handler() {
        using ret_t = typename Policy::ValueType;

        // Check if the DataHandler already exists
        auto it = policy_data_handlers_.find(std::type_index(typeid(Policy)));
        if (it == policy_data_handlers_.end()) {
            // Initialize if it doesn't exist
            initialize_policy_data_handler<Policy, IterMethod>();
            it = policy_data_handlers_.find(std::type_index(typeid(Policy)));
        }

        // Transform and update the DataHandler
        try {
            update_policy_data_handler<Policy, IterMethod>(it);
        } catch (const std::exception& e) {
            std::cerr << "Error updating DataHandler: " << e.what() << std::endl;
            throw; // or handle the error appropriately
        }

        // Return the existing DataHandler
        return *(it->second.template get<ret_t>());
    }

    template <typename Policy, typename IterMethod>
    void update_policy_data_handler(typename std::unordered_map<std::type_index, DataHandlerBaseWrapper>::iterator& it) {
        using ret_t = typename Policy::ValueType;
        auto transform_func = [](const ValueType& v) {
            constexpr std::size_t ind = IndexInPublishers<Policy, IterMethod>::index;
            return std::get<ind>(v);
        };

    std::unordered_map<std::string, ret_t> transformed_values;
       for (const auto& [tag, data] : neighbor_values_) {
            try {
                // Cast std::any to the expected type
                ret_t value = std::any_cast<ret_t>(transform_func(data)); 
                transformed_values[tag.id()] = value;
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Error casting std::any to expected type: " << e.what() << std::endl;
            }
        }

        if (it != policy_data_handlers_.end()) {
            DataHandler<ret_t>* handler = it->second.template get<ret_t>();
            if (handler) {
                handler->update(transformed_values);
            } else {
                throw std::runtime_error("Failed to cast to DataHandler");
            }
        }
    }



    /** @brief Ask this Policy to process an update.
     *
     * This function is to be called only if Policy defines ValueType.
     *
     * @param policy the Policy object to update.
     * @param std::true_type Flag to create overload resolution.
     */
    template <typename Policy, typename IterMethod>
    void process_policy_update_(Policy& policy, std::true_type)
    {
        policy.process_update(get_policy_data_handler<Policy, IterMethod>(),
                              *this);
    }
    /** @brief Ask this Policy to process an update.
     *
     * This function is to be called only if Policy does NOT define ValueType.
     *
     * @param policy the Policy object to update.
     * @param std::false_type Flag to create overload resolution.
     */
    template <typename Policy, typename IterMethod>
    void process_policy_update_(Policy&, std::false_type)
    {}

    /** @brief Ask a policy for its initial value, if it produces any.
     *
     *  Wraps that value in a std::tuple of length 1. If the Policy does
     *  not publish anything, returns a std::tuple<>. The purpose of
     *  this is to be included in a call to std::tuple_cat.
     *
     * For example, a derived IterMethod type might call
     * \code{.cpp}
     * std::tuple_cat
     *   (this->template get_init_tuple_<Processor, ThisT>(processor_),
     *    this->template get_init_tuple_<IterationPolicy, ThisT>(iteration_policy_),
     *    this->template get_init_tuple_<ResiliencePolicy,
     * ThisT>(this->resilience_policy_)); \code
     */
    template <typename Policy, typename IterMethod>
    auto get_init_tuple_(Policy& policy_obj)
    {
        using PubTup = typename IfHasValueType<Policy>::tuple_of_value_type;
        if constexpr (std::tuple_size_v<PubTup> == 0)
            return std::tuple<>();
        else
            return PubTup(policy_obj.get_init_publish_values());
    }

    /** @brief Ask a policy for the value it wants to publish, if it
     * produces any.
     *
     *  Wraps that value in a std::tuple of length 1. If the Policy does
     *  not publish anything, returns a std::tuple<>. The purpose of
     *  this is to be included in a call to std::tuple_cat.
     *
     * For example, a derived IterMethod type might call
     * \code{.cpp}
     * std::tuple_cat
     *   (this->template get_pub_tuple_<Processor, ThisT>(processor_,
     * publish_values_), this->template get_pub_tuple_<IterationPolicy,
     * ThisT>(iteration_policy_, publish_values_), this->template
     * get_pub_tuple_<ResiliencePolicy, ThisT>(this->resilience_policy_,
     * publish_values_)); \code
     */
    template <typename Policy, typename IterMethod>
    auto get_pub_tuple_(Policy& policy_obj,
                        const typename IterMethod::ValueType& vals)
    {
        using PubTup = typename IfHasValueType<Policy>::tuple_of_value_type;
        if constexpr (std::tuple_size_v<PubTup> == 0)
            return std::tuple<>();
        else {
            constexpr std::size_t ind =
                IndexInPublishers<Policy, IterMethod>::index;
            return PubTup(
                policy_obj.prepare_for_publication(std::get<ind>(vals)));
        }
    }

    Job* job_;
    TagType produced_tag_;
    std::vector<TagType> tags_;
    std::vector<TagType> dead_tags_;
    ResiliencePolicy resilience_policy_;

private:

    tag_map<TagType, DataType> neighbor_values_;
    std::vector<const TagType*> updated_tags_;

    template <typename Callable>
    friend class DataHandler;

    // Member variable for DataHandler
    std::unordered_map<std::type_index, DataHandlerBaseWrapper> policy_data_handlers_;

}; // class IterativeBase

} // namespace skywing

#endif // SKYWING_MID_INTERNAL_ITERATIVE_BASE_HPP
