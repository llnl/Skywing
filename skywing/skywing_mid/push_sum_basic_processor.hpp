#ifndef SKYWING_UPPER_PUSH_SUM_BASIC_HPP
#define SKYWING_UPPER_PUSH_SUM_BASIC_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/data_handler.hpp"
#include <random>
#include <iostream>
#include <unordered_map>
#include <tuple>

namespace skywing {

template <typename data_t = double, typename weight_t = double>
class PushSumBasicProcessor {
public:
    using ValueType = std::tuple<std::unordered_map<std::string, data_t>, std::unordered_map<std::string, weight_t>>;

    PushSumBasicProcessor(data_t starting_value, std::string id)
        : current_sum_value_(starting_value), id_(id) {
        send_value_[id_] = current_sum_value_;
        send_weight_[id_] = current_weight_value_;
    }

    ValueType get_init_publish_values() {
        return {send_value_, send_weight_};
    }

    template <typename IterMethod>
    void process_update(const DataHandler<ValueType>& data_handler, const IterMethod& iter_method) {
        for (const auto& pTag : data_handler.recvd_data_tags()) {
            if (pTag == iter_method.my_tag()) continue;
            const ValueType& nbr_data = data_handler.get_data(pTag);
            receive_sum_value_[pTag] =  get<0>(nbr_data).at(pTag);
            receive_weight_value_[pTag] =  get<1>(nbr_data).at(pTag);
        }

        current_sum_value_ = std::accumulate(receive_sum_value_.begin(), receive_sum_value_.end(), 0.5 * current_sum_value_,
            [](double sum, const std::pair<const std::string, double>& item) {
                return sum + item.second; // Accumulate the second element of the pair
            });
        current_weight_value_ = std::accumulate(receive_weight_value_.begin(), receive_weight_value_.end(), 0.5 * current_weight_value_,
            [](double sum, const std::pair<const std::string, double>& item) {
                return sum + item.second; // Accumulate the second element of the pair
            });

        send_value_[id_] = current_sum_value_;
        send_weight_[id_] = current_weight_value_;

        ++iteration_count_;
    }

    ValueType prepare_for_publication(ValueType) {
        update_send_values();
        return {send_value_, send_weight_};
    }

    data_t get_value() const {
        return current_sum_value_ / current_weight_value_;
    }

private:

    void update_send_values() {
        if (send_value_.empty()) {
            return;
        } else {
            // std::mt19937 gen{std::random_device{}()};
            // auto iter = send_value_.begin();
            // std::uniform_int_distribution<> dis(0, std::distance(iter, send_value_.end()) - 1);
            // std::advance(iter, dis(gen));
            // send_value_[iter->first] = 0.5 * my_sum_value_;
            // send_weight_[iter->first] = 0.5 * my_weight_value_;
            for (auto& [key, value] : send_value_) {
                send_value_[key] = 0.5 * current_sum_value_;   // Update send_value_ for each key
                send_weight_[key] = 0.5 * current_weight_value_; // Update send_weight_ for each key
            }
        }
    }

    int iteration_count_ = 0;
    data_t current_sum_value_;
    std::string id_;
    weight_t current_weight_value_ = 1;
    std::unordered_map<std::string, data_t> send_value_;
    std::unordered_map<std::string, weight_t> send_weight_;
    std::unordered_map<std::string, data_t> receive_sum_value_;
    std::unordered_map<std::string, weight_t> receive_weight_value_;
};

} // namespace skywing

#endif