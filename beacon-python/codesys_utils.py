import time


class PT:
    def __init__(self):
        self.q = False
        self.in_ = False
        self.start = 0
        self.reset()

    def time(self):
        return time.time() * 1000

    def reset(self):
        self.q = False
        self.in_ = False
        self.start = 0

    def ton(self, signal, delay):
        if not self.in_:
            self.start = self.time()
        self.in_ = signal
        self.q = (self.q or (self.time() - self.start) >= delay) and self.in_
        return self.q

    def tof(self, signal, delay):
        self.in_ = signal
        if self.in_:
            self.start = self.time()
        self.q = self.in_ or (self.q and (self.time() - self.start) < delay)
        return self.q

    def ton_reset(self, signal, delay):
        q = self.ton(signal, delay)
        if q:
            self.reset()
        return q
