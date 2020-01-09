#include <catch2/catch.hpp>

#include "skynet/master.hpp"

#include <thread>

using namespace skynet;

constexpr std::uint16_t publisher_port = 10000;
constexpr std::uint16_t subscriber_port = 20000;
constexpr const char* publisher_id = "publisher";
constexpr const char* subscriber_id = "subscriber";

constexpr int num_values_to_publish = 10;
constexpr std::int64_t value_to_publish = 10;

using Int64Tag = PublishTag<std::int64_t>;
const Int64Tag value_tag{"value"};

std::mutex catch_mutex;
int values_published = 0;
// For knowing that the subscriber has completed subscribing
int subscriptions_finished = 0;

void publish_once(int publish_number)
{
  Master master{publisher_port, publisher_id};
  // TODO: I don't think this can be called from the job thread safely
  // Maybe add a queue to master for pending connections to make and a way
  // to add connections to submit to the queue or something
  while (!master.connect_to_server("127.0.0.1", subscriber_port))
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  master.submit_job("job", [&](Job& job) {
    job.declare_publication_intent({value_tag});
    // Wait for the subscriber to finish subscribe
    while (subscriptions_finished <= publish_number)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Wait a bit for the subscription to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    job.publish(value_tag, value_to_publish);
  });
  master.run();
}

void subscriber()
{
  Master master{subscriber_port, subscriber_id};
  master.submit_job("job", [&](Job& job) {
    job.subscribe(value_tag).get();
    while (subscriptions_finished != num_values_to_publish)
    {
      if (subscriptions_finished != 0)
      {
        job.rebuild_missing_tag_connections().wait();
      }
      // Get value from the publisher
      ++subscriptions_finished;
      // wait a bit so the message can arrive
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      const auto value = job.get_future_for(value_tag).get();
      REQUIRE(value);
      REQUIRE(*value == value_to_publish);
      // Trying to get another value will always error as the publishing
      // thread will exit (then rejoin)
      const auto failed_value = job.get_future_for(value_tag).get();
      REQUIRE_FALSE(failed_value);
    }
  });
  master.run();
}

TEST_CASE("Subscribe channels breaking is fine", "[Skynet_BrokenSubscribe]")
{
  std::thread subscriber_thread{subscriber};
  for (int i = 0; i < num_values_to_publish; ++i)
  {
    std::thread publish_thread{publish_once, i};
    publish_thread.join();
  }
  subscriber_thread.join();
}
