#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <regex>

#include "../examples/math_interface_examples/cola/cola.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace skywing;

using OpenVector = AssociativeVector<index_t, scalar_t, true>;
using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

TEST_CASE("COLA", "[mid]")
{
    // Parameters
    size_t num_agents = 3;
    scalar_t lambda = 0.0001;
    bool shift_scale = false;

    const std::uint16_t starting_port = get_starting_port();

    std::string data_dir = std::string(DATA_DIR_DEST) + "/mg";

    std::string A_file = data_dir + "/A.txt";
    std::string b_file = data_dir + "/b.txt";

    std::string output_dir = "./temp_output";
    std::filesystem::create_directories(output_dir); // Create the folder

    // Read global matrix and rhs (for error checking later)
    ClosedMatrix A = ReadAssocitiveMatrix<index_t, scalar_t, false>(A_file);
    ClosedVector b = ReadAssocitiveVector<index_t, scalar_t, false>(b_file);

    // Run COLA
    drive_COLA(starting_port,
               num_agents,
               lambda,
               shift_scale,
               true,
               10,
               data_dir,
               output_dir);

    // Get the outputs from each agent
    std::regex pair_regex(R"(\(\s*(\d+),\s*([-+]?\d*\.\d+|\d+)\s*\))");
    OpenVector x_cola;

    // Iterate over output files
    for (size_t i = 0; i < num_agents; i++) {
        // Open the file
        std::ifstream output_file{output_dir + "/output" + std::to_string(i)
                                  + ".txt"};

        // Get last line of the file
        std::string line, lastline;
        while (std::getline(output_file, line)) {
            lastline = line;
        }

        // Search the line for values
        auto begin =
            std::sregex_iterator(lastline.begin(), lastline.end(), pair_regex);
        auto const end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            int const key = std::stoi((*it)[1].str());
            double const value = std::stod((*it)[2].str());
            x_cola[key] = value;
        }
    }

    // Check the residual error
    ClosedVector b_cola = A.matvec(x_cola);
    ClosedVector residual = b - b_cola;
    double rel_res = residual.dot(residual) / b.dot(b);

    REQUIRE(rel_res < 0.05);
}
