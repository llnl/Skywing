#include "skywing_core/subscription.hpp"

#include "utils.hpp"

#include <numbers>
#include <span>
#include <type_traits>

#include "skywing_core/tag.hpp"
#include "skywing_core/types.hpp"
#include <catch2/catch_test_macros.hpp>

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
            subscription_data{12.0,
                              std::vector<double>{pi / 4, pi / 2, pi, 3 * pi / 2, 2 * pi},
                              "test",
                              std::vector<std::string>{"str1", "str2", "str3", "str4"}};

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
                std::array<PublishValueVariant, 4> sub_data{data};
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
                std::array<PublishValueVariant, 5> sub_data{data};
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