from typing import List, Tuple
from collections import namedtuple
import threading
import time
import skywing.skywing_cpp_interface as skywing_cpp_interface

Neighbor = namedtuple("Neighbor", ["addr", "port"])


class Agent:
    def __init__(self, name, addr, port):
        self.name = name
        self.port = port
        self.addr = addr
        self.manager = skywing_cpp_interface.Manager(self.port, self.name)
        self.nbrs = []

    @property
    def id(self):
        return self.name

    def configure_neighbors(self, nbrs: List[Tuple]):
        for nbr in nbrs:
            addr, port = nbr
            self.nbrs.append(Neighbor(addr, port))
            self.manager.configure_initial_neighbors(addr, port)

    def run_continuous(self, job):
        job()
        thread = threading.Thread(target=self.manager.run, daemon=True)
        thread.start()
        while job():
            time.sleep(1)
