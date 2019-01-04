#ifndef SKYNET_GATEKEEPER_HPP__
#define SKYNET_GATEKEEPER_HPP__

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
     * \return a vector of DeviceReference objects that correspond to Devices
     * that have connected since the last time this method was called.
     */
    virtual const std::vector<DeviceReference> collect_new_connections() = 0;

  protected:
    std::unique_ptr<DeviceListener> gatekeeper_;

  }; // class Gatekeeper
} // namespace skynet

#endif /* SKYNET_GATEKEEPER_HPP__ */
