import os
import sys
import configparser
from parser2 import Parser
import unittest


class TestGetStream(unittest.TestCase):
    """Test parser for correctly catching the Adios stream.
    """
    
    #def tearDown(self):
    #    """Delete log files
    #    """
    #    logFile = "test.log"
    #    os.remove(logFile)    
      
    def test_getStream(self):
        """Test getStream() function, which performs an Adios advance operation.
        """

        config = configparser.ConfigParser(interpolation=None)
        config.read("test.cfg")

        config['Adios']['Method'] = 'BP'
        config['Adios']['InputFile'] = '../data/nwchem/20180801/tau-metrics.bp'

        prs = Parser(config)
        parseMethod = prs.getParseMethod()
        
        ctrl = 0
        count = 0
        while ctrl >= 0:
            prs.getStream()
            ctrl = prs.getStatus()
            count += 1
        
        self.assertEqual(count, 11)


if __name__ == '__main__':
    unittest.main()    
    
    
    
    
    
    
    
    
