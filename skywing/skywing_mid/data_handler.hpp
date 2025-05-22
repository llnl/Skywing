#ifndef SKYWING_DATA_HANDLER_HPP
#define SKYWING_DATA_HANDLER_HPP

#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/pubsub_converter.hpp"
#include <typeinfo>
#include <iostream>
#include <tuple>
#include <unordered_set>
#include <any>
#include <numeric> 

namespace skywing {

/** @brief Helper function to print a tuple for debugging purposes. */
template<typename Tuple, std::size_t... Is>
void print_tuple_impl(std::ostream& os, const Tuple& t, std::index_sequence<Is...>) {
    ((os << (Is == 0 ? "" : ", ") << std::get<Is>(t)), ...);
}

/** @brief Overloads the << operator to print a tuple. */
template<typename... Args>
std::ostream& operator<<(std::ostream& os, const std::tuple<Args...>& t) {
    os << "(";
    print_tuple_impl(os, t, std::index_sequence_for<Args...>{});
    return os << ")";
}


/**
 * @brief Template class to handle a collection of Data objects.
 * 
 * @tparam DataType The type of data being handled.
 */
template <typename DataType>
class DataHandler  {
public:
    /**
     * @brief Constructor to initialize the DataHandler with tags and neighbor values.
     * 
     * @param tags A vector of tags.
     * @param neighbor_values A map of neighbor values associated with tags.
     */    DataHandler(const std::vector<std::string>& tags, const std::unordered_map<std::string, DataType>& neighbor_values) : tags_(std::move(tags)) {
        for (const auto& tag : tags_) {
            if (neighbor_values.contains(tag)) {
                neighbor_data_.emplace_back(tag, neighbor_values.at(tag));
            }
        }
    }
    /**
     * @brief Default constructor for the DataHandler.
     */    DataHandler() {}
       //AF Note: Something to be mindful here. Until we get actual data for a tag the DataHandler object data_handler_ is not keeping track of it. 
       // But the tags_ is the number of tags that was passed to the DataHandler. Might need to update this in the future. 

    /**
     * @brief Updates the data associated with each tag using the provided map of new values.
     * 
     * @param new_values A map of new values to update.
     */    
     void update(const std::unordered_map<std::string, DataType>& new_values) {
        for (const auto& [tag, data] : new_values) {
            auto it = neighbor_data_.end(); 
             for (auto iter = neighbor_data_.begin(); iter != neighbor_data_.end(); ++iter) {
                if (iter->first == tag) {
                    it = iter;
                    break;
                }
            }

            if (it != neighbor_data_.end()) {
                // Update existing data
                it->second = data;
            } else {
                // Add new data if tag is not found
                neighbor_data_.emplace_back(tag, data);
                auto it = std::find(tags_.begin(), tags_.end(), tag);

                if (it == tags_.end()) {
                    tags_.push_back(tag); 
                }
            }
        }
    }

    /**
     * @brief Returns a new DataHandler with transformed data types using the provided transformation function.
     * 
     * @tparam SubDataType The type of the transformed data.
     * @param sub_transformer A function to transform the data type.
     * @return A DataHandler with transformed data.
     */    template <typename SubDataType>
    DataHandler<SubDataType> get_sub_handler(std::function<SubDataType(const DataType&)> sub_transformer) const {
        std::vector<std::string> tags;
        std::unordered_map<std::string, SubDataType> neighbor_values;

        for (const auto& data : neighbor_data_) {
            tags.push_back(data.first);
            neighbor_values[data.first] = sub_transformer(data.second);
           
        }
        return DataHandler<SubDataType>(tags, neighbor_values);
    }

    /**
     * @brief Returns a sub-handler for the k-th index of the tuple data.
     * 
     * @tparam SubDataType The type of the sub-handler data.
     * @tparam k The index of the tuple to extract.
     * @return A DataHandler for the k-th index.
     */    
    template <typename SubDataType, int k>
    DataHandler<SubDataType> get_kth_index_handler() const {
        return get_sub_handler<SubDataType>([](const DataType& v) { return std::get<k>(v); });
    }

    /**
    * @brief Accumulates neighbor data using a provided function and binary operation.
    * 
    * @tparam R The result type of the accumulation.
    * @param f A function to apply to each data element.
    * @param binary_op A binary operation to accumulate results.
    * @return The accumulated result.
    * @throws std::runtime_error if no neighbor data is available.
    */
    template <typename R>
    R f_accumulate(std::function<R(const DataType&)> f, std::function<R(R, R)> binary_op) const {
        if (neighbor_data_.empty()) {
            throw std::runtime_error("No neighbor data available for accumulation");
        }

        auto it = neighbor_data_.cbegin();
        R accumulated_value = f(it->second);
        ++it;

        for (; it != neighbor_data_.cend(); ++it) {
            accumulated_value = binary_op(std::move(accumulated_value), f(it->second));
        }

        return accumulated_value;
    }

    /**
     * @brief Computes the sum of the updated neighbor data.
     * 
     * @tparam R The result type of the sum.
     * @return The sum of the data.
     */
    template <typename R = DataType>
    R sum() const {
    return std::accumulate(neighbor_data_.cbegin(), neighbor_data_.cend(), R{},
        [](R total, const std::pair<std::string, DataType>& data) {
            return total + data.second;
        });
    }

    /**
     * @brief Computes the average of the updated neighbor data.
     * 
     * @tparam R The result type of the average.
     * @return The average of the data.
     */    template <typename R = DataType>
    R average() const {
        R num = sum<R>();
        R denom = neighbor_data_.size();
        
        return denom != 0 ? num / denom : 0;
    }

    /**
    * @brief Finds the minimum or maximum value from the updated neighbor data.
    *
    * @tparam R The return type, defaulting to the type of the data stored.
    * @param find_min A boolean flag indicating whether to find the minimum (true)
    *                 or maximum (false) value. Defaults to true.
    * @return The minimum or maximum value of type R from the neighbor data.
    * @throws std::runtime_error if the neighbor data is empty.
    *
    * @example
    * DataHandler<int> data_handler(tags, neighbor_values);
    * int min_value = data_handler.recvd_extreme(); // Finds the minimum value
    * int max_value = data_handler.recvd_extreme(false); // Finds the maximum value
    */    
    template <typename R = DataType>
    R recvd_extreme(bool find_min = true) const {
        if (neighbor_data_.empty()) {
            throw std::runtime_error("No neighbor data available to find extreme value");
        }

        R extreme_data = neighbor_data_.front().second; 

        for (const auto& data : neighbor_data_) {
            if (find_min) {
                if (data.second < extreme_data) {
                    extreme_data = data.second; 
                }
            } else {
                if (data.second > extreme_data) {
                    extreme_data = data.second; 
                }
            }
        }

        return extreme_data;
    }

    /**
    * @brief Returns a set of pointers to the data values in `neighbor_data_`.
    *
    * @tparam R The type of data values, defaulting to `DataType`.
    * @return std::unordered_set<const R*> Set of unique pointers to data values.
    */    
    template <typename R = DataType>
    std::unordered_set<const R*> recvd_data_values() const {
        std::unordered_set<const R*> received_data;
        received_data.reserve(neighbor_data_.size());
        for (const auto& data : neighbor_data_) {
            received_data.insert(&data.second);
        }
        return received_data;
    }

    /**
    * @brief Retrieves the tags of the updated neighbor data.
    *
    * This function returns a vector containing the tags associated with the neighbor data.
    * It optimizes memory allocation by reserving space in the vector based on the size of
    * `neighbor_data_`, reducing the need for reallocations during insertion.
    *
    * @return std::vector<std::string> A vector of tags from the neighbor data.    
    **/
    std::vector<std::string> recvd_data_tags() const {
        std::vector<std::string> received_tags;
        received_tags.reserve(neighbor_data_.size());
        for (const auto& data : neighbor_data_) 
            received_tags.push_back(data.first);
        return received_tags;
    }

    /**
    * @brief Retrieves the data associated with a specific tag.
    *
    * This function searches for the specified tag within the neighbor data and returns
    * the corresponding data value. It throws a runtime error if the tag is not found.
    *
    * @param tag The tag for which the data is to be retrieved.
    * @return DataType The data associated with the specified tag.
    * @throws std::runtime_error If the tag is not found in the neighbor data
    **/
    DataType get_data(const std::string& tag) const {
        for (const auto& data : neighbor_data_) {
            if (data.first == tag) {
                return data.second;
            }
        }
        throw std::runtime_error("Tag not found");
    }

    /**
    * @brief Returns the number of neighbor tags.
    *
    * This function returns the number of tags that were initially provided to the
    * `DataHandler`. It reflects the total count of tags, not necessarily the count
    * of updated neighbor data.
    *
    * @return size_t The number of neighbor tags.
    *
    **/  
    size_t num_neighbors() const {
        return tags_.size();
        for(auto tag :tags_) 
            std::cout<<tag<<std::endl;
    }

    private:
    std::vector<std::string> tags_ = {}; ///< Vector of tags. Note that the length of tags_ does not have to equal length of neighbor_data_ 
    std::vector<std::pair<std::string, DataType>> neighbor_data_ = {}; ///< Vector of neighbor data
};

/**
 * @brief A utility class that wraps a DataHandler instance, allowing for type-erased storage and retrieval.
 */
class DataHandlerBaseWrapper {
public:
    /**
     * @brief Constructs a wrapper around a DataHandler instance.
     * 
     * @tparam DataType The type of data handled by the DataHandler.
     * @param handler A unique pointer to a DataHandler instance.
     */
    template <typename DataType>
    DataHandlerBaseWrapper(std::unique_ptr<DataHandler<DataType>> handler)
        : handler_(std::make_shared<std::unique_ptr<DataHandler<DataType>>>(std::move(handler))) {}

     /**
     * @brief Retrieves the stored DataHandler instance if the type matches.
     * 
     * @tparam DataType The type of data handled by the DataHandler.
     * @return A pointer to the DataHandler instance, or nullptr if the type does not match.
     */
    template <typename DataType>
    DataHandler<DataType>* get() {
        if (auto ptr = std::any_cast<std::shared_ptr<std::unique_ptr<DataHandler<DataType>>>>(&handler_)) {
            return ptr->get()->get();
        }
        return nullptr; // Return nullptr if the cast fails
    }

private:
    std::any handler_; ///< Use std::any to store the DataHandler.
};


} // namespace skywing

#endif // SKYWING_DATA_HANDLER_HPP
