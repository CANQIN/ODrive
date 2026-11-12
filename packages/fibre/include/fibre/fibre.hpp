#ifndef __FIBRE_HPP
#define __FIBRE_HPP

#include <fibre/callback.hpp>
#include <fibre/bufptr.hpp>
#include <fibre/cpp_utils.hpp>
#include <fibre/event_loop.hpp>
#include <fibre/channel_discoverer.hpp>
#include <string>
#include <memory>

namespace fibre {

struct CallBuffers {
    Status status;
    cbufptr_t tx_buf;
    bufptr_t rx_buf;
};

struct CallBufferRelease {
    Status status;
    const uint8_t* tx_end;
    uint8_t* rx_end;
};

struct Function {
    virtual std::optional<CallBufferRelease>
    call(void**, CallBuffers, Callback<std::optional<CallBuffers>, CallBufferRelease>) = 0;
};

struct Object;
struct Interface;
class Domain;

struct Context {
    size_t n_domains = 0;
    EventLoop* event_loop;

#if FIBRE_ALLOW_HEAP
    std::unordered_map<std::string, ChannelDiscoverer*> discoverers;
#endif

    /**
     * @brief Creates a domain on which objects can subsequently be published
     * and discovered.
     * 
     * This potentially starts looking for channels on this domain.
     */
    Domain* create_domain(std::string specs);
    void close_domain(Domain* domain);

    void register_backend(std::string name, ChannelDiscoverer* backend);
    void deregister_backend(std::string name);
};

// TODO: don't declare these types here
struct LegacyProtocolPacketBased;
class LegacyObjectClient;
struct LegacyObject;

class Domain {
    friend struct Context;
public:
#if FIBRE_ENABLE_CLIENT
    // TODO: add interface argument
    // TODO: support multiple discovery instances
    void start_discovery(Callback<void, Object*, Interface*> on_found_object, Callback<void, Object*> on_lost_object);
    void stop_discovery();
#endif

    void add_channels(ChannelDiscoveryResult result);

    Context* ctx;
private:
#if FIBRE_ENABLE_CLIENT
    void on_found_root_object(LegacyObjectClient* obj_client, std::shared_ptr<LegacyObject> obj);
    void on_lost_root_object(LegacyObjectClient* obj_client, std::shared_ptr<LegacyObject> obj);
#endif
    void on_stopped(LegacyProtocolPacketBased* protocol, StreamStatus status);

#if FIBRE_ALLOW_HEAP
    std::unordered_map<std::string, fibre::ChannelDiscoveryContext*> channel_discovery_handles;
#endif
#if FIBRE_ENABLE_CLIENT
    Callback<void, Object*, Interface*> on_found_object_;
    Callback<void, Object*> on_lost_object_;
    std::unordered_map<Object*, Interface*> root_objects_;
#endif
};

/**
 * @brief Opens and initializes a Fibre context.
 * 
 * If FIBRE_ALLOW_HEAP=0 only one Fibre context can be open at a time.
 * 
 * @returns: A non-null pointer on success, null otherwise.
 */
Context* open(EventLoop* event_loop);

void close(Context*);


}

#endif // __FIBRE_HPP