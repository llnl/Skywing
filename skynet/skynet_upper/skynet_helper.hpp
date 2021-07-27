#ifndef SKYNETHELPER_HPP
#define SKYNETHELPER_HPP

#include <chrono>
#include "skynet_core/skynet.hpp"
#include "skynet_upper/skynet_config.hpp"


namespace skynet::helper
{
  constexpr std::chrono::milliseconds LOOP_DELAY =
    std::chrono::milliseconds(10);

  inline void connect_to_neighbors(const std::vector<uint16_t>& neighbor_ports,
    skynet::MasterHandle master_handle, std::chrono::seconds timeout)
  {
    std::chrono::time_point<std::chrono::steady_clock> time_limit
      = std::chrono::steady_clock::now() + timeout;
    for (const auto& port : neighbor_ports)
    {
      while (!master_handle.connect_to_server("127.0.0.1", port).get())
      {
        if (std::chrono::steady_clock::now() > time_limit)
        {
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
    if (!waiter.wait_for(timeout))
    {
      std::cerr << "Could not subscribe to tag " << tag.id() << std::endl;
      std::exit(-1);
    }
  }

  template<typename T>
  void wait_for_data(skynet::Job& job, const T& tag)
  {
    unsigned count = 0;
    while (!job.has_data(tag))
    {
      count++;
      if (count >= 50)
        std::cout << "Waiting on " << tag.id() << std::endl;
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
