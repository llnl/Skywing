#ifndef SKYNET_INITIALIZEHEART_HPP__
#define SKYNET_INITIALIZEHEART_HPP__

#include <vector>

#include "Heart.hpp"

namespace skynet
{
    /** \class InitializeHeart
     *  \brief Abract class to define the heart settings and
     *	begin the hearbeat when a new device comes online.
     */
    class InitializeHeart
    {
    public:
      
       /** \brief Read configuration file and use it to create a Heart object
       *
       *  \param config configuration file used to create heart object
       */
      template<typename T>
      const Heart& void create_heart(const T& config);
      
    private:

    }; // class InitializeHeart

} // namespace skynet

#endif /* SKYNET_INITIALIZEHEART_HPP__ */
