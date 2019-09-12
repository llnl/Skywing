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
    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The tag the data was sent with
     * \param data The raw data sent over
     * \return True if processing went find, false if there was an error
     */
    bool process_data(const std::size_t tag, const std::vector<char>& data)
    {
      return do_process_data(tag, data);
    }

    virtual ~JobBase() = default;

  protected:
    /** \brief Returns a handle to the associated master
     */
    Master& get_master() noexcept { return *master_; }
    const Master& get_master() const noexcept { return *master_; }

  private:
    /** \brief Implementation of process_data
     */
    virtual bool do_process_data(std::size_t tag, const std::vector<char>& data) = 0;

    // The handle to the associated master
    // Pointer instead of a reference to deal with circular dependencies
    Master* master_{nullptr};
  }; // Class JobBase
} // namespace skynet

#endif // SKYNET_JOB_BASE_HPP
