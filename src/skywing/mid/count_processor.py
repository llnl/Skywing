import numpy as np

from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor
from skywing.mid.idempotent_processor import IdemData, MaxProcessor
from skywing.mid.push_sum_processor import PushSumData, PushSumProcessor


class CountData(ProcessorData):
    max_data: IdemData
    mean_data: PushSumData


class CountProcessor(Processor):
    """This processor counts the number of agents in the collective.

    We utilize a MaxProcessor and a PushSumProcessor in the following way.

    First, every agent begins with a randomly generated value::

        x = (random)

    The MaxProcessor is used to get the maximum of the local x values
    over the collective. Then each agent compares their local x to
    the maximum value and sets::

        y = 1 if my x == max(x)
        y = 0 otherwise

    Thus, after the MaxProcessor has sufficiently iterated, there will
    be only a single agent with y = 1 and the rest will have y = 0.
    The PushSumProcessor is then used to get an average over the y
    values, [1, 0, 0, 0, ...], which will converge to 1/N, where N
    is the size of the collective.
    """

    # WM: todo - no data needed here... so doesn't fit the standard signature...
    #            This is annoying because to be used with the simple_iteration.py
    #            example, I need it to have the same signature... Consider changing
    #            base processor init funciton to have only kwargs?
    def __init__(self, _, **kwargs):
        super().__init__(None, **kwargs)
        # WM: todo - what's the probability of two agents having exaclty the same starting x? Is there a more robust approach?
        self.x = np.random.rand()
        self.y = 0.0
        self.max_processor = MaxProcessor(self.x, **kwargs)
        self.mean_processor = PushSumProcessor(self.y, **kwargs)
        self.result = 1.0

    def process_update(self, my_tag: str, recv_data: dict[str, tuple]):

        # WM: todo - is there a nicer way to organize this such that we don't need to
        #            manually do this data separation for nested processors? Refactor
        #            recv_data to have structure recv_data.property = {_tag: data for that property}?
        # Sort out incoming data for each sub processor
        max_recv_data = {}
        mean_recv_data = {}
        for _tag, nbr_data in recv_data.items():
            max_recv_data[_tag] = nbr_data.max_data
            mean_recv_data[_tag] = nbr_data.mean_data

        # Call underlying process_update() functions
        self.max_processor.process_update(my_tag, max_recv_data)
        self.mean_processor.process_update(my_tag, mean_recv_data)

        # Check whether this agent owns the maximal value
        # If yes, set y = 1.0, otherwise y = 0.0
        if self.x == self.max_processor.query():
            self.y = 1.0
        else:
            self.y = 0.0

        # Update data for the mean processor
        self.mean_processor.update_data(self.y)

        # Update result
        N_inv = self.mean_processor.query()
        if not N_inv:
            N_inv = 1.0
        self.result = round(1.0 / N_inv)

    def prepare_for_publication(self):
        return CountData(
            max_data=self.max_processor.prepare_for_publication(),
            mean_data=self.mean_processor.prepare_for_publication(),
        )
