#ifndef ITERATIVE_TEST_STUFF_HPP
#define ITERATIVE_TEST_STUFF_HPP

using ValueTag = skynet::PublishTag<int>;

template<typename T>
using tag_map = std::unordered_map<ValueTag, T, skynet::internal::hash<ValueTag>>;

int expected_result(ValueTag tag, size_t ind);

struct TestAsyncPublishPolicy
{
public:
  template<typename ValueType>
  bool operator()(const ValueType& new_val, const ValueType& old_val)
  { return new_val != old_val;  }
};

struct TestAsyncStopPolicy
{
  TestAsyncStopPolicy(tag_map<std::vector<int>> publish_values, std::vector<ValueTag> tags)
    : publish_values(publish_values), tags(tags)
  {}
                      
  tag_map<std::vector<int>> publish_values;
  std::vector<ValueTag> tags;
  
  template<typename CallerT>
  bool operator()(const CallerT& caller)
  { return caller.get_processor().stage_of_iteration_ >
      publish_values[tags[caller.get_processor().machine_ind_]].size();
  }
};

class TestAsyncProcessor
{
public:
  using ValueType = int;
  using ValueTag = skynet::PublishTag<ValueType>;

  TestAsyncProcessor(size_t machine_ind, tag_map<std::vector<int>> publish_values,
                     std::vector<ValueTag>& tags, std::mutex& mut)
    : machine_ind_(machine_ind),
      publish_values(publish_values),
      tags(tags),
      catch_mutex(mut)
  {}
  
  ValueType get_init_publish_values()
  {
    return publish_values[tags[machine_ind_]][stage_of_iteration_];
  }

  template<typename CallerT>
  void process_update(const std::vector<ValueTag>& nbr_tags, const std::vector<ValueType>& vals,
                      const CallerT& caller)
  {
    if (caller.tags().size() != tags.size())
    {
      std::cerr << "Unexpected connection drop!\n";
      std::exit(1);
    }
    for (size_t i = 0; i < nbr_tags.size(); i++)
    {
      const auto& received_val = vals[i];
      std::lock_guard g{catch_mutex};
      ++num_values_received_;
      REQUIRE(received_val == publish_values[nbr_tags[i]][stage_of_iteration_]); //expected_result(nbr_tags[i], stage_of_iteration_));
    }
    if (num_values_received_ == tags.size())
    {
      num_values_received_ = 0;
      ++stage_of_iteration_;
    }
  }

  ValueType prepare_for_publication([[maybe_unused]] ValueType vals_to_publish)
  {
    return publish_values[tags[machine_ind_]][stage_of_iteration_];
  }

private:
  size_t stage_of_iteration_ = 0;
  size_t num_values_received_ = 0;
  size_t machine_ind_;
  tag_map<std::vector<int>> publish_values;
  std::vector<ValueTag>& tags;
  std::mutex& catch_mutex;

  friend class TestAsyncPublishPolicy;
  friend class TestAsyncStopPolicy;
};


#endif // ITERATIVE_TEST_STUFF_HPP
