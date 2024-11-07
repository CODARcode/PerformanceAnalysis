# Dockerfiles

## `Dockerfile.spack_dev_ddb`

This Dockerfile defines an image which can be used to validate the visualization
package upgrades. The build will clone the `chimbuko-visualization2` (CV2) repository
and checkout the `dependency_upgrades` branch. Here is how to use this image to
test changes to CV2:

1. Build the image.
2. Use `docker ps` to find an open port `HOST_PORT`.
3. Then running the image with the `HOST_PORT` will execute `run.sh` and start
    the server simulation.
4. Load the frontend in a browser (`http://hostname:$HOST_PORT`).

### Build the image.

```sh
docker build \
    -f docker/ubuntu18.04/openmpi4.0.4/all_spack/Dockerfile.spack_dev_ddb \
    -t chimbuko/chimbuko-spack-dev-ddb:ubuntu18.04 \
    .
```

### Run the image.

```sh
docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
    -p $HOST_PORT:5002 \
    -v $(pwd):$(pwd) \
    -name ddb-csd \
    chimbuko/chimbuko-spack-dev:ubuntu18.04
```

