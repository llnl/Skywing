from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor
from skywing.mid.count_processor import CountData, CountProcessor
from skywing.mid.push_sum_processor import PushSumData, PushSumProcessor


class SumData(ProcessorData):
    mean_data: PushSumData
    count_data: CountData


class SumProcessor(Processor):
    """This processor calculates the sum of the input data over
    the collective. It utilizes an underlying PushSumProcessor
    to get the mean of the input data over the collective and a
    CountProcessor to get the size of the collective, then returns:
    result = (input data mean) * (collective size)
    """

    def __init__(self, data, **kwargs):
        super().__init__(data, **kwargs)
        self.mean_processor = PushSumProcessor(self.data, **kwargs)
        self.count_processor = CountProcessor(None, **kwargs)

    def update_data(self, data):
        super().update_data(data)
        self.mean_processor.update_data(self.data)

    def process_update(self, my_tag: str, recv_data: dict[str, tuple]):

        # WM: todo - is there a nicer way to organize this such that we don't need to
        #            manually do this data separation for nested processors? Refactor
        #            recv_data to have structure recv_data.property = {_tag: data for that property}?
        # Sort out incoming data for each sub processor
        mean_recv_data = {}
        count_recv_data = {}
        for _tag, nbr_data in recv_data.items():
            mean_recv_data[_tag] = nbr_data.mean_data
            count_recv_data[_tag] = nbr_data.count_data

        # Call underlying process_update() functions
        self.mean_processor.process_update(my_tag, mean_recv_data)
        self.count_processor.process_update(my_tag, count_recv_data)

        # Update result
        self.result = (
            self.mean_processor.query() * self.count_processor.query()  # pyright: ignore [reportOperatorIssue]
        )

    def prepare_for_publication(self):
        return SumData(
            mean_data=self.mean_processor.prepare_for_publication(),
            count_data=self.count_processor.prepare_for_publication(),
        )

    # Extra routines to get the mean or count for convenience
    def get_mean(self):
        return self.mean_processor.query()

    def get_count(self):
        return self.count_processor.query()

    # WM: todo - remove... I'm not bothering to implement these for compatibility with C++
    def convert(self, publish_data: tuple):
        return [], [], []

    def deconvert(self, recv_strings, recv_doubles, recv_ints):
        return ()
