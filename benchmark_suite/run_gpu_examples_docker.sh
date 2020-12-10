#!/bin/bash

#Run the docker image that contains the examples
#Connect to visualization on localhost:5002
#If you cannot run a web browser on the local machine, use port forwarding:
#    ssh -fNT -R 5002:localhost:5002 user@foo.bar
nvidia-docker run --rm -it -p 5002:5002 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined chimbuko/run_examples:ubuntu18.04-cuda10.1-provdb
