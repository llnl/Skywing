#include <limits>
#include <type_traits>

#include "skywing_core/tag.hpp"
#include "skywing_mid/idempotent_processor.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/data_handler.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

using IValueT = std::tuple<double, std::uint64_t, std::uint64_t>;
using DataHandlerT = DataHandler<IValueT>;
using TagType = Tag<double, std::uint64_t, std::uint64_t>;

using OtherTagType =
    UnwrapAndApply_t<PubSubConverter<IValueT>::pubsub_type, Tag>;
static_assert(std::is_same_v<TagType, OtherTagType>);
static_assert(std::is_same_v<IValueT, MaxProcessor<double>::ValueType>);

tag_map<TagType, IValueT> make_nbr_values(double d1,
                                          std::uint64_t v1,
                                          std::uint64_t wc1,
                                          double d2,
                                          std::uint64_t v2,
                                          std::uint64_t wc2,
                                          double d3,
                                          std::uint64_t v3,
                                          std::uint64_t wc3,
                                          std::vector<TagType> tags)
{
    tag_map<TagType, IValueT> nbr_values = {{tags[0], {d1, v1, wc1}},
                                            {tags[1], {d2, v2, wc2}},
                                            {tags[2], {d3, v3, wc3}}};
    return nbr_values;
}
template <typename IValueT>
std::tuple<std::vector<std::string>, std::unordered_map<std::string, IValueT>> transform_data(
    const std::vector<TagType>& tags, const tag_map<TagType, IValueT>& nbr_values) {
    
    // Transform tags to a vector of strings
    std::vector<std::string> transformed_tags;
    for (const auto& tag : tags) {
        transformed_tags.push_back(tag.id());
    }
    
    // Identity function for IValueT
    auto ident = [](const IValueT& iv) -> const IValueT& {
        return iv;
    };
    
    // Transform neighbor values to an unordered map with string keys
    std::unordered_map<std::string, IValueT> transformed_values;
    for (const auto& [tag, data] : nbr_values) {
        transformed_values[tag.id()] = ident(data);
    }
    
    return {transformed_tags, transformed_values};
}
    
TEST_CASE("Idempotent Processor", "[mid][unit]")
{
    // make MaxProcessor (ie IdempotentProcessor with max operator)
    MaxProcessor<double> max_proc(1.0);

    // data needed to build NeighborDataHandler object
    std::vector<TagType> tags = {{"A"}, {"B"}, {"C"}};
    tag_map<TagType, IValueT> nbr_values =
        make_nbr_values(0.0, 0, 0, 1.0, 0, 0, 2.0, 0, 0, tags);
    std::vector<const TagType*> updated_tags;

    // values are 1.0 from me, and 0.0, 1.0, and 2.0 from others
    // so max is 2.0, version 0
    auto transformed_data  = transform_data(tags,nbr_values);
    auto& transformed_tags = std::get<0>(transformed_data);
    auto& transformed_values = std::get<1>(transformed_data);
    DataHandlerT ndh1(transformed_tags, transformed_values);
    max_proc.process_update(ndh1, nullptr);
    REQUIRE(max_proc.get_value() == 2.0);
    REQUIRE(max_proc.get_version() == 0);

    // set value to 3.0, increments my version by 1
    // new max is 3.0, version 1
    max_proc.set_value(3.0);
    nbr_values = make_nbr_values(0.0, 1, 0, 1.0, 1, 0, 2.0, 1, 0, tags);
    transformed_data  = transform_data(tags,nbr_values);
    transformed_tags = std::get<0>(transformed_data);
    transformed_values = std::get<1>(transformed_data);    
    DataHandlerT ndh2(transformed_tags, transformed_values);    
    max_proc.process_update(ndh2, nullptr);
    REQUIRE(max_proc.get_value() == 3.0);
    REQUIRE(max_proc.get_version() == 1);

    // new neighbor data, with one of them at version 2 but my
    // contribution is always considered up to date, so its version is
    // increased to 2
    // so max is 3.0, version 2
    nbr_values = make_nbr_values(0.0, 1, 0, 1.0, 2, 0, 2.0, 1, 0, tags);
    transformed_data  = transform_data(tags,nbr_values);
    transformed_tags = std::get<0>(transformed_data);
    transformed_values = std::get<1>(transformed_data);     
    DataHandlerT ndh3(transformed_tags, transformed_values);    
    max_proc.process_update(ndh3, nullptr);
    REQUIRE(max_proc.get_value() == 3.0);
    REQUIRE(max_proc.get_version() == 2);

    // set value to 0.0. increments version to 3
    // high version dominates
    // so max is 0.0, version 3
    max_proc.set_value(0.0);
    REQUIRE(max_proc.get_value() == 0.0);
    REQUIRE(max_proc.get_version() == 3);

    // new neighbor data, with one of them at version 4.  my
    // contribution is always considered up to date, so its version is
    // bumped to 4
    // but my contribution is 0.0, and 1.0 is larger
    // so max is 1.0, version 4
    nbr_values = make_nbr_values(0.0, 3, 0, 1.0, 4, 0, 2.0, 3, 0, tags);
    transformed_data  = transform_data(tags,nbr_values);
    transformed_tags = std::get<0>(transformed_data);
    transformed_values = std::get<1>(transformed_data);
    DataHandlerT ndh4(transformed_tags, transformed_values);    
    max_proc.process_update(ndh4, nullptr);
    REQUIRE(max_proc.get_value() == 1.0);
    REQUIRE(max_proc.get_version() == 4);

    // setting my value increases version to 5
    // high version dominates
    // so max is 0.0, version 5
    max_proc.set_value(0.0);
    REQUIRE(max_proc.get_value() == 0.0);
    REQUIRE(max_proc.get_version() == 5);

    // check wrap around of version counter (64-bit integer) in case of
    // overflow: new neighbor data, where the version count for nbr 3 is the
    // maximum value of std::uint64_t. setting my value increases the version to
    // std::numeric_limits<std::uint64_t>::max() + 1 which wraps to 0 (and
    // version_wrap_count is incremented to 1 as well) so the high wrap count
    // dominates to max is 3.0, version 0, wrap count 1.
    nbr_values = make_nbr_values(0.0,
                                 3,
                                 0,
                                 1.0,
                                 4,
                                 0,
                                 2.0,
                                 std::numeric_limits<std::uint64_t>::max(),
                                 0,
                                 tags);
    transformed_data  = transform_data(tags,nbr_values);
    transformed_tags = std::get<0>(transformed_data);
    transformed_values = std::get<1>(transformed_data);     
    DataHandlerT ndhr5(transformed_tags, transformed_values);    
    max_proc.process_update(ndhr5, nullptr);
    max_proc.set_value(3.0);
    REQUIRE(max_proc.get_value() == 3.0);
    REQUIRE(max_proc.get_version() == 0);
    REQUIRE(max_proc.get_version_wrap_count() == 1);

    // setting my value increases version to 1
    // high version dominates
    // so max is 0.0, version 1, wrap count 1
    max_proc.set_value(0.0);
    REQUIRE(max_proc.get_value() == 0.0);
    REQUIRE(max_proc.get_version() == 1);
    REQUIRE(max_proc.get_version_wrap_count() == 1);
}
