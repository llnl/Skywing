#ifndef SKYNET_MPICOMMUNICATOR_HPP__
#define SKYNET_MPICOMMUNICATOR_HPP__

namespace skynet
{
  class MPICommunicator : public DeviceCommunicator
  {
  public:
    MPICommunicator(MPI_Comm comm)
      : comm_(comm)
    {}

  private:
    void do_send_to_(void* data, std::size_t data_size) const
    {
      MPI_Send(data, data_size, MPI_BYTE, other_rank_, 0, comm_);
    }

    std::vector<char> do_receive_from_() const
    {
      MPI_Status status;
      MPI_Probe(other_rank_, 0, comm_, &status);
      int data_size;
      MPI_Get_count(&status, MPI_BYTE, &data_size);
      
      std::vector<char> data(data_size);
      MPI_Status status;
      MPI_Recv(static_cast<void*>(&data[0]), data_size, MPI_BYTE, other_rank_,
	       0, comm_, &status);
      return data;
    }

    private:
    MPI_Comm comm_;
    int other_rank_;
  }; // class MPICommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */
