#ifndef SKYNETHELPER_HPP
#define SKYNETHELPER_HPP

#include <chrono>
#include "skynet_core/skynet.hpp"
//#include "skynet_upper/skynet_config.hpp"


namespace skynet::helper
{
  constexpr std::chrono::milliseconds LOOP_DELAY =
    std::chrono::milliseconds(10);

  inline void connect_to_neighbor(skynet::MasterHandle master_handle,
    const std::vector<std::tuple<std::string, uint16_t>>& neighbor_addresses, 
    std::chrono::seconds timeout)
  {
    std::chrono::time_point<std::chrono::steady_clock> time_limit = 
      std::chrono::steady_clock::now() + timeout;
    for (const auto& [ip, port] : neighbor_addresses) {
      while (!master_handle.connect_to_server(ip, port).get()) {
        if (std::chrono::steady_clock::now() > time_limit) {
          std::cerr << "Took too long to connect to " << port << std::endl;
          std::exit(-1);
        }
        std::this_thread::sleep_for(LOOP_DELAY);
      }
    }
  }

  template<typename T>
  void subscribe_to_tag(skynet::Job& job, const T& tag, std::chrono::seconds timeout)
  {
    auto waiter = job.subscribe(tag);
    if (!waiter.wait_for(timeout)) {
      std::cerr << "Could not subscribe to tag " << tag.id() << std::endl;
      std::exit(-1);
    }
  }

  template<typename T>
  void wait_for_data(skynet::Job& job, const T& tag, std::chrono::seconds timeout)
  {
    std::chrono::time_point<std::chrono::steady_clock> time_limit
      = std::chrono::steady_clock::now() + timeout;
    while (!job.has_data(tag)) {
      if (std::chrono::steady_clock::now() > time_limit) {
        std::cerr << "Could not get data from " << tag.id() << std::endl;
        std::exit(-1);
      }
      std::this_thread::sleep_for(LOOP_DELAY);
    }
  }
  //template<typename TagType>
  //auto& await_reduce_group(skynet::Job& job, const skynet::config::ReduceGroupConfig<TagType>& reduce_group_config) {
  //  auto fut = job.create_reduce_group(
  //      reduce_group_config.reduce_group_tag, 
  //      reduce_group_config.reduce_value_tag, 
  //      reduce_group_config.reduce_value_tags
  //  );
  //  auto& group = fut.get();
  //  return group;
  //}

} // namespace skynet::helper


#endif
