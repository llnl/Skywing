#ifndef SKYNET_DETAIL_MESSAGE_HPP
#define SKYNET_DETAIL_MESSAGE_HPP

#include "skynet/types.hpp"
#include "skynet/detail/utility/serialize.hpp"
#include "skynet/detail/utility/launder.hpp"

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

namespace skynet { namespace detail
{
  /** \brief The kinds of messages that can be sent
   */
  enum class MessageType
  {
    // Set-up messages
    goodbye,
    greeting,

    // Broadcast messages
    global_broadcast,
    local_broadcast
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
    JobID job_id;
    /// The tag that the message was sent with
    TagID tag_id;
    /// The id of the computer that the message is originally from
    MachineID origin;
    /// The id of the message
    MessageID message_id;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct Message

  #if SKYNET_GENERATED_MESSAGE_NETWORK_SIZE > 0
    // TODO: This is desirable, but due to padding this static_assert can
    //       produce false positives
    // Shouldn't be possible to have a serialized size smaller than the object
    // size; don't want any fields that aren't serialized
    // static_assert(
    //   Message::network_size >= sizeof(Message),
    //   "All data in skynet::detail::Message is not being serialized!"
    // );
  #endif // SKYNET_GENERATED_MESSAGE_NETWORK_SIZE

  // Message should always be trivially copyable
  static_assert(
    std::is_trivially_copyable<Message>::value,
    "skynet::detail::Message is not trivally copyable!\n"
    "Remove any complex types from the structure."
  );

  /** \brief Serialize a message
   */
  template <class Archive>
  void serialize(Archive& ar, Message& m) noexcept
  {
    ar(m.type, m.job_id, m.tag_id, m.origin, m.message_id, m.message_size);
  }

  /** \brief Simple class for holding and processing the raw bytes of a
   * recieved message and its data
   */
  class MessageAndDataBuffer
  {
  public:
    /** \brief Construct a buffer, marking which connection it came from.
     */
    explicit MessageAndDataBuffer(const MachineID from)
      : from_{from}
      , buffer_(Message::network_size)
    {}

    /** \brief Return a pointer to the start of the buffer, for putting the
     * serialized Message into
     */
    char* buffer() noexcept { return buffer_.data(); }

    /** \brief Return a deserialized message and adjust the buffer for reading
     * based on the content of the message
     *
     * \pre A serialized message has been written into the buffer
     */
    Message message()
    {
      const auto msg = from_bytes<Message>(buffer_);
      buffer_.resize(Message::network_size + msg.message_size);
      return msg;
    }

    /** \brief Return a pointer to the data part of the buffer
     */
    char* data() noexcept { return buffer_.data() + Message::network_size; }
    const char* data() const noexcept { return buffer_.data() + Message::network_size; }

    /** \brief Return a reference to the entire buffer
     */
    const std::vector<char>& vector() const noexcept { return buffer_; }

    /** \brief Return the id of the connection this message was sent from
     */
    MachineID from() const noexcept { return from_; }

  private:
    // Who sent the data
    MachineID from_;
    // Buffer to hold who the data is from
    std::vector<char> buffer_;
  };
} } // namespace skynet::detail

// Don't allow the macro to leak
#undef SKYNET_GENERATED_MESSAGE_NETWORK_SIZE

#endif // SKYNET_DETAIL_MESSAGE_HPP
