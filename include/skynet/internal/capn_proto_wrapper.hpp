#ifndef SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
#define SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP

// This header exists to allow more convienent and (within the codebase)
// conventional access to the Cap'n Proto messages

#include "message_format.capnp.h"

#include <capnp/serialize.h>

#include "skynet/internal/utility/overload_set.hpp"
#include "skynet/types.hpp"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace skynet::internal
{
  namespace detail
  {
    // Changing from Cap'n Proto's list of things to a vector of things is a common
    // operation; provide a function to do it
    template<typename To, typename From>
    std::vector<To> list_to_vector(const From& values) noexcept
    {
      std::vector<To> to_ret;
      to_ret.reserve(values.size());
      for (const auto& val : values)
      {
        to_ret.push_back(val);
      }
      return to_ret;
    }

    // Mapping for the publish data to retrieve things from it as a template
    template<typename T> struct PublishDataHandler;

    // Create a mapping for a type and a vector of that type
    #define SKYNET_MAKE_PUBLISH_DATA_HANDLER(cpp_type, capn_suffix) \
      template<> struct PublishDataHandler<cpp_type> \
      { \
        static std::optional<cpp_type> get(const cpnpro::PublishData::Reader& r) noexcept \
        { \
          if (!r.is##capn_suffix()) { return {}; } \
          return r.get##capn_suffix(); \
        } \
        static void set(cpnpro::PublishData::Builder& b, const cpp_type& value) noexcept \
        { \
          b.set##capn_suffix(value); \
        } \
      }; \
      template<> struct PublishDataHandler<std::vector<cpp_type>> \
      { \
        static std::optional<std::vector<cpp_type>> get(const cpnpro::PublishData::Reader& r) noexcept \
        { \
          if (!r.isR##capn_suffix()) { return {}; } \
          return list_to_vector<cpp_type>(r.getR##capn_suffix()); \
        } \
        static void set(cpnpro::PublishData::Builder& b, const std::vector<cpp_type>& values) noexcept \
        { \
          auto serialized_data = b.initR##capn_suffix(values.size()); \
          for (std::size_t i = 0; i < values.size(); ++i) \
          { \
            serialized_data.set(i, values[i]); \
          } \
        } \
      }

    SKYNET_MAKE_PUBLISH_DATA_HANDLER(double, D);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(float, F);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::int8_t, I8);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::int16_t, I16);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::int32_t, I32);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::int64_t, I64);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::uint8_t, U8);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::uint16_t, U16);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::uint32_t, U32);
    SKYNET_MAKE_PUBLISH_DATA_HANDLER(std::uint64_t, U64);

    #undef SKYNET_MAKE_PUBLISH_DATA_HANDLER

    // String is a little bit different
    template<> struct PublishDataHandler<std::string>
    {
      static std::optional<std::string> get(const cpnpro::PublishData::Reader& r) noexcept
      {
        if (!r.isStr()) { return {}; }
        return r.getStr();
      }
      static void set(cpnpro::PublishData::Builder& b, const std::string& value) noexcept
      {
        b.setStr(value);
      }
    };

    template<> struct PublishDataHandler<std::vector<std::string>>
    {
      static std::optional<std::vector<std::string>> get(const cpnpro::PublishData::Reader& r) noexcept
      {
        if (!r.isRStr()) { return {}; }
        return list_to_vector<std::string>(r.getRStr());
      }

      static void set(cpnpro::PublishData::Builder& b, const std::vector<std::string>& values) noexcept
      {
        auto serialized_data = b.initRStr(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
        {
          serialized_data.set(i, values[i]);
        }
      }
    };

    // Short name entirely for the one function below
    // (type alias templates can't be local)
    template<typename T>
    using pdh = PublishDataHandler<T>;
    template<typename T>
    using pdh_v = PublishDataHandler<std::vector<T>>;
  } // namespace detail

  // The categories each message type can be
  enum class MessageCategory
  {
    status,
    job
  };

  /** \brief Class representing the data that can be published
   */
  class PublishData
  {
  public:
    /** \brief Return a T if it was published
     */
    template<typename T>
    std::optional<T> get() const noexcept { return detail::PublishDataHandler<T>::get(r); }

    /** \brief Return the held value as a variant
     */
    std::optional<PublishDataVariant> get_variant() const noexcept
    {
      using namespace detail;
      // This is gross and I hate it, but...
      using vals = cpnpro::PublishData::Which;
      switch (r.which())
      {
      case vals::D:     return pdh<double>::get(r);
      case vals::R_D:   return pdh_v<double>::get(r);
      case vals::F:     return pdh<float>::get(r);
      case vals::R_F:   return pdh_v<float>::get(r);
      case vals::STR:   return pdh<std::string>::get(r);
      case vals::R_STR: return pdh_v<std::string>::get(r);
      case vals::I8:    return pdh<std::int8_t>::get(r);
      case vals::I16:   return pdh<std::int16_t>::get(r);
      case vals::I32:   return pdh<std::int32_t>::get(r);
      case vals::I64:   return pdh<std::int64_t>::get(r);
      case vals::U8:    return pdh<std::uint8_t>::get(r);
      case vals::U16:   return pdh<std::uint16_t>::get(r);
      case vals::U32:   return pdh<std::uint32_t>::get(r);
      case vals::U64:   return pdh<std::uint64_t>::get(r);
      case vals::R_I8:  return pdh_v<std::int8_t>::get(r);
      case vals::R_I16: return pdh_v<std::int16_t>::get(r);
      case vals::R_I32: return pdh_v<std::int32_t>::get(r);
      case vals::R_I64: return pdh_v<std::int64_t>::get(r);
      case vals::R_U8:  return pdh_v<std::uint8_t>::get(r);
      case vals::R_U16: return pdh_v<std::uint16_t>::get(r);
      case vals::R_U32: return pdh_v<std::uint32_t>::get(r);
      case vals::R_U64: return pdh_v<std::uint64_t>::get(r);
      }
      return {};
    }

  private:
    // Only allow Publish to construct this
    friend class Publish;
    explicit PublishData(cpnpro::PublishData::Reader reader) noexcept
      : r{std::move(reader)}
      {}

    cpnpro::PublishData::Reader r;
  };

  /** \brief Class representing a publish message
   */
  class Publish
  {
  public:
    VersionID version() const noexcept { return r.getVersion(); }
    TagID tag_id() const noexcept { return r.getTagID(); }
    MachineID origin() const noexcept { return r.getOrigin(); }
    std::uint8_t hops_left_p1() const noexcept { return r.getHopsLeftP1(); }
    PublishData data() const noexcept { return PublishData{r.getData()}; }

  private:
    cpnpro::Publish::Reader r;

    friend class MessageHandler;
    explicit Publish(cpnpro::Publish::Reader reader) noexcept
      : r{std::move(reader)}
      {}
  };

  /** \brief Class representing a greeting message
   */
  class Greeting
  {
  public:
    MachineID from() const noexcept { return r.getFrom(); }
    std::vector<MachineID> neighbors() const noexcept { return detail::list_to_vector<MachineID>(r.getNeighbors()); }

  private:
    cpnpro::Greeting::Reader r;

    friend class MessageHandler;
    explicit Greeting(cpnpro::Greeting::Reader reader) noexcept
      : r{std::move(reader)}
      {}
  };

  /** \brief Class representing a goodbye message
   */
  class Goodbye
  {
    // Intentionally empty
  };

  /** \brief Class representing a new neighbor message
   */
  class NewNeighbor
  {
  public:
    MachineID neighbor_id() const noexcept { return r.getNeighborID(); }

  private:
    cpnpro::NewNeighbor::Reader r;

    friend class MessageHandler;
    explicit NewNeighbor(cpnpro::NewNeighbor::Reader reader) noexcept
      : r{std::move(reader)}
      {}
  };

  /** \brief Class representing a remove neighbor message
   */
  class RemoveNeighbor
  {
  public:
    MachineID neighbor_id() const noexcept { return r.getNeighborID(); }

  private:
    cpnpro::RemoveNeighbor::Reader r;

    friend class MessageHandler;
    explicit RemoveNeighbor(cpnpro::RemoveNeighbor::Reader reader) noexcept
      : r{std::move(reader)}
      {}
  };

  /** \brief Class representing a heartbeat
   */
  class Heartbeat
  {
    // Intentionally empty
  };

  /** \brief Class that supresses Cap'n Proto's exceptions so that they
   * can be used in a non-exception friendly environment
   *
   * Note that all that has to be done to supress exceptions is to create
   * one of these on the stack.
   */
  class ExceptionSuppressor : public kj::ExceptionCallback
  {
  public:
    bool failed() const noexcept { return failed_; }

  private:
    void onRecoverableException(kj::Exception&&) override
    {
      // Mark it as failed
      failed_ = true;
    }

    void onFatalException(kj::Exception&&) override
    {
      // just return - nothing can be done here
      return;
    }

    bool failed_ = false;
  };

  /** \brief Class for converting the raw bytes of a message into a useable format
   */
  class MessageHandler
  {
  public:
    /** \brief Construct a message handler from a raw set of bytes
     */
    static std::optional<MessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept
    {
      ExceptionSuppressor suppressor;
      // Read the message from the passed bytes
      MessageHandler to_ret;
      kj::Array<const kj::byte> buffer{
        reinterpret_cast<const kj::byte*>(data.data()),
        data.size(),
        to_ret.impl_->null_disposer
      };
      kj::ArrayInputStream in_s{buffer};
      capnp::readMessageCopy(in_s, to_ret.impl_->message);
      to_ret.impl_->root = to_ret.impl_->message.getRoot<cpnpro::Message>();
      if (suppressor.failed())
      {
        return {};
      }
      else
      {
        return std::move(to_ret);
      }
    }

    /** \brief Perform a callback on the stored message
     *
     * Returns true if the callback was successful, false otherwise
     */
    template<typename... Ts>
    bool do_callback(Ts&&... callbacks) const noexcept
    {
      if (const auto msg = extract_message())
      {
        return std::visit(make_overload_set(std::forward<Ts>(callbacks)...), *msg);
      }
      return false;
    }

    /** \brief Return the category that the message represents
     */
    MessageCategory category() const noexcept
    {
      using vals = cpnpro::Message::Which;
      switch(impl_->root.which()) {
      case vals::PUBLISH:         return MessageCategory::job;
      case vals::GREETING:        return MessageCategory::status;
      case vals::GOODBYE:         return MessageCategory::status;
      case vals::NEW_NEIGHBOR:    return MessageCategory::status;
      case vals::REMOVE_NEIGHBOR: return MessageCategory::status;
      case vals::HEARTBEAT:       return MessageCategory::status;
      }
      // this should never happen
      return MessageCategory::status;
    }

  private:
    // The types of messages that can be produced
    using MessageVariant = std::variant<
      Publish,
      Greeting,
      Goodbye,
      NewNeighbor,
      RemoveNeighbor,
      Heartbeat
    >;

    // Process the stored message and return its internal type
    std::optional<MessageVariant> extract_message() const noexcept
    {
      using vals = cpnpro::Message::Which;
      ExceptionSuppressor suppressor;
      // This is kind of messy, but need to make sure that there's a way
      // to signify that there's no data due to, e.g., malformed input
      const std::optional<MessageVariant> to_ret = [&]() -> std::optional<MessageVariant> {
        switch(impl_->root.which()) {
        case vals::PUBLISH:         return Publish{impl_->root.getPublish()};
        case vals::GREETING:        return Greeting{impl_->root.getGreeting()};
        case vals::GOODBYE:         return Goodbye{/* impl_->root.getGoodbye() */};
        case vals::NEW_NEIGHBOR:    return NewNeighbor{impl_->root.getNewNeighbor()};
        case vals::REMOVE_NEIGHBOR: return RemoveNeighbor{impl_->root.getRemoveNeighbor()};
        case vals::HEARTBEAT:       return Heartbeat{/* impl_->root.getHeartbeat() */};
        }
        return {};
      }();
      if (suppressor.failed())
      {
        return {};
      }
      else
      {
        return to_ret;
      }
    }

    // Message isn't copyable or movable... so have a unique_ptr to allow moving
    struct Impl {
      kj::NullArrayDisposer null_disposer;
      capnp::MallocMessageBuilder message;
      cpnpro::Message::Reader root;
    };
    std::unique_ptr<Impl> impl_ = std::make_unique<Impl>();
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
