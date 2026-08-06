from skywing.core.types import ProcessorData
from skywing.mid.base_processor import Processor


class SimpleSumData(ProcessorData):
    data: dict[str, float]


class SimpleSumProcessor(Processor):
    def __init__(self, data: float, **kwargs):
        """Initialize the SimpleSumProcessor.

        Args:
            data: Initial data value
            **kwargs: Keyword arguments, must include 'unique_id'
        """
        super().__init__(data, **kwargs)
        # Setup sum dictionary with agents value
        self.unique_id = kwargs["unique_id"]
        self.data = {self.unique_id: data}
        self.result = data

    def update_data(self, data: float):
        self.data[self.unique_id] = data

    # Called by iteration
    def process_update(self, my_tag: str, recv_data: dict[str, SimpleSumData]):
        """Process udpates from the recv data"""
        new_data = {}
        # Copy all received values into agents own dictionary
        for _tag, mappings in recv_data.items():
            for id, value in mappings.data.items():
                # Update dictionary to include new values
                new_data[id] = value

        # Update our own value, in case another agent overwrote it with the wrong value. (Can't really happen with this logic tho)
        new_data[self.unique_id] = self.data[self.unique_id]

        # Save new dictionary
        self.data = new_data
        # Sum the result
        self.result = sum(self.data.values())

    def prepare_for_publication(self):
        """Prepares data for publication"""
        return SimpleSumData(data=self.data)
