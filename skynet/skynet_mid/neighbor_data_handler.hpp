#ifndef SKYNET_NEIGHBOR_DATA_HANDLER_HPP
#define SKYNET_NEIGHBOR_DATA_HANDLER_HPP

#include "skynet_mid/pubsub_converter.hpp"

namespace skynet
{
  template<typename TagType, typename T>
  using tag_map = std::unordered_map
    <TagType, T, skynet::internal::hash<TagType>>;

  template<typename BaseDataType, typename DataType>
  class NeighborDataHandler
  {
    using TagValueType = typename PubSubConverter<BaseDataType>::pubsub_type;
  public:
    using TagType = UnwrapAndApply_t<TagValueType, PublishTag>;

    NeighborDataHandler(std::function<DataType(const BaseDataType&)> transformer,
                        const std::vector<TagType>& tags,
                        const tag_map<TagType, BaseDataType>& neighbor_values,
                        const std::vector<const TagType*>& updated_tags)
      : transformer_(transformer), tags_(tags),
        neighbor_values_(neighbor_values), updated_tags_(updated_tags)
    {}

    template<typename SubDataType>
    NeighborDataHandler<BaseDataType, SubDataType>
    get_sub_handler(std::function<SubDataType(DataType&)> sub_transformer) const
    {
      return NeighborDataHandler<BaseDataType, SubDataType>
        ([&](BaseDataType& v){return sub_transformer(transformer_(v));},
         tags_, neighbor_values_, updated_tags_);
    }

    /******************************
     * Summation functions
     *****************************/
    
    template<typename R = DataType>
    R sum() const { return weighted_f_accumulate_<R>(transformer_, std::plus<R>()); }

    template<typename S = DataType, typename R = DataType>
    R weighted_sum(tag_map<TagType, S> coeffs) const
    {
      return weighted_f_accumulate_<DataType>
        (transformer_, [&](const TagType& t){return coeffs[t];}, std::plus<R>());
    }

    template<typename R>
    R f_sum(std::function<R(const DataType&)> f) const
    {
      return weighted_f_accumulate_<R>([&](const BaseDataType& v){return f(transformer_(v));},
                                      std::plus<R>());
    }

    /******************************
     * Averaging functions
     *****************************/

    template<typename R = DataType>
    R average() const { return sum() / num_neighbors(); }

    template<typename S>
    DataType weighted_average(tag_map<TagType, S> coeffs) const
    {
      DataType num = weighted_sum(std::move(coeffs));
      DataType denom = weighted_f_accumulate_<S>([&](const TagType& t){return coeffs[t];},
                                                 std::plus<S>());
      return num / denom;
    }

    /******************************
     * Other useful functions
     *****************************/

    std::size_t num_neighbors() const { return tags_.size(); }
    
    template<typename R>
    R f_accumulate(std::function<R(const DataType&)> f, std::function<R(R, R)> binary_op) const
    {
      return f_accumulate_<R>([&](const BaseDataType& v){return f(transformer_(v));},
                             std::move(binary_op));
    }

    DataType get_data_unsafe(const TagType& tag) const
    {
      return transformer_(neighbor_values_.at(tag));
    }

    const std::vector<const TagType*>& get_updated_tags() const { return updated_tags_; }
    
  private:
    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const BaseDataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op,
                             R* shift) const
    {
      auto tag_iter = tags_.cbegin();
      R val = binary_op(*shift, coef(*tag_iter) * f(neighbor_values_[*tag_iter]));
      for (; tag_iter != tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter) * f(neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const BaseDataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op) const
    {
      auto tag_iter = tags_.cbegin();
      R val = coef(*tag_iter) * f(neighbor_values_[*tag_iter]);
      for (; tag_iter != tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter) * f(neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R f_accumulate_(std::function<R(const BaseDataType&)> f,
                    std::function<R(R, R)> binary_op,
                    R* shift) const
    {
      auto tag_iter = tags_.cbegin();
      R val = binary_op(*shift, f(neighbor_values_[*tag_iter]));
      for (; tag_iter != tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), f(neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R f_accumulate_(std::function<R(const BaseDataType&)> f,
                    std::function<R(R, R)> binary_op) const
    {
      auto tag_iter = tags_.cbegin();
      R val = f(neighbor_values_[*tag_iter]);
      for (; tag_iter != tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), f(neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R weighted_f_accumulate_(std::function<R(const TagType&)> coef,
                             std::function<R(R, R)> binary_op) const
    {
      auto tag_iter = tags_.cbegin();
      R val = coef(*tag_iter);
      for (; tag_iter != tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter));
      return val;
    }

    std::function<DataType(const BaseDataType&)> transformer_;
    const std::vector<TagType>& tags_;
    const tag_map<TagType, BaseDataType>& neighbor_values_;
    const std::vector<const TagType*>& updated_tags_;
  }; // class NeighborDataHandler

} // namespace skynet

#endif // SKYNET_NEIGHBOR_DATA_HANDLER_HPP
