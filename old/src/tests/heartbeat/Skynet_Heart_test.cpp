#include "catch2/catch.hpp"
#include "devices/Skynet_Socket.hpp"
#include "devices/Skynet_SocketGateway.hpp"
#include "heartbeat/Skynet_Heart.hpp"
#include "heartbeat/Skynet_TrivialBeatInterpreter.hpp"
#include "heartbeat/Skynet_TrivialBeatSender.hpp"
#include "heartbeat/Skynet_TrivialDeviceManager.hpp"
#include "heartbeat/Skynet_TrivialPropertyChecker.hpp"
#include "heartbeat/Skynet_TrivialPulseTimer.hpp"

TEST_CASE("Single instantiation and connection", "[Skynet_Heart]")
{
  constexpr uint16_t skynet_port = 6000;
  // This device starts first and has nobody to connect to
  {
    std::ofstream outfile("heart_test_config.txt");
    outfile
      << "skynet_port\t" << skynet_port << '\n'
      << "number_of_devices\t0\n"
      << "address_type\tIPv4\n"
      << "task_cycle_pause\t1\n";
  }
  const skynet::KeyValueReader skynet_config("heart_test_config.txt", "\t");

  std::cout << "Starting heartbeat on T1\n";
  skynet::Heart heart_T1(
    std::make_unique<skynet::TrivialBeatSender>(),
    std::make_unique<skynet::TrivialBeatInterpreter>(),
    std::make_unique<skynet::TrivialPulseTimer>(),
    std::make_unique<skynet::TrivialDeviceManager>(
      std::make_unique<skynet::SocketGateway>(skynet_config)
    ),
    std::make_unique<skynet::TrivialPropertyChecker>(),
    skynet_config
  );
  heart_T1.activate();
  std::cout << "Terminating T1\n";
  heart_T1.terminate();
}

TEST_CASE("Instantiate 3 hearts in sequence", "[Skynet_Heart]")
{
  constexpr uint16_t skynet_port_t1 = 6100;
  constexpr uint16_t skynet_port_t2 = 6200;
  constexpr uint16_t skynet_port_t3 = 6300;
  {
    std::ofstream outfile("heart_test_config_T1.txt");
    // This heart starts first and has nobody to connect to
    outfile
      << "skynet_port\t" << skynet_port_t1 << '\n'
      << "number_of_devices\t0\n"
      << "address_type\tIPv4\n"
      << "task_cycle_pause\t1\n";
  }
  const skynet::KeyValueReader config_T1("heart_test_config_T1.txt", "\t");
  std::cout << "Starting heartbeat on T1\n";
  skynet::Heart heart_T1(
    std::make_unique<skynet::TrivialBeatSender>(),
    std::make_unique<skynet::TrivialBeatInterpreter>(),
    std::make_unique<skynet::TrivialPulseTimer>(),
    std::make_unique<skynet::TrivialDeviceManager>(
      std::make_unique<skynet::SocketGateway>(config_T1)
    ),
    std::make_unique<skynet::TrivialPropertyChecker>(),
    config_T1
  );
  heart_T1.activate();

  // This heart starts second and connects to T1
  {
    std::ofstream outfile("heart_test_config_T2.txt");
    outfile
      << "skynet_port\t" << skynet_port_t2 << '\n'
      << "number_of_devices\t1\n"
      << "address_type\tIPv4\n"
      << "device1_ip_address\t127.0.0.1\n"
      << "device1_port\t" << skynet_port_t1 << '\n'
      << "task_cycle_pause\t1\n";
  }
  const skynet::KeyValueReader config_T2("heart_test_config_T2.txt", "\t");
  std::cout << "Starting heartbeat on T2\n";
  skynet::Heart heart_T2(
    std::make_unique<skynet::TrivialBeatSender>(),
    std::make_unique<skynet::TrivialBeatInterpreter>(),
    std::make_unique<skynet::TrivialPulseTimer>(),
    std::make_unique<skynet::TrivialDeviceManager>(
      std::make_unique<skynet::SocketGateway>(config_T2)
    ),
    std::make_unique<skynet::TrivialPropertyChecker>(),
    config_T2
  );
  heart_T2.activate();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  REQUIRE(heart_T1.number_of_connections() == 1);
  REQUIRE(heart_T2.number_of_connections() == 1);

  // This heart starts third and connects to T1 and T2
  {
    std::ofstream outfile("heart_test_config_T3.txt");
    outfile
      << "skynet_port\t" << skynet_port_t3 << '\n'
      << "number_of_devices\t2\n"
      << "address_type\tIPv4\n"
      << "device1_ip_address\t127.0.0.1\n"
      << "device1_port\t" << skynet_port_t1 << '\n'
      << "device2_ip_address\t127.0.0.1\n"
      << "device2_port\t" << skynet_port_t2 << '\n'
      << "task_cycle_pause\t1\n";
  }
  const skynet::KeyValueReader config_T3("heart_test_config_T3.txt", "\t");
  std::cout << "Starting heartbeat on T3\n";
  skynet::Heart heart_T3(
    std::make_unique<skynet::TrivialBeatSender>(),
    std::make_unique<skynet::TrivialBeatInterpreter>(),
    std::make_unique<skynet::TrivialPulseTimer>(),
    std::make_unique<skynet::TrivialDeviceManager>(
      std::make_unique<skynet::SocketGateway>(config_T3)
    ),
    std::make_unique<skynet::TrivialPropertyChecker>(),
    config_T3
  );
  heart_T3.activate();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  REQUIRE(heart_T1.number_of_connections() == 2);
  REQUIRE(heart_T2.number_of_connections() == 2);
  REQUIRE(heart_T3.number_of_connections() == 2);

  std::cout << "Terminating T1\n";
  heart_T1.terminate();
  std::cout << "Terminating T2\n";
  heart_T2.terminate();
  std::cout << "Terminating T3\n";
  heart_T3.terminate();
}
