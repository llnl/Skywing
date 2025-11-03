#include <chrono>
#include <iostream>
#include <vector>

#include <skywing_core/skywing.hpp>

std::vector<size_t> ports{20000, 20001, 20002};
using pubtag_t = skywing::Tag<int>;
std::vector<pubtag_t> tags = {
    pubtag_t{"tag0"}, pubtag_t{"tag1"}, pubtag_t{"tag2"}};

int main(const int argc, const char* const argv[])
{
    using namespace skywing;
    size_t machine_num = std::stoul(argv[1]);
    std::string machine_name = std::string("machine") + argv[1];
    Manager manager(ports[machine_num], machine_name);
    // make initial connections
    if (machine_num > 0)
        manager.configure_initial_neighbors("localhost",
                                            ports[machine_num - 1]);

    auto pubsub_job = [&](Job& job, ManagerHandle manager_handle) {
        // declare publication, subscribe and wait for subscriptions
        job.declare_publication_intent(tags[machine_num]);
        for (size_t i = machine_num; i < tags.size(); i++)
            job.subscribe(tags[i]).wait();

        // repeatedly submit and receive values
        int pub_val = 100 * machine_num;
        for (size_t i = 0; i < 10; i++) {
            job.publish(tags[machine_num], pub_val);
            for (size_t i = machine_num; i < tags.size(); i++) {
                if (std::optional<int> nbr_val = job.get_waiter(tags[i]).get())
                {
                    std::cout << machine_name << " received value " << *nbr_val;
                    std::cout << " from neighbor at port " << ports[i]
                              << std::endl;
                }
            }
            pub_val++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    };
    manager.submit_job("job", pubsub_job);
    manager.run();
}
