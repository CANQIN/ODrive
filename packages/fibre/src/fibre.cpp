
#include <fibre/fibre.hpp>
#include <fibre/logging.hpp>
#include <fibre/channel_discoverer.hpp>
#include <fibre/legacy_protocol.hpp>
#include "print_utils.hpp"
#include <memory>
#include <algorithm>

#if FIBRE_ALLOW_HEAP
#include <unordered_map>
#include <string>
#endif

DEFINE_LOG_TOPIC(FIBRE);
USE_LOG_TOPIC(FIBRE);

using namespace fibre;

struct DiscoveryContext {
};

#if FIBRE_ALLOW_HEAP

template<typename T>
T* my_alloc() {
    return new T{};
}

template<typename T>
void my_free(T* ctx) {
    delete ctx;
}

#else

template<typename T>
struct TheInstance {
    static T instance;
    static bool in_use;
};

template<typename T> T TheInstance<T>::instance{};
template<typename T> bool TheInstance<T>::in_use = false;

template<typename T>
T* my_alloc() {
    if (!TheInstance<T>::in_use) {
        TheInstance<T>::in_use = true;
        return &TheInstance<T>::instance;
    } else {
        return nullptr;
    }
}

template<typename T>
void my_free(T* ctx) {
    if (ctx == &TheInstance<T>::instance) {
        TheInstance<T>::in_use = false;
    } else {
        FIBRE_LOG(E) << "bad instance";
    }
}

#endif

Context* fibre::open(EventLoop* event_loop) {
    Context* ctx = my_alloc<Context>();
    if (!ctx) {
        FIBRE_LOG(E) << "already opened";
        return nullptr;
    }

    ctx->event_loop = event_loop;
    return ctx;
}

void fibre::close(Context* ctx) {
    if (ctx->n_domains) {
        FIBRE_LOG(W) <<ctx->n_domains << " domains are still open";
    }

    my_free<Context>(ctx);
}

#if FIBRE_ALLOW_HEAP
Domain* Context::create_domain(std::string specs) {
    FIBRE_LOG(D) << "creating domain with path \"" << specs << "\"";

    Domain* domain = new Domain(); // deleted in close_domain
    domain->ctx = this;

    std::string::iterator prev_delim = specs.begin();
    while (prev_delim < specs.end()) {
        auto next_delim = std::find(prev_delim, specs.end(), ';');
        auto colon = std::find(prev_delim, next_delim, ':');
        auto colon_end = std::min(colon + 1, next_delim);

        std::string name{prev_delim, colon};
        auto it = discoverers.find(name);

        if (it == discoverers.end()) {
            FIBRE_LOG(W) << "transport layer \"" << name << "\" not implemented";
        } else {
            domain->channel_discovery_handles[name] = nullptr;
            it->second->start_channel_discovery(domain,
                    &*colon_end, next_delim - colon_end,
                    &domain->channel_discovery_handles[name]);
        }

        prev_delim = std::min(next_delim + 1, specs.end());
    }

    n_domains++;
    return domain;
}

void Context::close_domain(Domain* domain) {
    for (auto& it: domain->channel_discovery_handles) {
        discoverers[it.first]->stop_channel_discovery(it.second);
    }
    domain->channel_discovery_handles.clear();
    delete domain;
    n_domains--;
}

void Context::register_backend(std::string name, ChannelDiscoverer* backend) {
    if (discoverers.find(name) != discoverers.end()) {
        FIBRE_LOG(W) << "Discoverer " << name << " already registered";
        return; // TODO: report status
    }
    
    discoverers[name] = backend;
}

void Context::deregister_backend(std::string name) {
    auto it = discoverers.find(name);
    if (it == discoverers.end()) {
        FIBRE_LOG(W) << "Discoverer " << name << " not registered";
        return; // TODO: report status
    }
    
    discoverers.erase(it);
}
#endif

#if FIBRE_ENABLE_CLIENT
void Domain::start_discovery(Callback<void, Object*, Interface*> on_found_object, Callback<void, Object*> on_lost_object) {
    on_found_object_ = on_found_object;
    on_lost_object_ = on_lost_object;
    for (auto& it: root_objects_) {
        on_found_object_.invoke(it.first, it.second);
    }
}

void Domain::stop_discovery() {
    auto on_lost_object = on_lost_object_;
    on_found_object_ = nullptr;
    on_lost_object_ = nullptr;
    for (auto& it: root_objects_) {
        on_lost_object.invoke(it.first);
    }
}
#endif

void Domain::add_channels(ChannelDiscoveryResult result) {
    FIBRE_LOG(D) << "found channels!";

    if (result.status != kFibreOk) {
        FIBRE_LOG(W) << "discoverer stopped";
        return;
    }

    if (!result.rx_channel || !result.tx_channel) {
        FIBRE_LOG(W) << "unidirectional operation not supported yet";
        return;
    }

#if FIBRE_ENABLE_CLIENT || FIBRE_ENABLE_SERVER
    // Deleted during on_stopped()
    auto protocol = new fibre::LegacyProtocolPacketBased(result.rx_channel, result.tx_channel, result.mtu);
#if FIBRE_ENABLE_CLIENT
    protocol->start(MEMBER_CB(this, on_found_root_object), MEMBER_CB(this, on_lost_root_object), MEMBER_CB(this, on_stopped));
#else
    protocol->start(MEMBER_CB(this, on_stopped));
#endif
#endif
}

#if FIBRE_ENABLE_CLIENT
void Domain::on_found_root_object(LegacyObjectClient* obj_client, std::shared_ptr<LegacyObject> obj) {
    Object* root_object = reinterpret_cast<Object*>(obj.get());
    Interface* root_intf = reinterpret_cast<Interface*>(obj->intf.get());
    root_objects_[root_object] = root_intf;
    on_found_object_.invoke(root_object, root_intf);
}

void Domain::on_lost_root_object(LegacyObjectClient* obj_client, std::shared_ptr<LegacyObject> obj) {
    Object* root_object = reinterpret_cast<Object*>(obj.get());
    auto it = root_objects_.find(root_object);
    root_objects_.erase(it);
    on_lost_object_.invoke(root_object);
}
#endif

void Domain::on_stopped(LegacyProtocolPacketBased* protocol, StreamStatus status) {
    delete protocol;
}
