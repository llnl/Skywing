#include "utils.hpp"
#include <catch2/catch_test_macros.hpp>
#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/data_handler.hpp"  // Include the header file for the DataHandler class

using namespace skywing;

TEST_CASE("DataHandler Constructor Initializes Correctly", "[DataHandler]") {
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {{"tag1", 10}, {"tag2", 20}, {"tag3", 30}};
    DataHandler<int> handler(tags, neighbor_values);

    REQUIRE(handler.num_neighbors() == tags.size());
    REQUIRE(handler.get_data("tag1") == 10);
    REQUIRE(handler.get_data("tag2") == 20);
    REQUIRE(handler.get_data("tag3") == 30);
}

TEST_CASE("DataHandler Update Function Works", "[DataHandler]") {
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {{"tag1", 10}, {"tag2", 20}, {"tag3", 30}};
    DataHandler<int> handler(tags, neighbor_values);

    std::unordered_map<std::string, int> new_values = {{"tag1", 15}, {"tag4", 40}};
    handler.update(new_values);

    REQUIRE(handler.get_data("tag1") == 15);
    REQUIRE(handler.get_data("tag4") == 40);
    REQUIRE_THROWS_AS(handler.get_data("tag5"), std::runtime_error);
}

TEST_CASE("DataHandler Sum Function Works", "[DataHandler]") {
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {{"tag1", 10}, {"tag2", 20}, {"tag3", 30}};
    DataHandler<int> handler(tags, neighbor_values);

    REQUIRE(handler.sum() == 60);
}

TEST_CASE("DataHandler Average Function Works", "[DataHandler]") {
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {{"tag1", 10}, {"tag2", 20}, {"tag3", 30}};
    DataHandler<int> handler(tags, neighbor_values);

    REQUIRE(handler.average() == 20);
}

TEST_CASE("recvd_extreme finds minimum and maximum correctly", "[recvd_extreme]") {
    std::vector<std::string> tags = {"a", "b", "c"};
    std::unordered_map<std::string, int> neighbor_values = {
        {"a", 10},
        {"b", 20},
        {"c", 5}
    };

    skywing::DataHandler<int> data_handler(tags, neighbor_values);

    SECTION("Find minimum value") {
        int min_value = data_handler.recvd_extreme();
        REQUIRE(min_value == 5); // Expected minimum value is 5
    }

    SECTION("Find maximum value") {
        int max_value = data_handler.recvd_extreme(false);
        REQUIRE(max_value == 20); // Expected maximum value is 20
    }
}


TEST_CASE("DataHandler get_sub_handler with tuple transformation", "[DataHandler]") {
    // Initial setup with tuple data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, std::tuple<int, int>> neighbor_values = {
        {"tag1", std::make_tuple(10, 100)},
        {"tag2", std::make_tuple(20, 200)},
        {"tag3", std::make_tuple(30, 300)}
    };
    DataHandler<std::tuple<int, int>> handler(tags, neighbor_values);

    // Define a transformation function to extract the second element of the tuple
    auto transform_to_second = [](const std::tuple<int, int>& value) -> int {
        return std::get<1>(value);  // Extract the second element
    };

    // Get a sub-handler with transformed data
    DataHandler<int> sub_handler = handler.get_sub_handler<int>(transform_to_second);

    // Verify the transformed data
    REQUIRE(sub_handler.get_data("tag1") == 100);
    REQUIRE(sub_handler.get_data("tag2") == 200);
    REQUIRE(sub_handler.get_data("tag3") == 300);

    // Check the number of neighbors in the sub-handler
    REQUIRE(sub_handler.num_neighbors() == tags.size());
}

TEST_CASE("DataHandler get_kth_index_handler extracts k-th element from tuples", "[DataHandler]") {
    // Initial setup with tuple data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, std::tuple<int, int, int>> neighbor_values = {
        {"tag1", std::make_tuple(10, 100, 1000)},
        {"tag2", std::make_tuple(20, 200, 2000)},
        {"tag3", std::make_tuple(30, 300, 3000)}
    };
    DataHandler<std::tuple<int, int, int>> handler(tags, neighbor_values);

    // Test for extracting the second element (index 1) of the tuple
    DataHandler<int> second_element_handler = handler.get_kth_index_handler<int, 1>();

    // Verify the extracted data
    REQUIRE(second_element_handler.get_data("tag1") == 100);
    REQUIRE(second_element_handler.get_data("tag2") == 200);
    REQUIRE(second_element_handler.get_data("tag3") == 300);

    // Test for extracting the third element (index 2) of the tuple
    DataHandler<int> third_element_handler = handler.get_kth_index_handler<int, 2>();

    // Verify the extracted data
    REQUIRE(third_element_handler.get_data("tag1") == 1000);
    REQUIRE(third_element_handler.get_data("tag2") == 2000);
    REQUIRE(third_element_handler.get_data("tag3") == 3000);

    // Check the number of neighbors in the sub-handlers
    REQUIRE(second_element_handler.num_neighbors() == tags.size());
    REQUIRE(third_element_handler.num_neighbors() == tags.size());
}

TEST_CASE("DataHandler f_accumulate applies function and combines results", "[DataHandler]") {
    // Initial setup with integer data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {
        {"tag1", 10},
        {"tag2", 20},
        {"tag3", 30}
    };
    DataHandler<int> handler(tags, neighbor_values);

    // Define a function to apply to each data element
    auto square_function = [](const int& value) -> int {
        return value * value;
    };

    // Define a binary operation to combine results
    auto sum_operation = [](int a, int b) -> int {
        return a + b;
    };

    // Use f_accumulate to apply the function and combine results
    int accumulated_result = handler.f_accumulate<int>(square_function, sum_operation);

    // Verify the accumulated result
    // Expected: 10^2 + 20^2 + 30^2 = 100 + 400 + 900 = 1400
    REQUIRE(accumulated_result == 1400);
}

TEST_CASE("DataHandler recvd_data_values returns correct data pointers", "[DataHandler]") {
    // Initial setup with integer data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {
        {"tag1", 10},
        {"tag2", 20},
        {"tag3", 30}
    };
    DataHandler<int> handler(tags, neighbor_values);

    // Get received data values
    auto received_values = handler.recvd_data_values<int>();

    // Verify the received data values
    std::unordered_set<int> expected_values = {10, 20, 30};
    for (const auto& value_ptr : received_values) {
        REQUIRE(expected_values.find(*value_ptr) != expected_values.end());
    }
    REQUIRE(received_values.size() == expected_values.size());
}

TEST_CASE("DataHandler recvd_data_tags returns correct tags", "[DataHandler]") {
    // Initial setup with integer data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {
        {"tag1", 10},
        {"tag2", 20},
        {"tag3", 30}
    };
    DataHandler<int> handler(tags, neighbor_values);

    // Get received data tags
    auto received_tags = handler.recvd_data_tags();

    // Verify the received data tags
    REQUIRE(received_tags == tags);
}

TEST_CASE("DataHandler num_neighbors returns correct number of neighbors", "[DataHandler]") {
    // Initial setup with integer data
    std::vector<std::string> tags = {"tag1", "tag2", "tag3"};
    std::unordered_map<std::string, int> neighbor_values = {
        {"tag1", 10},
        {"tag2", 20},
        {"tag3", 30}
    };
    DataHandler<int> handler(tags, neighbor_values);

    // Verify the number of neighbors
    REQUIRE(handler.num_neighbors() == tags.size());
}