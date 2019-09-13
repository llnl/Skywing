#ifndef SKYNET_JOB_BASE_HPP
#define SKYNET_JOB_BASE_HPP

#include <vector>

namespace skynet
{
  class Master;
  /** \brief Job that Skynet instances are working on
   *
   * Handles wrapping communications and buffering information from tags
   */
  class JobBase
  {
  public:
    /** \brief Create a JobBase with the owning master
     */
    explicit JobBase(Master& master)
      : master_{master}
    {}

    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The tag the data was sent with
     * \param data The raw data sent over
     * \return True if processing went find, false if there was an error
     */
    bool process_data(
      const std::size_t tag,
      const char* const data,
      const std::size_t size
    )
    {
      return do_process_data(tag, data, size);
    }

    virtual ~JobBase() = default;

  protected:
    /** \brief Returns a handle to the associated master
     */
    Master& get_master() noexcept { return master_; }
    const Master& get_master() const noexcept { return master_; }

  private:
    /** \brief Implementation of process_data
     */
    virtual bool do_process_data(std::size_t tag, const char* data, std::size_t size) = 0;

    // The handle to the associated master
    Master& master_;
  }; // Class JobBase
} // namespace skynet

#endif // SKYNET_JOB_BASE_HPP
