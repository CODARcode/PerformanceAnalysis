"""@package Chimbuko
This module implements a background worker that keep the data for VIS into a buffer, send it to
VIS and receive the response. This implementation is to allow AD to keep working on new frame
while the worker takes care of data send/receive.

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 26, 2019
"""
import threading
import requests as req
import json
#import time

from queue import Queue

class dataWorker(object):
    def __init__(self, name='dataWorker', interval=1, maxsize=0, log=None):
        self.name = name
        self.interval = interval
        self.job_q = Queue(maxsize=maxsize)
        self.log = log

        self._stop_event = threading.Event()

        thread = threading.Thread(target=self._run, args=())
        thread.daemon = True # daemonize thread
        thread.start()       # start the execution

    def put(self, method, path, data):
        self.job_q.put((method, path, data))

    def join(self, force_to_quit):
        self.job_q.join()
        if force_to_quit:
            self._stop_event.set()

    def _run(self):
        while not self._stop_event.isSet():
            method, path, data = self.job_q.get()

            msg = "Success"
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
                with open(path, 'w') as f:
                    json.dump(data, f, indent=4, sort_keys=True)
            else:
                msg = "Unknown method."

            if self.log is not None:
                self.log.info('[{:s}]'.format(self.name) + msg)

            self.job_q.task_done()
            #time.sleep(self.interval)
            self._stop_event.wait(self.interval)