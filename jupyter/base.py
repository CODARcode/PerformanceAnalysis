# ----------------------------------------------------------------------
# Copyright (C) 2014-2015, Numenta, Inc.  Unless you have an agreement
# with Numenta, Inc., for a separate license for this software code, the
# following terms and conditions apply:
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License
# along with this program.  If not, see http://www.gnu.org/licenses.
#
# http://numenta.org/licenses/
# ----------------------------------------------------------------------

import abc
import os
import pandas
import sys

from datetime import datetime
from util import createPath, getProbationPeriod

# class NAB_Dataset:
#     def __init__(self, df):
#         super().__init__()
#         self.data = df.rename(columns={'run_time':'value'})
#         try:
#             self.func_name = df.name
#         except:
#             self.func_name = None
        

class AnomalyDetector(object, metaclass=abc.ABCMeta):
  """
  Base class for all anomaly detectors. When inheriting from this class please
  take note of which methods MUST be overridden, as documented below.
  """

  def __init__(self,
            data,
                probationaryPercent = 0.1, *args, **kwargs):

    self.data = data
    self.probationaryPeriod = getProbationPeriod(
      probationaryPercent,self.data.shape[0])

    self.inputMin = self.data["run_time"].min()
    self.inputMax = self.data["run_time"].max()
    self.results = None
    

  def initialize(self):
    """Do anything to initialize your detector in before calling run.

    Pooling across cores forces a pickling operation when moving objects from
    the main core to the pool and this may not always be possible. This function
    allows you to create objects within the pool itself to avoid this issue.
    """
    pass

  def getAdditionalHeaders(self):
    """
    Returns a list of strings. Subclasses can add in additional columns per
    record.

    This method MAY be overridden to provide the names for those
    columns.
    """
    return []


  @abc.abstractmethod
  def handleRecord(self, inputData):
    """
    Returns a list [anomalyScore, *]. It is required that the first
    element of the list is the anomalyScore. The other elements may
    be anything, but should correspond to the names returned by
    getAdditionalHeaders().

    This method MUST be overridden by subclasses
    """
    raise NotImplementedError


  def getHeader(self):
    """
    Gets the outputPath and all the headers needed to write the results files.
    """
    headers = ["timestamp",
                "value",
                "anomaly_score"]

    headers.extend(self.getAdditionalHeaders())

    return headers


  def run(self):
    """
    Main function that is called to collect anomaly scores for a given file.
    """

    headers = self.getHeader()

    rows = []
    for i, row in self.data.iterrows():

      inputData = row.to_dict()

      detectorValues = self.handleRecord(inputData)

      # Make sure anomalyScore is between 0 and 1
      if not 0 <= detectorValues[0] <= 1:
        raise ValueError(
          f"anomalyScore must be a number between 0 and 1. "
          f"Please verify if '{self.handleRecord.__qualname__}' method is "
          f"returning a value between 0 and 1")

      outputRow = list(row) + list(detectorValues)
    
      rows.append(outputRow)

      # Progress report
    #   if (i % 1000) == 0:
    #     print(".", end=' ')
    #     sys.stdout.flush()

    try:
        self.results = pandas.DataFrame(rows, columns=headers)
    except:
        self.results = pandas.DataFrame(rows)
    return self.results


def detectDataSet(args):
  """
  Function called in each detector process that run the detector that it is
  given.

  @param args   (tuple)   Arguments to run a detector on a file and then
  """
  (i, detectorInstance, detectorName, labels, outputDir, relativePath) = args

  relativeDir, fileName = os.path.split(relativePath)
  fileName =  detectorName + "_" + fileName
  outputPath = os.path.join(outputDir, detectorName, relativeDir, fileName)
  createPath(outputPath)

  print("%s: Beginning detection with %s for %s" % \
                                                (i, detectorName, relativePath))
  detectorInstance.initialize()

  results = detectorInstance.run()

  # label=1 for relaxed windows, 0 otherwise
  results["label"] = labels

  results.to_csv(outputPath, index=False)

  print("%s: Completed processing %s records at %s" % \
                                        (i, len(results.index), datetime.now()))
  print("%s: Results have been written to %s" % (i, outputPath))

