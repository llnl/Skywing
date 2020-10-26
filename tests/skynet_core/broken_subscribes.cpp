#include <catch2/catch.hpp>

#include "skynet_core/enable_logging.hpp"
#include "skynet_core/master.hpp"

#include <thread>

using namespace skynet;

constexpr std::uint16_t publisher_port = 10000;
constexpr std::uint16_t subscriber_port = 20000;
constexpr const char* publisher_id = "publisher";
constexpr const char* subscriber_id = "subscriber";

constexpr int num_values_to_publish = 5;
constexpr std::int64_t value_to_publish = 10;

using Int64Tag = PublishTag<std::int64_t>;
const Int64Tag value_tag{"value"};

std::mutex catch_mutex;
std::atomic<int> values_retrieved = 0;

void publish_once(int publish_number)
{
  // Wait to start to allow the subscriber to notice that the publisher has
  // disconnected so it won't discard this connection for re-using the id
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  Master base_master{publisher_port, publisher_id};
  base_master.submit_job("job", [&](Job& job, MasterHandle master) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    while (!master.connect_to_server("127.0.0.1", subscriber_port).get()) { /* nothing */
    }
    job.declare_publication_intent(value_tag);
    while (master.number_of_subscribers(value_tag) == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    job.publish(value_tag, value_to_publish);
    // Wait for the value to be retrieved
    while (values_retrieved <= publish_number) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  base_master.run();
}

void subscriber()
{
  Master base_master{subscriber_port, subscriber_id};
  base_master.submit_job("job", [&](Job& job, MasterHandle) {
    job.subscribe(value_tag).get();
    while (values_retrieved != num_values_to_publish) {
      if (values_retrieved != 0) {
        // wait a bit so the publisher can disconnect
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        job.rebuild_missing_tag_connections().wait();
      }
      // Get value from the publisher
      const auto value = job.get_waiter(value_tag).get();
      REQUIRE(value);
      REQUIRE(*value == value_to_publish);
      ++values_retrieved;
      // Trying to get another value will always error as the publishing
      // thread will exit (then rejoin)
      const auto failed_value = job.get_waiter(value_tag).get();
      REQUIRE_FALSE(failed_value);
    }
  });
  base_master.run();
}

TEST_CASE("Subscribe channels breaking is fine", "[Skynet_BrokenSubscribe]")
{
  std::thread subscriber_thread{subscriber};
  for (int i = 0; i < num_values_to_publish; ++i) {
    std::thread publish_thread{publish_once, i};
    publish_thread.join();
  }
  subscriber_thread.join();
}
