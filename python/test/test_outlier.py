import sys
import os
import configparser
import pickle
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import outlier
import unittest


class TestCompOutlier(unittest.TestCase):
    """Test outlier computation.
    """
    
    def setUp(self):
        """Setup scaled function execution time data (x-axis (timestamp), y-axis (function execution time)) 
        """
        # Setup data that has outliers
        passData = np.zeros((10,7))
        np.random.seed(1)
        passData[:,4] = np.random.uniform(0,1,10)
        passData = passData[passData[:,4].argsort(kind='mergesort')]
        passData[:,5] = [0.0096, 0.0067758, 0.01, 0.0079, 0.00098, 0.2, 0.0893986, 0.00967758, 0.000961595, 0.00967758]
       
        pfile = "passFunTime.pickle"
        with open(pfile, 'wb') as handle:
            pickle.dump(passData, handle, protocol=pickle.HIGHEST_PROTOCOL)

        # Setup data that does not have any outliers
        failData = np.zeros((10,7))
        failData[:,4] = np.array([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0])
        failData[:,5] = np.array([0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5])
        
        pfile = "failFunTime.pickle"
        with open(pfile, 'wb') as handle:
            pickle.dump(failData, handle, protocol=pickle.HIGHEST_PROTOCOL)
        

    def tearDown(self):
        """Delete pickle files
        """
        pfile = "passFunTime.pickle"
        os.remove(pfile)
        pfile = "failFunTime.pickle"
        os.remove(pfile)
        
    
    def test_compOutlier(self):
        """Test outlier detection.
        """
        
        pfile = "passFunTime.pickle"
        with open(pfile, 'rb') as handle:
            passData = pickle.load(handle)

        config = configparser.ConfigParser(interpolation=None)
        config.read("test.cfg")
        
        passOtl = outlier.Outlier(config)
        funId = 0
        passOtl.compOutlier(passData, funId)
        passFunOutl = np.array(passOtl.getOutlier())       
        passFunOutlId = passData[passFunOutl == -1][:,6]
        passNumOutl = len(passFunOutlId)
        
        self.assertEqual(passNumOutl, 1)
    
        pfile = "failFunTime.pickle"
        with open(pfile, 'rb') as handle:
            failData = pickle.load(handle)
        
        failOtl = outlier.Outlier(config)
        funId = 0
        failOtl.compOutlier(failData, funId)
        failFunOutl = np.array(failOtl.getOutlier())     
        failFunOutlId = failData[failFunOutl == -1][:,6]
        failNumOutl = len(failFunOutlId)
        
        self.assertEqual(failNumOutl, 0)
        
        # Plot if needed
        #=======================================================================
        # fig = plt.figure()
        # plt.subplots_adjust(hspace=0.75)
        #
        # ax1 = fig.add_subplot(211)
        # ax1.set_xlabel('Scaled entry timestamp')
        # ax1.set_ylabel('Scaled function exec. time')
        # ax1.scatter(passData[:,4], passData[:,5] , alpha=0.8, c="red", edgecolors='none', s=30, label="data")
        # passMuv = np.full(10, np.mean(passData[:,5]))
        # ax1.plot(passData[:,4], passMuv, color='blue', linestyle='-', linewidth=2, label="mean")
        # ax1.set_title('Pass Test Data')
        # ax1.legend(loc=0)
        # 
        # ax2 = fig.add_subplot(212)
        # ax2.set_xlabel('Scaled entry timestamp')
        # ax2.set_ylabel('Scaled function exec. time')
        # ax2.scatter(failData[:,4], failData[:,5] , alpha=0.8, c="red", edgecolors='none', s=30, label="data")
        # failMuv = np.full(10, np.mean(failData[:,5]))
        # ax2.plot(failData[:,4], failMuv, color='blue', linestyle='-', linewidth=2, label="mean")
        # ax2.set_title('Fail Test Data')
        # ax2.legend(loc=0)
        # 
        # plt.show()
        #=======================================================================

if __name__ == '__main__':
    unittest.main()
      
#if __name__ == '__main__':
#    compOutlier = TestCompOutlier()
#    compOutlier.setUp()
#    compOutlier.test_compOutlier()
    
    
    
    
    
    
    
    
    
