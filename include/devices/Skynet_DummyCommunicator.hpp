#ifndef SKYNET_DUMMYCOMMUNICATOR_HPP__
#define SKYNET_DUMMYCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"

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

      void set_ip_address(std::string ip_address)
      {
          ip_address_ = ip_address;
      }

      std::string get_ip_address() const
      {
          return ip_address_;
      }

      private:
      void do_send_to_(void* data, std::size_t data_size) const override
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
    std::pair<void*, std::size_t> do_receive_from_() const override
      {

        int a = 1100; 
        std::pair<void*, std::size_t> message(&a,sizeof(int));
        return message; 
      }

    private:
	std::string ip_address_;
	int sock_;
}; // class DummyCommunicator

} // namespace skynet


#endif /* SKYNET_DUMMYCOMMUNICATOR_HPP__ */
