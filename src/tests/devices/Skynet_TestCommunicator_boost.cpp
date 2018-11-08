#define BOOST_TEST_MODULE Skynet_TestCommunicator Test
#include <boost/test/included/unit_test.hpp>

#include "Skynet_TestCommunicator.hpp"

BOOST_AUTO_TEST_CASE( test )
{
  skynet::TestCommunicator communicator("192.0.0.1");
  std::pair<void*, std::size_t> message;
  message = communicator.receive_from(1);
  double* a = (double*) message.first;
  double b = *a;
  BOOST_CHECK( abs(b  - 100.0) < 1e-16);
}
