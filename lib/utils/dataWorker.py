"""@package Chimbuko

This module implements a background worker that keep the data for VIS into a buffer, send it to
VIS and receive the response. This implementation is to allow AD to keep working on new frame
while the worker takes care of data send/receive.

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 26, 2019

Last Modified:
    April 11, 2019
"""
import threading
import requests as req
import json
import pickle

from queue import Queue

class dataWorker(object):
    """dataWorker
        background thread to take care of only IO operations including
            - requests operations
            - file writing
            - file reading (not yet support)
    """
    def __init__(self, name='dataWorker', interval=1, maxsize=0, log=None, saveType='json'):
        """
        constructor
        Args:
            name: data worker name
            interval: interval time (sec)
            maxsize: maximum buffer size (0 is for unlimited buffer)
            log: logging object to get back ground thread progress (e.g. log.info(msg))
            saveType: output file type, [json, pkl]
        """
        self.name = name
        self.interval = interval
        self.job_q = Queue(maxsize=maxsize)
        self.log = log
        self.saveType = saveType

        self._stop_event = threading.Event()

        thread = threading.Thread(target=self._run, args=())
        thread.daemon = True # daemonize thread
        thread.start()       # start the execution

    def put(self, method, path, data, rank=-1, frame_id=0):
        """Add a job in the queue"""
        self.job_q.put((method, path, data, rank, frame_id))
        if self.log is not None:
            self.log.info('[{}][{}][{}][{}] enque data: {}!'.format(
                self.name, rank, frame_id, method, path
            ))

    def join(self, force_to_quit, rank=-1):
        """
        Finalize all jobs
        Args:
            force_to_quit: if true, thread will be terminated once all jobs in the queue are done
            rank: MPI rank
        """
        if self.log is not None:
            self.log.info("[{}][{}] {} data is remained to process!".format(
                self.name, rank, self.job_q.qsize()))
        self.job_q.join()

        if force_to_quit:
            self._stop_event.set()
            if self.log is not None:
                self.log.info("[{}][{}] Forced to quit!".format(self.name, rank))

    def _run(self):
        """background process"""
        if self.log is not None:
            self.log.info('[{}] start runing ...'.format(self.name))

        while not self._stop_event.isSet():
            method, path, data, rank, frame_id = self.job_q.get()

            msg = "Success: " + path
            if method == "online":
                try:
                    r = req.post(path, json=data)
                    r.raise_for_status()
                except req.exceptions.HTTPError as e:
                    msg = "Http Error: {}".format(e)
                except req.exceptions.ConnectionError as e:
                    msg = "Connection Error: {}".format(e)
                except req.exceptions.Timeout as e:
                    msg = "Timeout Error: {}".format(e)
                except req.exceptions.RequestException as e:
                    msg = "OOps: something else: {}".format(e)
                except Exception as e:
                    msg = "Really unknown error: {}".format(e)

            elif method == "offline":
                path = path + '.' + self.saveType

                if self.saveType == 'json':
                    with open(path, 'w') as f:
                        # indenting will increase data size, but useful for debug!!!
                        #json.dump(data, f, indent=4, sort_keys=True)
                        json.dump(data, f)

                elif self.saveType == 'pkl':
                    pickle.dump(data, open(path, 'wb'))

            else:
                msg = "Unknown method."

            if self.log is not None:
                self.log.info('[{}][{}][{}] {}'.format(self.name, rank, frame_id, msg))

            self.job_q.task_done()
            self._stop_event.wait(self.interval)