import functools
import inspect

_skywing_jobs = {}
_call_site_cache = {}


def get_call_site_key():
    """
    Retrieves a unique key for the current call site in the user code.

    The key is derived from the filename and line number of the code that is
    three levels above the current function call. This is used to uniquely
    identify different invocations of jobs.

    Returns:
        Tuple[str, int]: A tuple containing the filename and line number of the call site.
    """
    # Retrieve the user function frame (3 levels above get_call_site_key).
    current_frame = inspect.currentframe()
    if current_frame is None or current_frame.f_back is None:
        raise RuntimeError("Unable to retrieve frame information")
    if (
        current_frame.f_back.f_back is None
        or current_frame.f_back.f_back.f_back is None
    ):
        raise RuntimeError("Unable to retrieve caller frame information")
    frame = current_frame.f_back.f_back.f_back
    # Get the frame information; you could include additional details if needed.
    frame_info = inspect.getframeinfo(frame)
    # Use filename and line number as a simple key.
    return (frame_info.filename, frame_info.lineno)


def get_unique_id_for_call_site():
    """
    Generates or retrieves a unique ID for the current call site.

    This function uses `get_call_site_key` to identify the call site and caches
    the unique IDs for each call site.

    Returns:
        int: A unique integer ID for the call site.
    """
    key = get_call_site_key()
    if key not in _call_site_cache:
        _call_site_cache[key] = len(_call_site_cache)
    return _call_site_cache[key]


def add_unique_id(func):
    """
    Decorator that injects a unique ID into the keyword arguments of the decorated function.

    The unique ID is generated based on the call site of the decorated function.

    Args:
        func (Callable): The function to decorate.

    Returns:
        Callable: The decorated function.
    """

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # Get or create a unique ID based on the call site.
        unique_id = get_unique_id_for_call_site()
        # Inject the unique ID into the keyword arguments.
        kwargs["unique_id"] = unique_id
        return func(*args, **kwargs)

    return wrapper


def register_uid(uid, job):
    """Register a new unique ID.

    Args:
       uid (int) : The unique ID to register.

    Returns:
       None
    """
    _skywing_jobs[uid] = job


def check_for_uid(uid):
    """Check if a unique ID has already been registered.

    Args:
       uid (int) : The unique ID to check.

    Returns:
       bool : True if already registerd, false otherwise.
    """
    return uid in _skywing_jobs


def get_job_by_uid(uid):
    """Get the job assocaited with a given uid.

    Args:
       uid (int) : The unique ID of the job to get.

    Returns:
       The job associated with the unique ID.

    Raises an error if the given uid is not associated with any job.
    """
    return _skywing_jobs[uid]
