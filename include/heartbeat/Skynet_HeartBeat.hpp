#ifndef SKYNET_HEARTBEAT_HPP__
#define SKYNET_HEARTBEAT_HPP__


namespace skynet
{
  /** \class Heartbeat
   *  \brief Abstract class for operting the heartbeat.
   *	Defines the sequence of events that occurs each
   *	time the heart "beats."
   */
   class HeartBeat
   {
   public:

     /** \brief Send a 

     	/** \brief Send a heartbeat pulse to a Device and measure the
	 *         response time.
	 */
	double send_heartbeat_pulse(const Device& device);

     	/** \brief Pronounce a Skynet Device no longer online.
	 *
	 * This gets called either because a Device informs others it
	 * is going offline, or because it has failed to respond to
	 * enough heartbeat pulses.
	 */
	void pronounce_device_terminated(Device& device);

    
   private:

   };// class Heartbeat


}// namespace skynet

#endif /* SKYNET_HEARTBEAT_HPP__ */

