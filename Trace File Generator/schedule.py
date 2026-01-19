#!/usr/bin/env python3

"""
This code is provided solely for the personal and private use of students
taking the CSC369H course at the University of Toronto. Copying for purposes
other than this use is expressly prohibited. All forms of distribution of
this code, including but not limited to public repositories on GitHub,
GitLab, Bitbucket, or any other online platform, whether as given or with
any changes, are expressly prohibited.

Author: Louis Ryan Tan
"""

from collections import deque
import struct
import re
from argparse import ArgumentParser
from abc import abstractmethod, ABC
from random import choice, seed
from typing import override

START_REFFILE = "start.ref"

TRACE_REG = re.compile(r"([ILMSF])\s+([0-9a-fA-F]+)\s+(\d+)")


class ParseError(BaseException):
    def __init__(self, msg):
        super().__init__()
        self.msg = msg


def parse_memtrace_line(line):
    m = re.match(TRACE_REG, line)
    if m is None:
        raise ParseError(line)
    return m.group(1), int(m.group(2), 16), int(m.group(3))


cur = 0
procs = dict()


class SchedulerError(BaseException):
    def __init__(self, msg):
        super().__init__()
        self.msg = msg


class Process:
    """
    Class that holds relevant data corresponding to a trace file.
    """

    def __init__(self, refdir, filename, vpid):
        self.refdir = refdir
        self.filename = filename
        self.tracefile = open(f"{refdir}/{filename}", "r")
        self.vpid = vpid

    def __eq__(self, o):
        return self.vpid == o.vpid

    def trace(self):
        return self.tracefile.readline()

    def end(self):
        self.tracefile.close()


class Scheduler(ABC):
    """
    Abstract base class of a scheduler.
    """

    @abstractmethod
    def schedule(self, process) -> None:
        pass

    @abstractmethod
    def run(self) -> Process:
        pass

    @abstractmethod
    def kill(self, vpid) -> None:
        pass

    @abstractmethod
    def is_empty(self) -> bool:
        pass


class RandomScheduler(Scheduler):
    """
    Random scheduler with a fixed time quantum.
    """

    def __init__(self, quantum=10):
        self.processes: list[Process] = []
        self.quantum = quantum
        self.__cur_interval = 0
        self.running: Process | None = None

    @override
    def schedule(self, process: Process) -> None:
        self.processes.append(process)

    @override
    def kill(self, vpid):
        # not called often (hopefully), shouldn't matter in terms of perf
        for p in self.processes:
            if p.vpid == vpid:
                self.processes.remove(p)

                p.end()
        if self.running is not None and self.running.vpid == vpid:
            self.__cur_interval = 0
            self.running = None

    @override
    def run(self) -> Process:
        assert len(self.processes) > 0
        if self.running is None:
            self.__cur_interval = 0
            self.running = choice(self.processes)
            return self.running

        assert self.__cur_interval <= self.quantum
        if self.__cur_interval == self.quantum:
            self.__cur_interval = 0
            self.running = choice(self.processes)
            return self.running

        self.__cur_interval += 1
        return self.running

    @override
    def is_empty(self):
        return len(self.processes) == 0


class RoundRobinScheduler(Scheduler):
    """
    Round-Robin scheduler with a fixed time quantum.
    """

    def __init__(self, quantum=10):
        self.processes: deque[Process] = deque()
        self.quantum = quantum
        self.__cur_interval = 0
        self.running: Process | None = None

    @override
    def schedule(self, process: Process):
        self.processes.append(process)

    @override
    def kill(self, vpid):
        if self.running is not None and self.running.vpid == vpid:
            self.__cur_interval = 0
            self.running.end()
            self.running = None
        else:
            # not called often (hopefully), shouldn't matter in terms of perf
            for p in self.processes:
                if p.vpid == vpid:
                    self.processes.remove(p)
                    p.end()

    @override
    def run(self) -> Process:
        assert len(self.processes) > 0
        if self.running is None:
            self.__cur_interval = 0
            self.running = self.processes.popleft()

        assert self.__cur_interval <= self.quantum

        if self.__cur_interval == self.quantum:
            self.processes.appendleft(self.running)
            self.running = self.processes.popleft()

        self.__cur_interval += 1
        return self.running

    @override
    def is_empty(self):
        return len(self.processes) == 0


class TraceCombinator:
    def __init__(self, scheduler: Scheduler, outpath, out_as_bin: bool):
        self.scheduler = scheduler
        self.outpath = outpath
        self.out_as_bin = out_as_bin
        if self.out_as_bin:
            self.outfile = open(outpath, "wb")
        else:
            self.outfile = open(outpath, "w")

    def writeline(self, vpid: int, reftype: str, vaddr: int, value: int):
        if self.out_as_bin:
            self.outfile.write(
                struct.pack(
                    "@LccxQ",
                    vpid,
                    ord(reftype),
                    vaddr,
                    value,
                )  # type: ignore
            )
        else:
            self.outfile.write(f"{vpid} {reftype} {vaddr:x} {value}\n")  # type: ignore # noqa: E501

    def combine(self, refdirs: list[str]):
        curmax_vpid = 0
        assert refdirs is not None
        for refdir in refdirs:
            p = Process(refdir, START_REFFILE, curmax_vpid)
            curmax_vpid += 1
            self.scheduler.schedule(p)

            self.writeline(p.vpid, "B", 0, 0)
        while not self.scheduler.is_empty():
            p = self.scheduler.run()
            traceline = p.trace()
            if traceline == "":
                # process ends
                self.scheduler.kill(p.vpid)
                self.writeline(p.vpid, "E", 0, 0)
                continue
            reftype, vaddr, val = parse_memtrace_line(traceline)
            if reftype == "F":
                # vaddr here is the child real pid
                cpid = vaddr
                chld = Process(p.refdir, f"{cpid}.ref", curmax_vpid)
                vaddr = curmax_vpid
                curmax_vpid += 1
                self.scheduler.schedule(chld)
            self.writeline(p.vpid, reftype, vaddr, val)

    def finish(self):
        self.outfile.close()


if __name__ == "__main__":
    seed(369)
    ap = ArgumentParser()
    ap.add_argument(
        "--output", "-o", type=str, help="output file name", default="out.mref"
    )
    ap.add_argument(
        "--refs",
        "-r",
        type=str,
        help="directories containing memory references",
        nargs="+",
        required=True,
    )
    ap.add_argument("--binary", "-b", action="store_true")

    args = ap.parse_args()

    if args.binary:
        args.output += ".bin"

    s = RandomScheduler()
    tc = TraceCombinator(s, args.output, args.binary)
    tc.combine(args.refs)
    tc.finish()
