"""@package Chimbuko

Computing mean and standard deviation in the incremental manner
(ref: https://en.wikipedia.org/wiki/Standard_deviation)

Author(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019

Last Modified:
    April 11, 2019
"""
import numpy as np

class RunStats(object):
    """RunStats class to efficientily compute mean and standard deviation for streaming data."""

    # counting divider to avoid overflow
    # - s0 is actually equivalent to the sum of N/factor, so to get N, this value must be
    #   multipllied. As a trade-off, it will have a bit less arithmetic accuracy.
    factor=1000000

    def __init__(self, s0=0., s1=0., s2=0., n_abnormal=0.):
        """
        constructor
        Args:
            s0: initial power of sum with j == 0
            s1: initial power of sum with j == 1
            s2: initial power of sum with j == 2
            n_abnormal: the number of abnormal trace events
        """
        self.s0 = s0
        self.s1 = s1
        self.s2 = s2

        # this is to keep the number of abnormal cases
        # - n_normal: s0 - n_abnormal
        self.n_abnormal = n_abnormal

    def __str__(self):
        return "stat: s0 = {:.3f}, s1 = {:.3f}, s2 = {:.3f}".format(self.s0, self.s1, self.s2)

    def mean(self):
        """Return current mean"""
        try:
            # mn = self.s1 / self.s0
            mn = (self.s1 / self.factor) / self.s0
        except ZeroDivisionError:
            mn = 0
        return mn

    def var(self):
        """Return currrent variance"""
        try:
            # var = (self.s0*self.s2 - self.s1*self.s1) / (self.s0*self.s0)
            var = self.s0*self.s2 - self.s1*self.s1
            var = (var / self.factor) / self.s0
            var = (var / self.factor) / self.s0
        except ZeroDivisionError:
            var = np.inf
        return var

    def std(self):
        """Return current standard deviation"""
        try:
            # std = np.sqrt(max(0., self.s0*self.s2 - self.s1*self.s1)) / self.s0
            left = self.s0 * self.s2
            right = self.s1 * self.s1 / self.factor
            diff = max(0., left - right)
            std = np.sqrt(diff) / np.sqrt(self.factor) / self.s0
        except ZeroDivisionError:
            std = np.inf
        return std

    def stat(self):
        """Return power sums, (s0, s1, s2)"""
        return [self.s0, self.s1, self.s2]

    def count(self, multiply_factor=False):
        """Return counters, (s0, n_abnormal)"""
        if multiply_factor: return self.s0 * self.factor, self.n_abnormal
        return self.s0, self.n_abnormal

    def add_abnormal(self, n):
        """Add abnormal count"""
        self.n_abnormal += n

    def add(self, x):
        """Add single data point"""
        self.s0 += 1./self.factor
        self.s1 += x
        self.s2 += x*x

    def update(self, s0, s1, s2, n_abnormal=0):
        """Update statistics"""
        self.s0 += s0
        self.s1 += s1
        self.s2 += s2
        self.n_abnormal += n_abnormal

    def reset(self, s0, s1, s2, n_abnormal=None):
        """Reset statistics"""
        self.s0 = s0
        self.s1 = s1
        self.s2 = s2
        if n_abnormal is not None:
            self.n_abnormal = n_abnormal

    def reset_abnormal(self, n):
        """Reset abnormal count"""
        self.n_abnormal = n

    def __add__(self, other):
        """Add two running stat"""
        s0 = self.s0 + other.s0
        s1 = self.s1 + other.s1
        s2 = self.s2 + other.s2
        n_abnormal = self.n_abnormal + other.n_abnormal
        return RunStats(s0, s1, s2, n_abnormal)