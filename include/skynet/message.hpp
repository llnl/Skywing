#ifndef SKYNET_MESSAGE_HPP
#define SKYNET_MESSAGE_HPP

#include "utility/serialize.hpp"
#include "utility/launder.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

// To ensure that the proper message seralization size is always used,
// a generated header is made that contains the size, but it needs
// to be excluded if the header isn't being generated
// A macro is used so it can be undefined later
// TODO: Is there a better/easier way to do this?
#ifdef SKYNET_GENERATE_MESSAGE_SIZE_HEADER
  #define SKYNET_GENERATED_MESSAGE_NETWORK_SIZE 0
#else
  // I would like this to be in a subdirectory but the only way to do it with
  // Meson would involve having things in weird places
  #include "message_network_size.hpp"
#endif

namespace skynet
{
  /** \brief The kinds of messages that can be sent
   */
  enum class MessageType
  {
    greeting,
    broadcast
  }; // enum class MessageType

  /** \brief The message that is sent between two Skynet instances
   *
   * Note that this structure MUST NOT contain any variable sized data
   */
  struct Message
  {
    /// The size of the message when sent over the network
    static constexpr std::size_t network_size = SKYNET_GENERATED_MESSAGE_NETWORK_SIZE;

    /// The type of message that this is
    MessageType type;
    /// The job that this message is for
    std::uint32_t job_id;
    /// The tag that the message was sent with
    std::uint32_t tag_id;
    /// The id of the computer that the message is originally from
    std::uint32_t origin;
    /// The id of the message
    std::uint32_t message_id;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct Message

  // Message should always be trivially copyable
  static_assert(std::is_trivially_copyable<Message>::value,
    "skynet::Message is not trivally copyable!\n"
    "Remove any complex types from the structure."
  );

  /** \brief Serialize a message
   */
  template <class Archive>
  void serialize(Archive& ar, Message& m) noexcept
  {
    ar(m.type, m.job_id, m.tag_id, m.origin, m.message_id, m.message_size);
  }

  /** \brief Grouping of a message and its data together for easier propigation
   */
  class MessageAndData
  {
  public:
    /** \brief Initialize the structure with enough memory to hold the message
     * passed and the data
     *
     * Also includes a field for which connection this was recieved from,
     * since that doesn't need to be transmitted across the network
     */
    explicit MessageAndData(const Message& m, const std::uint32_t from)
      : data_(m.message_size + Message::network_size)
      , from_{from}
    {
      new (data_.data()) Message{m};
    }

    // Disable copying (probably would be fine though?)
    MessageAndData(const MessageAndData&) = delete;
    MessageAndData& operator=(const MessageAndData&) = delete;
    // Moving is fine
    MessageAndData(MessageAndData&&) = default;
    MessageAndData& operator=(MessageAndData&&) = default;

    /** \brief Returns a reference to the contained message
     */
    const Message& message() const noexcept
    {
      return *launder(reinterpret_cast<const Message*>(data_.data()));
    }

    /** \brief Returns a pointer to the start of the data
     */
    char* data() noexcept
    {
      return data_.data() + Message::network_size;
    }
    const char* data() const noexcept
    {
      return data_.data() + Message::network_size;
    }

    /** \brief Returns the id of the machine that this is from
     */
    std::uint32_t from() const noexcept { return from_; }

    /** \brief Returns a reference to the internal vector
     */
    const std::vector<char>& vector() const noexcept { return data_; }

  private:
    // Holding data (make sure it's aligned correctly)
    static constexpr std::size_t data_align = std::max(alignof(Message), alignof(std::vector<char>));
    alignas(data_align) std::vector<char> data_;

    // The id of the machine that this is from
    std::uint32_t from_;
  };

} // namespace skynet

// Don't allow the macro to leak
#undef SKYNET_GENERATED_MESSAGE_NETWORK_SIZE

#endif // SKYNET_MESSAGE_HPP
