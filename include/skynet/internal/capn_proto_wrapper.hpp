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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace skynet::internal
{
  namespace detail
  {
    // For recursing below, I feel like there's a better way of doing this, but I can't think of it.
    template<typename T> struct IsVector : std::false_type {};
    template<typename T> struct IsVector<std::vector<T>> : std::true_type {};

    // Changing from Cap'n Proto's list of things to a vector of things is a common
    // operation; provide a function to do it
    template<typename To, typename From>
    std::vector<To> list_to_vector(const From& values) noexcept
    {
      std::vector<To> to_ret;
      to_ret.reserve(values.size());
      for (std::size_t i = 0; i < values.size(); ++i)
      {
        if constexpr (IsVector<To>::value)
        {
          to_ret.push_back(list_to_vector<typename To::value_type>(values[i]));
        }
        else
        {
          to_ret.push_back(values[i]);
        }
      }
      return to_ret;
    }

    // Mapping for the publish data to retrieve things from it as a template
    template<typename T> struct PublishValueHandler;

    // Create a mapping for a type and a vector of that type
    #define SKYNET_MAKE_PUBLISH_VALUE_HANDLER(cpp_type, capn_suffix) \
      template<> struct PublishValueHandler<cpp_type> \
      { \
        static std::optional<cpp_type> get(const cpnpro::PublishData::Value::Reader& r) noexcept \
        { \
          if (!r.is##capn_suffix()) { return {}; } \
          return r.get##capn_suffix(); \
        } \
        static void set(cpnpro::PublishData::Value::Builder& b, const cpp_type& value) noexcept \
        { \
          b.set##capn_suffix(value); \
        } \
      }; \
      template<> struct PublishValueHandler<std::vector<cpp_type>> \
      { \
        static std::optional<std::vector<cpp_type>> get(const cpnpro::PublishData::Value::Reader& r) noexcept \
        { \
          if (!r.isR##capn_suffix()) { return {}; } \
          return list_to_vector<cpp_type>(r.getR##capn_suffix()); \
        } \
        static void set(cpnpro::PublishData::Value::Builder& b, const std::vector<cpp_type>& values) noexcept \
        { \
          auto serialized_data = b.initR##capn_suffix(values.size()); \
          for (std::size_t i = 0; i < values.size(); ++i) \
          { \
            serialized_data.set(i, values[i]); \
          } \
        } \
      }

    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(double, D);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(float, F);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int8_t, I8);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int16_t, I16);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int32_t, I32);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int64_t, I64);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint8_t, U8);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint16_t, U16);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint32_t, U32);
    SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint64_t, U64);

    #undef SKYNET_MAKE_PUBLISH_VALUE_HANDLER

    // String is a little bit different
    template<> struct PublishValueHandler<std::string>
    {
      static std::optional<std::string> get(const cpnpro::PublishData::Value::Reader& r) noexcept
      {
        if (!r.isStr()) { return {}; }
        return r.getStr();
      }
      static void set(cpnpro::PublishData::Value::Builder& b, const std::string& value) noexcept
      {
        b.setStr(value);
      }
    };

    template<> struct PublishValueHandler<std::vector<std::string>>
    {
      static std::optional<std::vector<std::string>> get(const cpnpro::PublishData::Value::Reader& r) noexcept
      {
        if (!r.isRStr()) { return {}; }
        return list_to_vector<std::string>(r.getRStr());
      }

      static void set(cpnpro::PublishData::Value::Builder& b, const std::vector<std::string>& values) noexcept
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
    using pvh = PublishValueHandler<T>;
    template<typename T>
    using pvh_v = PublishValueHandler<std::vector<T>>;
  } // namespace detail

  /** \brief Class representing values that can be published
   */
  class PublishValue
  {
  public:
    /** \brief Return a T if it was published
     */
    template<typename T>
    std::optional<T> get() const noexcept { return detail::PublishValueHandler<T>::get(r); }

    /** \brief Return the held value as a variant
     */
    std::optional<PublishValueVariant> get_variant() const noexcept
    {
      using namespace detail;
      // This is gross and I hate it, but...
      using vals = cpnpro::PublishData::Value::Which;
      switch (r.which())
      {
      case vals::D:     return pvh<double>::get(r);
      case vals::R_D:   return pvh_v<double>::get(r);
      case vals::F:     return pvh<float>::get(r);
      case vals::R_F:   return pvh_v<float>::get(r);
      case vals::STR:   return pvh<std::string>::get(r);
      case vals::R_STR: return pvh_v<std::string>::get(r);
      case vals::I8:    return pvh<std::int8_t>::get(r);
      case vals::I16:   return pvh<std::int16_t>::get(r);
      case vals::I32:   return pvh<std::int32_t>::get(r);
      case vals::I64:   return pvh<std::int64_t>::get(r);
      case vals::U8:    return pvh<std::uint8_t>::get(r);
      case vals::U16:   return pvh<std::uint16_t>::get(r);
      case vals::U32:   return pvh<std::uint32_t>::get(r);
      case vals::U64:   return pvh<std::uint64_t>::get(r);
      case vals::R_I8:  return pvh_v<std::int8_t>::get(r);
      case vals::R_I16: return pvh_v<std::int16_t>::get(r);
      case vals::R_I32: return pvh_v<std::int32_t>::get(r);
      case vals::R_I64: return pvh_v<std::int64_t>::get(r);
      case vals::R_U8:  return pvh_v<std::uint8_t>::get(r);
      case vals::R_U16: return pvh_v<std::uint16_t>::get(r);
      case vals::R_U32: return pvh_v<std::uint32_t>::get(r);
      case vals::R_U64: return pvh_v<std::uint64_t>::get(r);
      }
      return {};
    }

  private:
    // Only allow PublishData to construct this
    friend class PublishData;
    explicit PublishValue(cpnpro::PublishData::Value::Reader reader) noexcept
      : r{std::move(reader)}
      {}

    cpnpro::PublishData::Value::Reader r;
  };

  /** \brief Class representing a publish message
   */
  class PublishData
  {
  public:
    VersionID version() const noexcept { return r.getVersion(); }
    TagID tag_id() const noexcept { return r.getTagID(); }
    PublishValue value() const noexcept { return PublishValue{r.getValue()}; }

  private:
    cpnpro::PublishData::Reader r;

    friend class PublishMessageHandler;
    explicit PublishData(cpnpro::PublishData::Reader reader) noexcept
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
    std::uint16_t base_port() const noexcept { return r.getBasePort(); }

  private:
    cpnpro::Greeting::Reader r;

    friend class StatusMessageHandler;
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

    friend class StatusMessageHandler;
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

    friend class StatusMessageHandler;
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

  /** \brief Class representing information on which machines produce what tags
   */
  class ReportPublishers
  {
  public:
    std::vector<TagID> tags() const noexcept { return detail::list_to_vector<TagID>(r.getTags()); }
    std::vector<std::vector<std::string>> addresses() const noexcept
    {
      return detail::list_to_vector<std::vector<std::string>>(r.getAddresses());
    }
    std::vector<TagID> locally_produced_tags() const noexcept
    {
      return detail::list_to_vector<TagID>(r.getLocallyProducedTags());
    }

  private:
    cpnpro::ReportPublishers::Reader r;

    friend class StatusMessageHandler;
    explicit ReportPublishers(cpnpro::ReportPublishers::Reader reader) noexcept
      : r{std::move(reader)}
      {}
  };

  /** \brief Request information for which machines produce which tags
   */
  class GetPublishers
  {
  public:
    std::vector<std::string> tags() const noexcept { return detail::list_to_vector<TagID>(r.getTags()); }

  private:
    cpnpro::GetPublishers::Reader r;

    friend class StatusMessageHandler;
    explicit GetPublishers(cpnpro::GetPublishers::Reader reader) noexcept
      : r{std::move(reader)}
      {}
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
  class StatusMessageHandler
  {
  public:
    /** \brief Construct a message handler from a raw set of bytes
     */
    static std::optional<StatusMessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept
    {
      ExceptionSuppressor suppressor;
      // Read the message from the passed bytes
      StatusMessageHandler to_ret;
      kj::Array<const kj::byte> buffer{
        reinterpret_cast<const kj::byte*>(data.data()),
        data.size(),
        to_ret.impl_->null_disposer
      };
      kj::ArrayInputStream in_s{buffer};
      capnp::readMessageCopy(in_s, to_ret.impl_->message);
      to_ret.impl_->root = to_ret.impl_->message.getRoot<cpnpro::StatusMessage>();
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

  private:
    // The types of messages that can be produced
    using MessageVariant = std::variant<
      Greeting,
      Goodbye,
      NewNeighbor,
      RemoveNeighbor,
      Heartbeat,
      ReportPublishers,
      GetPublishers
    >;

    // Process the stored message and return its internal type
    std::optional<MessageVariant> extract_message() const noexcept
    {
      using vals = cpnpro::StatusMessage::Which;
      ExceptionSuppressor suppressor;
      // This is kind of messy, but need to make sure that there's a way
      // to signify that there's no data due to, e.g., malformed input
      const std::optional<MessageVariant> to_ret = [&]() -> std::optional<MessageVariant> {
        switch(impl_->root.which()) {
        case vals::GREETING:          return Greeting{impl_->root.getGreeting()};
        case vals::GOODBYE:           return Goodbye{/* impl_->root.getGoodbye() */};
        case vals::NEW_NEIGHBOR:      return NewNeighbor{impl_->root.getNewNeighbor()};
        case vals::REMOVE_NEIGHBOR:   return RemoveNeighbor{impl_->root.getRemoveNeighbor()};
        case vals::HEARTBEAT:         return Heartbeat{/* impl_->root.getHeartbeat() */};
        case vals::REPORT_PUBLISHERS: return ReportPublishers{impl_->root.getReportPublishers()};
        case vals::GET_PUBLISHERS:    return GetPublishers{impl_->root.getGetPublishers()};
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
      cpnpro::StatusMessage::Reader root;
    };
    std::unique_ptr<Impl> impl_ = std::make_unique<Impl>();
  };

  /** Class for converting raw bytes of a publish message into a usable format
   */
  class PublishMessageHandler
  {
  public:
    /** \brief Construct a message handler from a raw set of bytes
     */
    static std::optional<PublishMessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept
    {
      ExceptionSuppressor suppressor;
      // Read the message from the passed bytes
      PublishMessageHandler to_ret;
      kj::Array<const kj::byte> buffer{
        reinterpret_cast<const kj::byte*>(data.data()),
        data.size(),
        to_ret.impl_->null_disposer
      };
      kj::ArrayInputStream in_s{buffer};
      capnp::readMessageCopy(in_s, to_ret.impl_->message);
      to_ret.impl_->root = to_ret.impl_->message.getRoot<cpnpro::Publish>();
      if (suppressor.failed())
      {
        return {};
      }
      else
      {
        return std::move(to_ret);
      }
    }

    /** \brief Gets the published data, or if it was a shutdown message, no data
     */
    std::optional<PublishData> data() const noexcept
    {
      ExceptionSuppressor suppressor;
      if (!impl_->root.isData()) { return {}; }
      auto to_ret = PublishData{impl_->root.getData()};
      if (suppressor.failed())
      {
        return {};
      }
      else
      {
        return std::move(to_ret);
      }
    }

  private:
    // PIMPL to allow moving
    struct Impl {
      kj::NullArrayDisposer null_disposer;
      capnp::MallocMessageBuilder message;
      cpnpro::Publish::Reader root;
    };
    std::unique_ptr<Impl> impl_ = std::make_unique<Impl>();
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
