#include <arpa/inet.h>
#include "catch2/catch.hpp"
#include "devices/Skynet_Socket.hpp"
#include "devices/Skynet_SocketGateway.hpp"
#include "heartbeat/Skynet_Heart.hpp"
#include "heartbeat/Skynet_TrivialBeatInterpreter.hpp"
#include "heartbeat/Skynet_TrivialBeatSender.hpp"
#include "heartbeat/Skynet_TrivialDeviceManager.hpp"
#include "heartbeat/Skynet_TrivialPropertyChecker.hpp"
#include "heartbeat/Skynet_TrivialPulseTimer.hpp"

TEST_CASE( "Heart Instantiation", "[Skynet_Heart]" )
{
  const uint16_t SKYNET_PORT = 5000;

  skynet::Heart heart_T1(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(AF_INET, SKYNET_PORT) //TODO: figure out why skynet::Socket::IPv4 doesnt work instead of AF_INET
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  std::cout << "It's ALIVE!!" << std::endl;

  heart_T1.run_heartbeat();
}
