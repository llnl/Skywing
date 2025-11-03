#ifndef SKYWINGHELPER_HPP
#define SKYWINGHELPER_HPP

#include <chrono>

#include "skywing_core/skywing.hpp"

namespace skywing::helper
{
constexpr std::chrono::milliseconds LOOP_DELAY = std::chrono::milliseconds(10);

template <typename T>
void subscribe_to_tag(skywing::Job& job,
                      const T& tag,
                      std::chrono::seconds timeout)
{
    auto waiter = job.subscribe(tag);
    if (!waiter.wait_for(timeout)) {
        std::cerr << "Could not subscribe to tag " << tag.id() << std::endl;
        std::exit(-1);
    }
}

template <typename T>
void wait_for_data(skywing::Job& job,
                   const T& tag,
                   std::chrono::seconds timeout)
{
    std::chrono::time_point<std::chrono::steady_clock> time_limit =
        std::chrono::steady_clock::now() + timeout;
    while (!job.has_data(tag)) {
        if (std::chrono::steady_clock::now() > time_limit) {
            std::cerr << "Could not get data from " << tag.id() << std::endl;
            std::exit(-1);
        }
        std::this_thread::sleep_for(LOOP_DELAY);
    }
}

} // namespace skywing::helper

#endif
