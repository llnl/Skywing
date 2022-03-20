#ifndef ITERATIVE_TEST_STUFF_HPP
#define ITERATIVE_TEST_STUFF_HPP

// using ValueTag = skynet::PublishTag<int>;

// template<typename T>
// using tag_map = std::unordered_map<ValueTag, T, skynet::internal::hash<ValueTag>>;

// template<typename T>
// using data_id_map = std::unordered_map<std::string, T>;

int expected_result(std::string& tag_id, size_t ind);

struct TestAsyncPublishPolicy
{
public:
  template<typename ValueType>
  bool operator()(const ValueType& new_val, const ValueType& old_val)
  { return new_val != old_val;  }
}; // struct TestAsyncPublishPolicy

struct TestAsyncStopPolicy
{
  TestAsyncStopPolicy(std::unordered_map<std::size_t, std::vector<int>> publish_values,
                      std::vector<std::string> tag_ids)
    : publish_values(publish_values), tag_ids(tag_ids)
  {}
                      
  std::unordered_map<std::size_t, std::vector<int>> publish_values;
  std::vector<std::string> tag_ids;
  
  template<typename CallerT>
  bool operator()(const CallerT& caller)
  {
    return caller.get_processor().stage_of_iteration_ >
      publish_values.at(caller.get_processor().machine_ind_).size();
  }
}; // struct TestAsyncStopPolicy

struct TestWaitForNbrsStopPolicy
{
  using ValueType = double;
  
  TestWaitForNbrsStopPolicy(double coef, double stop_val)
    : coef_(coef), stop_val_(stop_val)
  {}

  ValueType get_init_publish_values()
  {
    return get_curr_val();
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      [[maybe_unused]] const IterMethod&)
  {
    ++curr_iter_;
    min_val_ = nbr_data_handler.template f_accumulate<double>
      ([](const double& d){return d;},
       [](double d1, double d2) { return d1 < d2 ? d1 : d2; });
    double curr_val = get_curr_val();
    if (curr_val < min_val_) min_val_ = curr_val;
  }

  ValueType prepare_for_publication([[maybe_unused]] ValueType vals_to_publish)
  { return min_val_; }

  template<typename CallerT>
  bool operator()(const CallerT&)
  { return min_val_ > stop_val_;  }

private:
  double get_curr_val() { return coef_ * curr_iter_; }
  double coef_;
  double stop_val_;
  size_t curr_iter_ = 0;
  double min_val_ = 0;
}; // struct TestWaitForNbrsStopPolicy

class TestAsyncProcessor
{
public:
  using ValueType = int;

  TestAsyncProcessor(size_t machine_ind,
                     std::unordered_map<std::size_t, std::vector<ValueType>> publish_values,
                     std::vector<std::string>& tag_ids, std::mutex& mut)
    : machine_ind_(machine_ind),
      publish_values(publish_values),
      tag_ids(tag_ids),
      catch_mutex(mut)
  {}
  
  ValueType get_init_publish_values()
  {
    return publish_values.at(machine_ind_)[stage_of_iteration_];
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      [[maybe_unused]] const IterMethod&)
  {
    std::cout << "Machine " << machine_ind_ << " about to process values." << std::endl;
    if (nbr_data_handler.num_neighbors() != tag_ids.size())
    {
      std::cerr << "Machine " << machine_ind_ << " expected " << tag_ids.size() << " neighbors but only have " << nbr_data_handler.num_neighbors() << std::endl;
      std::exit(1);
    }
    for (const auto& pTag : nbr_data_handler.get_updated_tags())
    {
      const auto& received_val = nbr_data_handler.get_data_unsafe(*pTag);
      std::size_t ind = static_cast<int>(pTag->id().back()) - '0';
      std::cout << "Machine " << machine_ind_ << " got val " << received_val;
      std::cout << " from tag " << pTag->id();
      std::cout << " on iteration stage " << stage_of_iteration_;
      std::cout << ", val should be " << publish_values.at(ind)[stage_of_iteration_] << std::endl;
      std::lock_guard g{catch_mutex};
      ++num_values_received_;
      REQUIRE(received_val == publish_values.at(ind)[stage_of_iteration_]);
    }
    if (num_values_received_ == tag_ids.size())
    {
      num_values_received_ = 0;
      ++stage_of_iteration_;
    }
    std::cout << "Machine " << machine_ind_ << " finished processing values." << std::endl;
  }

  ValueType prepare_for_publication([[maybe_unused]] ValueType vals_to_publish)
  {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return publish_values.at(machine_ind_)[stage_of_iteration_];
  }

private:
  size_t stage_of_iteration_ = 0;
  size_t num_values_received_ = 0;
  size_t machine_ind_;
  std::unordered_map<std::size_t, std::vector<ValueType>> publish_values;
  std::vector<std::string>& tag_ids;
  std::mutex& catch_mutex;

  friend class TestAsyncPublishPolicy;
  friend class TestAsyncStopPolicy;
}; // class TestAsyncProcessor


#endif // ITERATIVE_TEST_STUFF_HPP
