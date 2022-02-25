#ifndef SKYNET_NEIGHBOR_DATA_HANDLER_HPP
#define SKYNET_NEIGHBOR_DATA_HANDLER_HPP

namespace skynet
{
  template<typename TagType, typename T>
  using tag_map = std::unordered_map
    <TagType, T, skynet::internal::hash<TagType>>;

  template<typename IterMethod, typename ret_type>
  class NeighborDataHandler
  {
    using TagType = typename IterMethod::TagType;
    using DataType = typename IterMethod::DataT;
  public:

    NeighborDataHandler(std::function<ret_type(DataType&)> transformer,
                        IterMethod& iter_method)
      : transformer_(transformer), iter_method_(iter_method)
    {}

    template<typename sub_type>
    NeighborDataHandler<IterMethod, sub_type>
    get_sub_handler(std::function<sub_type(ret_type&)> sub_transformer)
    {
      return NeighborDataHandler<IterMethod, sub_type>
        ([&](DataType& v){return sub_transformer(transformer_(v));},
         iter_method_);
    }

    /******************************
     * Summation functions
     *****************************/
    
    template<typename R = ret_type>
    R sum() { return weighted_f_accumulate_<R>(transformer_, std::plus<R>()); }

    template<typename S = ret_type, typename R = ret_type>
    R weighted_sum(tag_map<TagType, S> coeffs)
    {
      return weighted_f_accumulate_<ret_type>
        (transformer_, [&](const TagType& t){return coeffs[t];}, std::plus<R>());
    }

    template<typename R>
    R f_sum(std::function<R(const ret_type&)> f)
    {
      return weighted_f_accumulate_<R>([&](const DataType& v){return f(transformer_(v));},
                                      std::plus<R>());
    }

    /******************************
     * Averaging functions
     *****************************/

    template<typename R = ret_type>
    R average() { return sum() / num_neighbors(); }

    template<typename S>
    ret_type weighted_average(tag_map<TagType, S> coeffs)
    {
      ret_type num = weighted_sum(std::move(coeffs));
      ret_type denom = weighted_f_accumulate_<S>([&](const TagType& t){return coeffs[t];},
                                                 std::plus<S>());
      return num / denom;
    }

    /******************************
     * Other useful functions
     *****************************/

    std::size_t num_neighbors() { return iter_method_.tags_.size(); }
    
    template<typename R>
    R f_accumulate(std::function<R(const DataType&)> f,
                   std::function<R(R, R)> binary_op)
    {
      return f_accumulate_<R>([&](const DataType& v){return f(transformer_(v));},
                             std::move(binary_op));
    }

    ret_type get_data_unsafe(TagType& tag)
    { return transform_(iter_method_.neighbor_values_[tag]); }

    const std::vector<const TagType*>& get_updated_tags() { return iter_method_.updated_tags_; }

    
  private:
    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const DataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op,
                             R* shift)
    {
      auto tag_iter = iter_method_.tags_.cbegin();
      R val = binary_op(*shift, coef(*tag_iter) * f(iter_method_.neighbor_values_[*tag_iter]));
      for (; tag_iter != iter_method_.tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter) * f(iter_method_.neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const DataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op)
    {
      auto tag_iter = iter_method_.tags_.cbegin();
      R val = coef(*tag_iter) * f(iter_method_.neighbor_values_[*tag_iter]);
      for (; tag_iter != iter_method_.tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter) * f(iter_method_.neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R f_accumulate_(std::function<R(const DataType&)> f,
                    std::function<R(R, R)> binary_op,
                    R* shift)
    {
      auto tag_iter = iter_method_.tags_.cbegin();
      R val = binary_op(*shift, f(iter_method_.neighbor_values_[*tag_iter]));
      for (; tag_iter != iter_method_.tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), f(iter_method_.neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R f_accumulate_(std::function<R(const DataType&)> f,
                    std::function<R(R, R)> binary_op)
    {
      auto tag_iter = iter_method_.tags_.cbegin();
      R val = f(iter_method_.neighbor_values_[*tag_iter]);
      for (; tag_iter != iter_method_.tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), f(iter_method_.neighbor_values_[*tag_iter]));
      return val;
    }

    template<typename R>
    R weighted_f_accumulate_(std::function<R(const TagType&)> coef,
                             std::function<R(R, R)> binary_op)
    {
      auto tag_iter = iter_method_.tags_.cbegin();
      R val = coef(*tag_iter);
      for (; tag_iter != iter_method_.tags_.cend(); ++tag_iter)
        val = binary_op(std::move(val), coef(*tag_iter));
      return val;
    }

    std::function<ret_type(DataType&)> transformer_;
    IterMethod& iter_method_;
  }; // class NeighborDataHandler

} // namespace skynet

#endif // SKYNET_NEIGHBOR_DATA_HANDLER_HPP
