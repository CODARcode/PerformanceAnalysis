
import sys
import configparser
import numpy as np
import outlier.py
import unittest


class TestCompOutlier(unittest.TestCase):
    """Test outlier computation.
    """
    
    def setUp(selfs):
        """Setup function call trace data
        """
        