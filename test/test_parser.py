import os
import sys
import parser
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
        
        prs = parser.Parser("test.cfg")
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
    
    
    
    
    
    
    
    
