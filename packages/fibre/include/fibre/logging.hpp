#ifndef __FIBRE_LOGGING_HPP
#define __FIBRE_LOGGING_HPP

#include <cstddef>

#ifndef FIBRE_MAX_LOG_VERBOSITY
#define FIBRE_MAX_LOG_VERBOSITY 0
#endif
#ifndef FIBRE_DEFAULT_LOG_VERBOSITY
#define FIBRE_DEFAULT_LOG_VERBOSITY 2
#endif

namespace fibre {

enum log_level_t {
    LOG_LEVEL_F = 0,
    LOG_LEVEL_E = 1,
    LOG_LEVEL_W = 2,
    LOG_LEVEL_I = 3,
    LOG_LEVEL_D = 4,
    LOG_LEVEL_T = 5,
};

using LogSink = void (*)(void* context, log_level_t level, const char* topic,
                        const char* message, size_t length);

// 在协议执行上下文中设置；回调同步执行，message 只在回调期间有效。
void set_log_sink(LogSink sink, void* context = nullptr);

namespace detail {
struct LogDestination {
    LogSink sink;
    void* context;
};
extern LogDestination log_destination;
}

}

#if FIBRE_MAX_LOG_VERBOSITY

#include <sstream>

namespace fibre {

class LogEntry {
public:
    LogEntry(LogSink sink, void* context, log_level_t level, const char* topic)
        : sink_(sink), context_(context), level_(level), topic_(topic) {}

    ~LogEntry() {
        if (sink_) {
            const std::string message = stream_.str();
            sink_(context_, level_, topic_, message.data(), message.size());
        }
    }

    std::ostringstream& get_stream() { return stream_; }

private:
    LogSink sink_;
    void* context_;
    log_level_t level_;
    const char* topic_;
    std::ostringstream stream_;
};

template<typename Topic>
constexpr log_level_t get_default_log_verbosity() {
    return static_cast<log_level_t>(FIBRE_DEFAULT_LOG_VERBOSITY);
}

template<typename Topic>
constexpr log_level_t get_max_log_verbosity() {
    return static_cast<log_level_t>(FIBRE_MAX_LOG_VERBOSITY);
}

template<typename Topic, log_level_t Level>
LogEntry make_log_entry() {
    const auto destination = detail::log_destination;
    const bool enabled = Level <= get_default_log_verbosity<Topic>() &&
                         Level <= get_max_log_verbosity<Topic>();
    return {enabled ? destination.sink : nullptr, destination.context,
            Level, Topic::get_label()};
}

}

#define DEFINE_LOG_TOPIC(name) \
    struct LOG_TOPIC_ ## name { \
        static const char* get_label() { return #name; } \
    }
#define USE_LOG_TOPIC(name) using current_log_topic = LOG_TOPIC_ ## name
#define CONFIG_LOG_TOPIC(topic, default_verbosity, max_verbosity) \
    template<> constexpr fibre::log_level_t fibre::get_default_log_verbosity<LOG_TOPIC_ ## topic>() { return (default_verbosity); } \
    template<> constexpr fibre::log_level_t fibre::get_max_log_verbosity<LOG_TOPIC_ ## topic>() { return (max_verbosity); }
#define FIBRE_LOG(level) \
    fibre::make_log_entry<current_log_topic, fibre::LOG_LEVEL_ ## level>().get_stream()

#else

namespace fibre {
struct NullLogStream {
    template<typename T> NullLogStream& operator<<(const T&) { return *this; }
};
}

#define DEFINE_LOG_TOPIC(topic)
#define USE_LOG_TOPIC(topic)
#define CONFIG_LOG_TOPIC(topic, default_verbosity, max_verbosity)
#define FIBRE_LOG(level) fibre::NullLogStream()

#endif

#endif
