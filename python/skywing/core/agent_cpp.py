import threading
from collections import namedtuple

from skywing import skywing_bind_interface

Neighbor = namedtuple("Neighbor", ["addr", "port"])


class AgentCpp:
    def __init__(self, addr: str, port: int) -> None:
        self.port = port
        self.addr = addr
        self.manager = skywing_bind_interface.Manager(
            self.port, str(addr) + "_" + str(port)
        )
        self.manager_thread = None
        self.nbrs = []
        self.job_uids = {}

    def configure_neighbors(self, nbrs: list[tuple], timeout: int = 10) -> None:
        for nbr in nbrs:
            addr, port = nbr
            self.nbrs.append(Neighbor(addr, port))
            self.manager.configure_initial_neighbors(addr, port, timeout)

    def submit_job(self, job_name: str, pyjob) -> None:
        self.manager.submit_pyjob(job_name, pyjob)

    def create_tag_from_uid(self, uid, nbr=None) -> str:
        if nbr is None:
            # then assume this is our tag
            return str(self.addr) + "_" + str(self.port) + "_" + str(uid)
        else:
            return str(nbr.addr) + "_" + str(nbr.port) + "_" + str(uid)

    def launch(self) -> None:
        if self.manager_thread is None:
            self.manager_thread = threading.Thread(target=self.manager.run, daemon=True)
            self.manager_thread.start()
