#include "skywing_core/subscription.hpp"

#include "utils.hpp"

#include <numbers>
#include <span>
#include <type_traits>
#include <cstdint>

#include "skywing_core/tag.hpp"
#include "skywing_core/types.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace skywing;
using namespace std::numbers;

TEST_CASE("Subscription Test", "[core]")
{
    Tag<double> tag{"0"};
    Subscription subscription{tag};

    SECTION("Object creation requires tag")
    {
        REQUIRE(!std::is_default_constructible<Subscription>::value);
    }

    SECTION("Object is not copyable")
    {
        REQUIRE(!std::is_copy_constructible_v<Subscription>);
    }

    SECTION("Subscription has data after adding data to buffer")
    {
        std::array<PublishValueVariant, 1> data{2.0};
        const VersionID version = 1U;
        subscription.add_data(data, version);
        REQUIRE(subscription.has_data());
    }

    SECTION("Subscription can be reset")
    {
        std::array<PublishValueVariant, 1> data{2.0};
        const VersionID version = 1U;
        subscription.add_data(data, version);
        subscription.reset();
        REQUIRE(!subscription.has_data());
        REQUIRE(!subscription.has_error());
    }

    SECTION("Change connection status to disconnected")
    {
        subscription.mark_tag_as_dead();
        REQUIRE(subscription.is_disconnected());
    }

    SECTION("Subscription with discarded tag has error state")
    {
        subscription.discard_tag();
        REQUIRE(subscription.has_incorrect_type());
    }
}

template <typename... Ts>
SubscribeDataAssert<Ts...> checkIf(std::span<PublishValueVariant> data)
{
    return SubscribeDataAssert<Ts...>{data};
}

TEST_CASE("Store Data as Subscription Works", "[core]")
{
    Manager manager{get_starting_port(), "0"};

    manager.submit_job("job", [&](Job& job, ManagerHandle manager_handle) {
        (void) manager_handle;

        std::vector<std::variant<double,
                                std::vector<double>,
                                std::string,
                                std::vector<std::string>>>
            subscription_data{  12.0,
                                std::vector<double>{2 * pi},
                                "test",
                                std::vector<std::string>{"str1"}};

        for (const auto& variant : subscription_data) {
            if (std::holds_alternative<double>(variant)) {
                double data = std::get<double>(variant);
                Tag<double> tag{"0"};
                std::array<PublishValueVariant, 1> sub_data{data};
                job.declare_publication_intent(tag);
                job.subscribe(tag);
                job.publish(tag, data);
                checkIf<double>(sub_data).isStoredUnderTag(tag, job);
            }
            else if (std::holds_alternative<std::string>(variant)) {
                std::string data = std::get<std::string>(variant);
                Tag<std::string> tag{"1"};
                std::array<PublishValueVariant, 1> sub_data{data};
                job.declare_publication_intent(tag);
                job.subscribe(tag);
                job.publish(tag, data);
                checkIf<std::string>(sub_data).isStoredUnderTag(tag, job);
            }
            else if (std::holds_alternative<std::vector<double>>(variant)) {
                std::vector<double> data =
                    std::get<std::vector<double>>(variant);
                const Tag<std::vector<double>> tag{"2"};
                std::array<PublishValueVariant, 1> sub_data{data};
                job.declare_publication_intent(tag);
                job.subscribe(tag);
                job.publish(tag, data);
                checkIf<std::vector<double>>(sub_data).isStoredUnderTag(tag,
                                                                        job);
            }
            else if (std::holds_alternative<std::vector<std::string>>(variant))
            {
                std::vector<std::string> data =
                    std::get<std::vector<std::string>>(variant);
                const Tag<std::vector<std::string>> tag{"3"};
                std::array<PublishValueVariant, 1> sub_data{data};
                job.declare_publication_intent(tag);
                job.subscribe(tag);
                job.publish(tag, data);
                checkIf<std::vector<std::string>>(sub_data).isStoredUnderTag(
                    tag, job);
            }
        }
    });

    manager.run();
}

TEST_CASE("Store Multiple-Value Data as Subscription Works", "[core]")
{
    auto int_data = GENERATE(1, 2, 3, 4);
    auto uint_data = GENERATE(as<std::uint16_t>{}, 1u, 2u, 3u, 4u);

    SECTION("Store int and unsigned int types")
    {
        Manager manager{get_starting_port(), "0"};
        manager.submit_job("job", [&](Job& job, ManagerHandle manager_handle) {
            (void) manager_handle;
            Tag<int, std::uint16_t> tag {"0"};
            job.declare_publication_intent(tag);
            job.subscribe(tag);
            job.publish(tag, int_data, uint_data);
            std::array<PublishValueVariant, 2> subscription_data {int_data, uint_data};
            checkIf<int, std::uint16_t>(subscription_data).isStoredUnderTag(tag, job);
        });

        manager.run();
    }

    auto str_data = GENERATE(as<std::string>{}, "a", "bb", "ccc", "dddd");

    SECTION("Store int, unsigned int, and string types")
    {
        Manager manager{get_starting_port(), "0"};
        manager.submit_job("job", [&](Job& job, ManagerHandle manager_handle) {
            (void) manager_handle;
            Tag<int, std::uint16_t, std::string> tag {"0"};
            job.declare_publication_intent(tag);
            job.subscribe(tag);
            job.publish(tag, int_data, uint_data, str_data);
            std::array<PublishValueVariant, 3> subscription_data {int_data, uint_data, str_data};
            checkIf<int, std::uint16_t, std::string>(subscription_data).isStoredUnderTag(tag, job);
        });

        manager.run();
    }
}