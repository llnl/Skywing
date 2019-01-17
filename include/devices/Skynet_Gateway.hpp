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

    /** \brief Collect new initial connections.
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices that have connected since the last time this method was called.
     */
    std::vector<std::unique_ptr<CommunicatorFactory>> collect_new_connections()
    {
      std::vector<std::unique_ptr<CommunicatorFactory>> new_factories;
      while (client_requesting_connection())
      {
        // create new CommunicatorFactory connected to client and add it to the
        // new_factories list
        new_factories.push_back(create_new_factory());
      }
      return new_factories;
    }


  private:

    /** \brief Determine if there is a connection request
     *
     * \return whether there is a connection request
     */
    virtual bool client_requesting_connection() = 0;

    /** \brief Create new CommunicatorFactory using a provided DeviceCommunicator
     *
     * \return a unique_ptr to a new CommunicatorFactory
     */
    virtual std::unique_ptr<CommunicatorFactory> create_new_factory() = 0;

  }; // class Gateway
} // namespace skynet

#endif /* SKYNET_GATEWAY_HPP__ */
