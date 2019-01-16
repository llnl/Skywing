#ifndef SKYNET_GATEKEEPER_HPP__
#define SKYNET_GATEKEEPER_HPP__

#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_DeviceListener.hpp"

namespace skynet
{

  /** \class Gatekeeper
   * \brief Abstract class, Gatekeeper to listen for new initial connections.
   *
   */
  class Gatekeeper
  {
  public:

    /** \brief Collect new initial connections.
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices that have connected since the last time this method was called.
     */
    virtual std::vector<std::unique_ptr<CommunicatorFactory>>
      collect_new_connections() = 0;

  protected:
    std::unique_ptr<DeviceListener> gatekeeper_;

  }; // class Gatekeeper
} // namespace skynet

#endif /* SKYNET_GATEKEEPER_HPP__ */
