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

     /** \brief Send a signal to a device from the nearby
     *   device list and receive response.
     */
     template<typename T>
     T send_heartbeat(const Device& device);

     /** \brief Determine the status of a device based on its
      * responses to the heartbeat
      *
      * \param response_history History (probably a vector) of responses
      *	from the device that will be used to determine device status
      *
      * \return 1 if device is alive or at least we are not ready to
      *	pronounce it dead, 0 if device is determined to be dead
      */
     template<typename T>
     virtual bool determine_device_status(const T& response_history);


     //
     
     //      run pulse: send heartbeat
      

   private:

   };// class Heartbeat


}// namespace skynet

#endif /* SKYNET_HEARTBEAT_HPP__ */

