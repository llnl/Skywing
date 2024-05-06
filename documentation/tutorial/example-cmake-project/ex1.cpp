#include <iostream>

#include <skywing_core/skywing.hpp>

int main(int argc, char* argv[])
{
    using namespace skywing;
    std::string machine_name = argv[1];
    std::uint16_t machine_port =
        static_cast<std::uint16_t>(std::stoul(argv[2]));
    Manager manager(machine_port, machine_name);
    manager.submit_job("job_name", [&](Job& job, ManagerHandle manager_handle) {
        std::cout << "Hello Skywing world from agent " << machine_name << "!"
                  << std::endl;
    });
    manager.run();
    return EXIT_SUCCESS;
}
