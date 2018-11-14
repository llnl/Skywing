#ifndef SKYNET_DUMMYCOMMUNICATOR_HPP__
#define SKYNET_DUMMYCOMMUNICATOR_HPP__

#include <Skynet_DeviceCommunicator.hpp>

#include <string>
#include <sys/socket.h>

namespace skynet
{
    /** \class TestCommunicator
     *  \brief A DeviceCommunicator for testing purposes.
     */
    class DummyCommunicator : public DeviceCommunicator
    {
    public:
	/** \brief Construct a new DummyCommunicator
	 *
	 *  \param ip_address The IP address of the Device.
	 */
	DummyCommunicator(std::string ip_address)
	    : ip_address_(ip_address)
	    {
		    sock_ = 0;
	    }
      /** \brief Send data to the associated Device.
       *
       * \param data Data to send.
       * \param data_size Number of bytes of data to send.
       * \param tag A tag associated with the data.
       */
      void send_to(void* data, std::size_t data_size, int tag) const
      {
          // do nothing
      }

      /** \brief Receive data from the associated Device.
       *
       * \param tag A tag associated with the expected data.
       *
       * \return A pair providing the data received and the size of
       * the data received.
       */
      std::pair<void*, std::size_t> receive_from(int tag) const
      {
          double a = 100.0;
          std::pair<void*, std::size_t> message(&a,100);
          return message;
      }

      void set_ip_address(std::string ip_address)
      {
          ip_address_ = ip_address;
      }

      std::string get_ip_address() const
      {
          return ip_address_;
      }

    private:
	std::string ip_address_;
	int sock_;
}; // class DummyCommunicator

} // namespace skynet


#endif /* SKYNET_DUMMYCOMMUNICATOR_HPP__ */
