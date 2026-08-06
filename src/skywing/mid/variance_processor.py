from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor
from skywing.mid.push_sum_processor import PushSumProcessor

from .push_sum_processor import PushSumData


class VarianceData(ProcessorData):
    mean_data: PushSumData
    variance_data: PushSumData


class VarianceProcessor(Processor):
    def __init__(self, data: float, **kwargs):
        super().__init__(data, **kwargs)
        self.mean_processor = PushSumProcessor(data, **kwargs)
        # Initialize variance processor with squared deviation from initial data
        squared_deviation = 0.0
        self.variance_processor = PushSumProcessor(squared_deviation, **kwargs)
        self.result: float = 0.0

    def update_data(self, data: float):
        super().update_data(data)
        # Update mean processor with new data
        self.mean_processor.update_data(data)

    # Called by iteration
    def process_update(self, my_tag: str, recv_data: dict[str, VarianceData]):
        """Process updates from the recv data"""
        mean_recv_data = {}
        variance_recv_data = {}

        for tag, data_tuple in recv_data.items():
            # First value is for mean processor
            mean_recv_data[tag] = data_tuple.mean_data
            # Next value is for variance processor
            variance_recv_data[tag] = data_tuple.variance_data

        # Process updates for mean processor
        self.mean_processor.process_update(my_tag, mean_recv_data)

        # Update variance processor's data with new squared deviation
        mean = self.mean_processor.query()
        if mean is not None:
            squared_deviation = (self.data - mean) ** 2
            self.variance_processor.update_data(squared_deviation)

        # Process updates for variance processor
        self.variance_processor.process_update(my_tag, variance_recv_data)

        # Update result with variance
        self.result = self.variance_processor.query()

    def prepare_for_publication(self):
        """Prepares data for publication"""
        return VarianceData(
            mean_data=self.mean_processor.prepare_for_publication(),
            variance_data=self.variance_processor.prepare_for_publication(),
        )
