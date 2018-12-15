#ifndef SKYNET_BEATINTERPRETER_HPP__
#define SKYNET_BEATINTERPRETER_HPP__


namespace skynet
{
  /** \class BeatInterpreter
   *  \brief Abstract class for deciding what to do with history
   *	of heartbeats (e.g. if neighboring devices should be
   *	pronounced dead)
   */
   class BeatInterpreter
   {
   public:

     /** \brief Determine the status of a device based on its
      * responses to the heartbeat
      *
      * \param device the device that we want to determine the status of
      * \param response_history History (probably a vector) of responses
      *	from the device that will be used to determine device status
      *
      * \return 1 if device is alive or at least we are not ready to
      *	pronounce it dead, 0 if device is determined to be dead
      */
     template<typename T>
     bool determine_device_status(const DeviceReference& device, const T& response_history);

   private:

   };// class BeatInterpreter


}// namespace skynet

#endif /* SKYNET_BEATINTERPRETER

