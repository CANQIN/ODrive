
import sys
import time
import threading

if sys.version_info < (3, 3):
    class TimeoutError(Exception):
        pass
else:
    TimeoutError = TimeoutError

## Threading utils ##
class Event():
    """
    Alternative to threading.Event(), enhanced by the subscribe() function
    that the original fails to provide.
    @param Trigger: if supplied, the newly created event will be triggered
                    as soon as the trigger event becomes set
    """
    def __init__(self, trigger=None):
        self._evt = threading.Event()
        self._subscribers = []
        self._mutex = threading.Lock()
        if not trigger is None:
            trigger.subscribe(lambda: self.set())

    def is_set(self):
        return self._evt.is_set()

    def set(self):
        """
        Sets the event and invokes all subscribers if the event was
        not already set
        """
        subscribers = []
        self._mutex.acquire()
        try:
            if not self._evt.is_set():
                self._evt.set()
                for s in self._subscribers:
                    subscribers.append(s)
        finally:
            self._mutex.release()

        # Invoke subscribes with the mutex released to prevent deadlocks
        for s in subscribers:
            s()

    def subscribe(self, handler):
        """
        Invokes the specified handler exactly once as soon as the
        specified event is set. If the event is already set, the
        handler is invoked immediately.
        The subscribers are called in the reverse order in which they subscribed.
        Returns a function that can be invoked to unsubscribe.
        """
        if handler is None:
            raise TypeError
        self._mutex.acquire()
        try:
            self._subscribers.insert(0, handler)
            if self._evt.is_set():
                handler()
        finally:
            self._mutex.release()
        return handler
    
    def unsubscribe(self, handler):
        self._mutex.acquire()
        try:
            self._subscribers.pop(self._subscribers.index(handler))
        finally:
            self._mutex.release()

    def wait(self, timeout=None):
        if not self._evt.wait(timeout=timeout):
            raise TimeoutError()

    def trigger_after(self, timeout):
        """
        Triggers the event after the specified timeout.
        This function returns immediately.
        """
        def delayed_trigger():
            if not self.wait(timeout=timeout):
                self.set()
        threading.Thread(target=delayed_trigger)
        t.daemon = True
        t.start()
        

def wait_any(timeout=None, *events):
    """
    Blocks until any of the specified events are triggered.
    Returns the index of the event that was triggerd or raises
    a TimeoutError
    Param timeout: A timeout in seconds
    """
    or_event = threading.Event()
    subscriptions = []
    for event in events:
        subscriptions.append((event, event.subscribe(lambda: or_event.set())))
    or_event.wait(timeout=timeout)
    for event, sub in subscriptions:
        event.unsubscribe(sub)
    for i in range(len(events)):
        if events[i].is_set():
            return i
    raise TimeoutError()


## Log utils ##

class Logger():
    """
    Logs messages to stdout
    """

    COLOR_DEFAULT = 0
    COLOR_GREEN = 1
    COLOR_CYAN = 2
    COLOR_YELLOW = 3
    COLOR_RED = 4

    def __init__(self, verbose=True):
        self._prefix = ''
        self._skip_bottom_line = False # If true, messages are printed one line above the cursor
        self._verbose = verbose
        self._print_lock = threading.Lock()

    def indent(self, prefix='  '):
        indented_logger = Logger()
        indented_logger._prefix = self._prefix + prefix
        return indented_logger

    def print_on_second_last_line(self, text, color):
        self.print_colored(text, color)

    def print_colored(self, text, color):
        with self._print_lock:
            sys.stdout.write(text + '\n')
            sys.stdout.flush()

    def debug(self, text):
        if self._verbose:
            self.print_colored(self._prefix + text, Logger.COLOR_DEFAULT)
    def success(self, text):
        self.print_colored(self._prefix + text, Logger.COLOR_GREEN)
    def info(self, text):
        self.print_colored(self._prefix + text, Logger.COLOR_DEFAULT)
    def notify(self, text):
        self.print_colored(self._prefix + text, Logger.COLOR_CYAN)
    def warn(self, text):
        self.print_colored(self._prefix + text, Logger.COLOR_YELLOW)
    def error(self, text):
        # TODO: write to stderr
        self.print_colored(self._prefix + text, Logger.COLOR_RED)
