"""
Parses files (currently .bp)
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import adios as ad
import configparser


class Parser():
    def __init__(self, configFile):
      self.config = configparser.ConfigParser()
      self.config.read(configFile)
      self.fileType = config['Parser']['FileType']
      self.inputFile = config['Parser']['InputFile'] # input file path
      self.parseMode = config['Parser']['ParseMode']
      self.stream = None
      self.status = None
      
      if fileType == "bp":
          ad.read_init("BP", parameters="verbose=3") # initialize adios streaming mode
          if self.parseMode == "stream":
              self.stream = ad.file(inputFile, "BP", is_stream=True, timeout_sec=10.0)
              
    
    def getStream(self):
        strm = self.stream
        status = self.stream.advance()
        return strm
    
    def getStatus(self):
        return self.status
        