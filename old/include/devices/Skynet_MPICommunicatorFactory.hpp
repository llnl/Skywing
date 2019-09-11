#ifndef SKYNET_MPICOMMUNICATORFACTORY_HPP__
#define SKYNET_MPICOMMUNICATORFACTORY_HPP__

#include "mpi.h"
#include <memory>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_MPICommunicator.hpp"

namespace skynet
{

  /** \class MPICommunicatorFactory
   * \brief Implements CommunicatorFactory for MPI-based communications.
   *
   * Note that an MPI Communicator and a Skynet Communicator are NOT
   * analogous.
   */
  class MPICommunicatorFactory : public CommunicatoryFactory
  {
  public:

    /** \brief Create a new MPICommunicatorFactory.
     *
     * \param other_rank The MPI rank of the device with which we'll
     * be communicating.
     */
    MPICommunicatorFactory(const int other_rank)
      : comm_(MPI_COMM_WORLD),
        tag_(0),
        other_rank_(other_rank)
    { }

    /** \brief Create a new MPICommunicatorFactory.
     *
     * \param comm The MPI Communicator used for this Skynet
     * Communicator.
     * \param other_rank The MPI rank of the device
     *  with which we'll be communicating.
     */
    MPICommunicatorFactory(const MPI_Comm comm, const int other_rank)
      : comm_(comm),
        tag_(0),
        other_rank_(other_rank)
    { }

    /** \brief Create a new DeviceCommunicator.
     *
     * From the CommunicatorFactory interface.
     *
     * \param comm_config_info Not used here.
     */
    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(const std::vector<std::string>& /*comm_config_info*/) override
    {

      return MPICommunicator(newcomm);
    }

  private:
    MPI_Comm comm_;
    int tag_;
    int other_rank_;

  }; // MPICommunicatorFactory
} // namespace skynet

#endif /* SKYNET_MPICOMMUNICATORFACTORY_HPP__ */
