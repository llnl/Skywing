#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <utility>
#include <vector>
#include <thread>

namespace skynet
{
  /** \class Temporary Heart Class
   *  \brief This is only to be a temporary container to get ns3 working
   *
   */
  class Heart
  {
  public:
    Heart()
    {}

    ~Heart()
    {}

    /** \brief Begin the heartbeat. */
    void begin_heartbeat()
    { std::cout << "I'm alive!!!\n"; }

  private:

  private:

  private:
  }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */
