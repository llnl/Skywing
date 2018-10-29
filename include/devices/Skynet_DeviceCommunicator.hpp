#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

#include <cstddef>

namespace skynet
{
  /** \class DeviceCommunicator
   *  \brief Abstract class, policy for communicating with a Device.
   *
   * This is an abstract class that provides a device-to-device
   * communications interface.
   */
  class DeviceCommunicator
  {
  public:

    /** \brief Send data to the associated Device. 
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \param tag A tag associated with the data.
     */
    virtual void send_to(void* data, std::size_t data_size, int tag) const = 0;

    /** \brief Receive data from the associated Device.
     *
     * \param tag A tag associated with the expected data.
     *
     * \return A pair providing the data received and the size of
     * the data received.
     */
    virtual std::pair<void*, std::size_t> receive_from(int tag) const = 0;
    
  }; // class DeviceCommunicator

} // namespace skynet


#endif /*  SKYNET_DEVICECOMMUNICATOR_HPP__ */
