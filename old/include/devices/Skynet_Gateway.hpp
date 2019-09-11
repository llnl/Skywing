#ifndef SKYNET_GATEWAY_HPP__
#define SKYNET_GATEWAY_HPP__

#include "Skynet_CommunicatorFactory.hpp"

namespace skynet
{

  /** \class Gateway
   * \brief Abstract class to listen for and respond to connection requests.
   *
   */
  class Gateway
  {
  public:

    /** \brief Create a Communicator Factory if one is pending.
     *
     * \return A Communicator Factory if one was requested, nullptr otherwise
     */
    std::unique_ptr<CommunicatorFactory> collect_new_connection() const
    {
      if (client_requesting_connection())
      {
        return create_new_factory();
      }
      return nullptr;
    }

    /** \brief Create CommunicatorFactory for each device in configuration file.
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices listed in configuration file.
     */
    virtual std::vector<std::unique_ptr<CommunicatorFactory>>
      create_initial_connections() const = 0;

    virtual ~Gateway() = default;

  private:

    /** \brief Determine if there is a connection request
     *
     * \return whether there is a connection request
     */
    virtual bool client_requesting_connection() const = 0;

    /** \brief Create new CommunicatorFactory using a provided DeviceCommunicator
     *
     * \return a unique_ptr to a new CommunicatorFactory
     */
    virtual std::unique_ptr<CommunicatorFactory> create_new_factory() const = 0;

  }; // class Gateway
} // namespace skynet

#endif /* SKYNET_GATEWAY_HPP__ */
