#include <catch2/catch.hpp>

#include "skynet_core/skynet.hpp"
#include "skynet_core/enable_logging.hpp"
#include <iostream>

using namespace skynet;

using ValueTag = ReduceValueTag<std::int32_t>;
const ReduceGroupTag<std::int32_t> reduce_tag1{"reduce op1"};
const std::vector<ValueTag> tags1{ValueTag{"tag1"}, ValueTag{"tag2"}};
const ReduceGroupTag<std::int32_t> reduce_tag2{"reduce op2"};
const std::vector<ValueTag> tags2{ValueTag{"tag3"}, ValueTag{"tag4"}};

TEST_CASE("Reduce groups with same machines work", "[Skynet_ReduceTagBug]")
{
  const auto make_task = [&](int index, std::uint16_t port) -> std::thread {
    return std::thread{[index, port]() {
      Master master_base{port, std::to_string(port)};
      master_base.submit_job("job", [&](Job& job, MasterHandle) {
        auto g1 = job.create_reduce_group(reduce_tag1, tags1[index], tags1);
        auto g2 = job.create_reduce_group(reduce_tag2, tags2[index], tags2);
        g1.wait();
        g2.wait();
      });
      master_base.run();
    }};
  };
  make_task(0, 10000).detach();
  make_task(1, 20000).detach();
  Master master_base{30000, "glue"};
  master_base.submit_job("job", [&](Job&, MasterHandle master) {
    std::cout << "Henlo\n";
    while (true) { if (master.connect_to_server("127.0.0.1", 10000).get()) { break; } }
    while (true) { if (master.connect_to_server("127.0.0.1", 20000).get()) { break; } }
    // sleep to allow information to exchange
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  });
  master_base.run();
}
