#define BOOST_TEST_MODULE Skynet_TestCommunicator Test
#include <boost/test/included/unit_test.hpp>

#include "Skynet_TestCommunicator.hpp"

BOOST_AUTO_TEST_CASE( test_communication_methods )
{
    skynet::TestCommunicator communicator("192.0.0.1");
    std::pair<void*, std::size_t> message;
    message = communicator.receive_from(1);
    double* a = (double*) message.first;
    double b = *a;
    BOOST_CHECK( abs(b  - 100.0) < 1e-16);
}

BOOST_AUTO_TEST_CASE( test_get_set_methods )
{
    skynet::TestCommunicator communicator("192.0.0.1");
    std::string ip_address = communicator.get_ip_address();
    BOOST_CHECK(ip_address.compare("192.0.0.1") == 0);
    communicator.set_ip_address("192.0.0.2");
    ip_address = communicator.get_ip_address();
    BOOST_CHECK(ip_address.compare("192.0.0.2") == 0);
}
