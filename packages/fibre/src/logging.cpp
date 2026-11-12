#include <fibre/logging.hpp>

namespace fibre {

detail::LogDestination detail::log_destination{};

void set_log_sink(LogSink sink, void* context) {
    detail::log_destination = {sink, context};
}

}
