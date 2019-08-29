#ifndef SKYNET_MPICOMMUNICATOR_HPP__
#define SKYNET_MPICOMMUNICATOR_HPP__

#include "mpi.h"
#include "Skynet_DeviceCommunicator.hpp"
#include <vector>

namespace skynet
{
  /** \class MPICommunicator
   * \brief Implements DeviceCommunicator for MPI-based communications.
   *
   * Note that an MPI Communicator and a Skynet Communicator are NOT
   * analogous.
   */
  class MPICommunicator : public DeviceCommunicator
  {
  public:
    /** \brief Construct a new MPICommunicator.
     *
     * \param comm The MPI Communicator used.
     * \param tag The tag used for this Skynet Communicator.
     * \param other_rank The rank of the device with which we'll communicate.
     */
    MPICommunicator(const MPI_Comm comm, const int tag, const int other_rank)
      : comm_(comm),
        tag_(tag),
        other_rank_(other_rank)
    {}

  private:
    /** \brief Send data to the associated Device.
     *
     * From the DeviceCommunicator interface.
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \param tag A tag associated with the data.
     */
    void do_send_to_(const void* const data, const std::size_t data_size)
    {
      MPI_Send(data, data_size, MPI_BYTE, other_rank_, tag_, comm_);
    }

    /** \brief Receive data from the associated Device.
     *
     * From the DeviceCommunicator interface.
     *
     * \param tag A tag associated with the expected data.
     *
     * \return A pair providing the data received and the size of
     * the data received.
     */
    std::vector<char> do_receive_from_()
    {
      MPI_Status status;
      MPI_Probe(other_rank_, tag_, comm_, &status);
      int data_size;
      MPI_Get_count(&status, MPI_BYTE, &data_size);

      std::vector<char> data(data_size);
      MPI_Status status;
      MPI_Recv(static_cast<void*>(&data[0]), data_size, MPI_BYTE, other_rank_,
	      tag_, comm_, &status);
      return data;
    }

    MPI_Comm comm_;
    int tag_;
    int other_rank_;

  }; // class MPICommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */
