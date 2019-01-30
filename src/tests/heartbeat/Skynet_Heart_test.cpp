#include "catch2/catch.hpp"
#include "devices/Skynet_Socket.hpp"
#include "devices/Skynet_SocketGateway.hpp"
#include "heartbeat/Skynet_Heart.hpp"
#include "heartbeat/Skynet_TrivialBeatInterpreter.hpp"
#include "heartbeat/Skynet_TrivialBeatSender.hpp"
#include "heartbeat/Skynet_TrivialDeviceManager.hpp"
#include "heartbeat/Skynet_TrivialPropertyChecker.hpp"
#include "heartbeat/Skynet_TrivialPulseTimer.hpp"

TEST_CASE( "Single instantiation and connection", "[Skynet_Heart]" )
{
  const uint16_t SKYNET_PORT = 5000;
  int IPv4 = skynet::Socket::IPv4;
  // This device starts first and has nobody to connect to
  std::ofstream outfile("test_config.txt");
  outfile << "skynet_port\t" << SKYNET_PORT << std::endl;
  outfile << "number_of_devices\t0" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile.close();
  skynet::KeyValueReader skynet_config("test_config.txt", "\t");

  std::cout << "Starting heartbeat on T1" << std::endl;
  skynet::Heart heart_T1(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(skynet_config)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T1.run_heartbeat();

  std::cout << "Creating Communicator Factory to T1" << std::endl;
  skynet::SocketCommunicatorFactory comm_factory(IPv4, "127.0.0.1", SKYNET_PORT);

  std::cout << "Creating Device Communicator to T1" << std::endl;
  std::unique_ptr<skynet::DeviceCommunicator> communicator =
    comm_factory.create_new_communicator(std::vector<std::string>(0));
  REQUIRE( heart_T1.number_of_connections() == 1 );
  std::cout << "Terminating T1" << std::endl;
  heart_T1.terminate_device();
}

TEST_CASE( "Instantiate 3 hearts in sequence", "[Skynet_Heart]" )
{
  const uint16_t SKYNET_PORT_T1 = 6000;
  const uint16_t SKYNET_PORT_T2 = 7000;
  const uint16_t SKYNET_PORT_T3 = 8000;
  std::ofstream outfile;
  // This heart starts first and has nobody to connect to
  outfile.open("test_config_T1.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_T1 << std::endl;
  outfile << "number_of_devices\t0" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile.close();
  skynet::KeyValueReader config_T1("test_config_T1.txt", "\t");
  std::cout << "Starting heartbeat on T1" << std::endl;
  skynet::Heart heart_T1(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(config_T1)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T1.run_heartbeat();

  // This heart starts second and connects to T1
  outfile.open("test_config_T2.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_T2 << std::endl;
  outfile << "number_of_devices\t1" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile << "device1_ip_address\t127.0.0.1" << std::endl;
  outfile << "device1_port\t" << SKYNET_PORT_T1 << std::endl;
  outfile.close();
  skynet::KeyValueReader config_T2("test_config_T2.txt", "\t");
  std::cout << "Starting heartbeat on T2" << std::endl;
  skynet::Heart heart_T2(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(config_T2)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T2.run_heartbeat();
  std::this_thread::sleep_for (std::chrono::seconds(1));
  REQUIRE( heart_T1.number_of_connections() == 1 );
  REQUIRE( heart_T2.number_of_connections() == 1 );

  // This heart starts third and connects to T1 and T2
  outfile.open("test_config_T3.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_T3 << std::endl;
  outfile << "number_of_devices\t2" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile << "device1_ip_address\t127.0.0.1" << std::endl;
  outfile << "device1_port\t" << SKYNET_PORT_T1 << std::endl;
  outfile << "device2_ip_address\t127.0.0.1" << std::endl;
  outfile << "device2_port\t" << SKYNET_PORT_T2 << std::endl;
  outfile.close();
  skynet::KeyValueReader config_T3("test_config_T3.txt", "\t");
  std::cout << "Starting heartbeat on T3" << std::endl;
  skynet::Heart heart_T3(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(config_T3)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T3.run_heartbeat();
  std::this_thread::sleep_for (std::chrono::seconds(1));
  REQUIRE( heart_T1.number_of_connections() == 2 );
  REQUIRE( heart_T2.number_of_connections() == 2 );
  REQUIRE( heart_T3.number_of_connections() == 2 );

  std::cout << "Terminating T1" << std::endl;
  heart_T1.terminate_device();
  std::cout << "Terminating T2" << std::endl;
  heart_T2.terminate_device();
  std::cout << "Terminating T3" << std::endl;
  heart_T3.terminate_device();
}
