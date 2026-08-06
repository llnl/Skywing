from skywing.mid.stop_policy import StopPolicyDefault
from skywing.mid.utils import add_unique_id


class IterationCppCore:
    """This class implements the skeleton of a distributed iteration in which
    agents alternately communicate with their neighbors and perform local
    computations.
    """

    @add_unique_id
    def __init__(self, processor, agent, stop_policy=None, **kwargs):
        self.processor = processor
        if stop_policy is None:
            self.stop_policy = StopPolicyDefault(**kwargs)
        else:
            self.stop_policy = stop_policy
        self.agent = agent
        self.uid = kwargs["unique_id"]
        self.iter_data = {}

    def launch(self) -> None:
        """Start the iteration. Note that this does not require/assume the iteration has launched on neighbor agents.
        The underlying call here is to self.agent.submit_job() and self.agent.launch(). If the agent is already running,
        then self.agent.launch() is a no-op.
        WM: todo - what happens if we call agent.submit_job() for the same job multiple times? Need to know this to know
        the behavior of calling iteration.launch() multiple times. Should we raise a warning/error?
        """
        self.agent.submit_job("job_" + str(self.uid), self)
        self.agent.launch()

    def update_data(self, *args) -> None:
        """Update the local data for the iteration (may reset/restart some algorithms as appropriate)."""
        self.processor.update_data(*args)

    def update_parameters(self, **kwargs) -> None:
        """Update algorithm-specific parameters."""
        self.process.update_parameters(**kwargs)

    def query(self):
        """Get the current result of the iteration."""
        return self.processor.query()

    def publish_data(self) -> None:
        """Publish current data to self.my_tag
        WM: todo - generalize to publish to multiple tags if sending different info to different nbrs?
        """
        data = self.processor.prepare_for_publication()
        data_to_publish = self.processor.convert(data)
        self.job.publish(self.my_tag, data_to_publish)

    def gather_data(self) -> None:
        """Update the self.iter_data dictionary with data received from
        other tags where possible. Before the first data received from tag,
        self.iter_data[tag] is not a valid key. Once the value at that tag
        is populated, it will remain unchanged until new data is received
        and will then be overwritten.
        """
        for tag in self.other_tags:
            if self.job.has_data(tag):
                # Retrieves any new data received under this tag
                tag_data = self.job.get_data_if_present(tag)
                self.iter_data[tag] = self.processor.deconvert(
                    # Current skywing type is lists of strings, ints, and floats
                    tag_data[0],
                    tag_data[1],
                    tag_data[2],
                )

    def run(self, job) -> None:
        """
        This function sets up publications and subscriptions, then performs the main
        loop of updating this agent's publication, retrieving data from subscriptions,
        and performing local computation.

        Args:
            job: Job instance providing publish/subscribe methods (wraps C++ JobHandle)
        """
        self.job = job
        self.my_tag = self.agent.create_tag_from_uid(self.uid)
        # Subscribe to the tags of all neighbors
        self.other_tags = [self.my_tag] + [
            self.agent.create_tag_from_uid(self.uid, nbr) for nbr in self.agent.nbrs
        ]
        # Ensure list of tags has no duplicates
        self.other_tags = list(set(self.other_tags))

        job.declare_publication_intent(self.my_tag)
        for tag in self.other_tags:
            job.subscribe(tag)

        self.stop_policy.initialize()

        # Loop over the following steps:
        # 1. Check if we should keep iterating.
        # 2. Publish my data out to neighbors.
        # 3. Gather any newly available data.
        # 4. Pass current data to processor to process update.
        while not self.stop_policy.should_stop(self.processor):
            self.publish_data()
            self.gather_data()
            self.processor.process_update(self.my_tag, self.iter_data)
