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
      self.fileType = self.config['Parser']['FileType']
      self.inputFile = self.config['Parser']['InputFile'] # input file path
      self.parseMode = self.config['Parser']['ParseMode']
      self.stream = None
      self.status = None
      self.bpAttrib = None
      self.bpNumAttrib = None
      
      if self.fileType == "bp":
          ad.read_init("BP", parameters="verbose=3") # initialize adios streaming mode
          if self.parseMode == "stream":
              self.stream = ad.file(self.inputFile, "BP", is_stream=True, timeout_sec=10.0)
              self.bpAttrib = stream.attr
              self.bpNumAttrib = stream.nattrs
              print("Adios stream ready... \n\n\n")
              print("Attribute names: \n", self.bpAttrib)
              
              
    
    def getStream(self):
        strm = self.stream
        status = self.stream.advance()
        return strm
    
    def getStatus(self):
        return self.status
        