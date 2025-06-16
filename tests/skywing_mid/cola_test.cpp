#include "../examples/cola_driver/cola_driver.hpp"
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iostream>
#include <regex>

using namespace skywing;

TEST_CASE("COLA", "[mid]")
{
    // Parameters
    size_t num_agents = 3;
    size_t topology = 2;
    scalar_t lambda = 0.0001;
    bool shift_scale = false;
    int problem = 2;
    std::string data_file;
    std::string output_dir = "output";

    // Run COLA
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_agents; ++i) {
        threads.emplace_back(drive_COLA, i, num_agents, topology,
                             lambda, shift_scale, problem, data_file, output_dir);
    }
    for (auto&& thread : threads) {
        thread.join();
    }

    // Read results from file and verify
    std::ifstream result_file;
    result_file.open("output/output0.txt");

    // Check that the solution updates and error in v_k go to zero
    std::regex rel_norm_delta_regex(R"(Relative norm delta x\s*=\s*([0-9eE\+\.-]+))");
    std::regex ax_err_regex(R"(Relative error in Ax - v_k\s*=\s*([0-9eE\+\.-]+))");
    std::smatch match;
    double rel_norm_delta, ax_err;

    std::string line;
    while (std::getline(result_file, line)) {
        if (std::regex_search(line, match, rel_norm_delta_regex)) {
            rel_norm_delta = std::stod(match[1]);
        }
        if (std::regex_search(line, match, ax_err_regex)) {
            ax_err = std::stod(match[1]);
        }
    }

    result_file.close();

    REQUIRE(rel_norm_delta < 0.01);
    REQUIRE(ax_err < 0.01);
}
