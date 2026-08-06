import threading


class Subscription:
    """Subscription to a tag with data buffer.

    Keeps only the most recent value, using version numbers to prevent
    premature overwrites and detect skipped messages.
    """

    # Constant for uninitialized version
    NO_VERSION = 0xFFFFFFFF  # Max uint32

    def __init__(self, tag: str) -> None:
        self.tag = tag
        self._most_recent_value = None  # Most recent value only

        # Version tracking
        # stored_version: version of the data currently in the buffer
        # last_fetched_version: version of the last data returned by get_data()
        self._stored_version: int = self.NO_VERSION
        self._last_fetched_version: int = self.NO_VERSION

        # Lock protecting buffer state (value, versions).
        # Accessed from multiple threads:
        # - Manager thread: calls add_data() when messages arrive
        # - Job thread: calls has_data() and get_data() to read messages
        self._lock = threading.Lock()

    def has_data(self) -> bool:
        """Check if NEW data has arrived since last read.

        Logic: stored_version >= last_fetched_version + 1
        This detects when messages have been received but not yet consumed.

        """
        with self._lock:
            # No data ever received
            if self._stored_version == self.NO_VERSION:
                return False
            # Data received but never fetched
            if self._last_fetched_version == self.NO_VERSION:
                return True
            # Normal case: check if we have unread data
            return self._stored_version >= self._last_fetched_version + 1

    def get_data(self):
        """Get the most recent data and mark as read.

        Sets last_fetched_version = stored_version to mark the data as consumed.
        """
        with self._lock:
            # Check if data is available (inline to avoid double lock)
            # Match the has_data() logic
            has_data = False
            if self._stored_version == self.NO_VERSION:
                has_data = False
            elif self._last_fetched_version == self.NO_VERSION:
                has_data = True
            else:
                has_data = self._stored_version >= self._last_fetched_version + 1

            if has_data:
                data = self._most_recent_value
                # Mark as read by updating last_fetched_version
                self._last_fetched_version = self._stored_version
                return data
            return None

    def add_data(self, data, version: int) -> None:
        """Add data to the buffer, only if version is newer.

        Only overwrites if version > stored_version. This prevents old messages
        from overwriting newer ones.

        Args:
            data: Data to store
            version: Version number of this data (must be monotonically increasing)
        """
        with self._lock:
            # Only update if this is a newer version (or first message)
            if (
                version > self._stored_version
                or self._stored_version == self.NO_VERSION
            ):
                self._most_recent_value = data
                self._stored_version = version


class Job:
    """Pure Python Job for managing subscriptions and publications.

    The Job manages subscriptions and publications. It pulls data from
    subscriptions rather than using callbacks.

    Attributes:
        job_id: Unique identifier for this job
        manager: Reference to the manager
        to_run: Function to execute in this job
        subscriptions: Map of tag_id -> Subscription
        tags_produced: Tags this job publishes on
    """

    def __init__(self, job_id: str, manager, to_run, timeout: float = 10.0) -> None:
        """Initialize Job.

        Args:
            job_id: Unique identifier for this job
            manager: Manager instance
            to_run: Callable to execute, receives the Job instance as its argument
            timeout: Duration for job execution (default: 10 seconds)
        """
        self.job_id = job_id
        self.manager = manager
        self.to_run = to_run
        self.timeout = timeout

        # Subscriptions: tag_id -> Subscription
        self.subscriptions: dict[str, Subscription] = {}

        self._subs_lock = threading.Lock()

        # Tags this job produces
        self.tags_produced: set[str] = set()

        # Lock protecting processor state accessed from multiple threads.
        # The processor is accessed from:
        # 1. Job thread (iteration.run()) - main algorithm execution
        # 2. Main thread (update_data/query calls) - external queries/updates
        self._processor_lock = threading.Lock()

        # Processor reference (set by iteration)
        self._processor = None

        # Job state
        self._has_started = False
        self._has_finished = False
        self._thread = None

    def declare_publication_intent(self, *tags: str) -> None:
        """Declare intent to publish on tags.

        Must be called before publishing on a tag.

        Args:
            *tags: Tag IDs to publish on
        """
        for tag in tags:
            self.tags_produced.add(tag)

        # Inform manager about new tags
        if tags:
            self.manager.report_new_publish_tags(list(tags))

    def subscribe(self, *tags: str) -> None:
        """Subscribe to tags.

        Creates subscription buffers for the specified tags.

        Args:
            *tags: Tag IDs to subscribe to
        """
        with self._subs_lock:
            for tag in tags:
                if tag not in self.subscriptions:
                    self.subscriptions[tag] = Subscription(tag)

        # Inform manager about subscriptions
        if tags:
            self.manager.job_subscribe(self, list(tags))

    def has_data(self, tag: str) -> bool:
        """Check if a tag has data available.

        Args:
            tag: Tag ID to check

        Returns:
            True if data is available, False otherwise
        """
        with self._subs_lock:
            if tag in self.subscriptions:
                return self.subscriptions[tag].has_data()
            return False

    def get_data_if_present(self, tag: str):
        """Get data from a tag if available.

        This is non-blocking and returns None if no data is available.

        Args:
            tag: Tag ID to get data from

        Returns:
            Data if available, None otherwise
        """
        with self._subs_lock:
            if tag in self.subscriptions:
                return self.subscriptions[tag].get_data()
            return None

    def publish(self, tag: str, value) -> None:
        """Publish data on a tag.

        Args:
            tag: Tag ID to publish on
            value: Value to publish (must be serializable)
        """
        # In debug mode, assert that tag was declared
        assert tag in self.tags_produced, f"Tag '{tag}' not declared for publication"

        # Send to manager for broadcasting
        self.manager.publish(tag, value)

    def process_data(self, tag_id: str, data, version: int) -> bool:
        """Process incoming data by storing it in the subscription buffer.

        This is called by the Manager when data arrives.

        Args:
            tag_id: Tag the data arrived on
            data: The data received
            version: Version number of this data (monotonically increasing)

        Returns:
            True if successful, False if tag not subscribed
        """
        with self._subs_lock:
            if tag_id in self.subscriptions:
                self.subscriptions[tag_id].add_data(data, version)
                return True
            return False

    def run(self) -> threading.Thread:
        """Start the job in a new thread.

        Returns:
            Thread object for the job
        """
        self._has_started = True

        def thread_func():
            try:
                self.to_run(self)  # Pass the Job instance to the callable
            finally:
                self._has_finished = True

        self._thread = threading.Thread(target=thread_func, daemon=True)
        self._thread.start()
        return self._thread

    def has_started(self) -> bool:
        """Check if job has started."""
        return self._has_started

    def has_finished(self) -> bool:
        """Check if job has finished."""
        return self._has_finished

    @property
    def processor_lock(self):
        """Get the processor lock for thread-safe access to processor state.

        This lock protects processor state accessed from both the job thread
        (in iteration.run()) and the main thread (in update_data/query calls).

        Returns:
            The processor lock
        """
        return self._processor_lock
