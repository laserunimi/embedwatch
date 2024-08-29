# Embedwatch
Binary protection against user-controlled memory errors enforced with ARM trustzone.

## Prerequisite
* Install SVF on your own machine: https://github.com/SVF-tools/SVF/wiki/Set-up-the-Debugging-Environment-for-SVF-in-VSCode
* Use SVF checkout at `1c09651a6c4089402b1c072a1b0ab901bc963846` (this version use LLVM 13.0.0)
* In test/Makefile canche the variable `PATH_2_REACH_SVF = your_path_here with` the path of SVF main folder

## Getting started
Run test:
_Generate library wrapper_
* `your-pc$ docker run -it -v $PWD:/root/zbouncer --rm teozoia/optee32:latest`
* `docker-optee32$ cd ~/zbouncer/build`
* `docker-optee32$ make optee32-lib`

_LLVM compile all tests_
* `your-pc$ docker run -it --rm -v $PWD/:/root/zbouncer teozoia/llvm-cross-x86_64-arm:latest /bin/bash`
* `docker-llvm$ cd ~/zbouncer/test`
* `docker-llvm$ make test-compile`

_SVF analysis on your machine (you need to install SVF before)_
* `your-pc$ cd Zbouncer/test` (directory which contains tests)
* `your-pc$ make instr-svf` (You need to specify path of SVF in Zbouncer/test/Makefile)

_Instrumentation with tz_
* `your-pc$ docker run -it --rm -v $PWD/:/root/zbouncer teozoia/llvm-cross-x86_64-arm:latest /bin/bash`
* `docker-llvm$ cd ~/zbouncer/test_svf`
* `docker-llvm$ make instr-zbouncer`

_Execute instrumented binary inside qemu-optee-os arm32_
* `your-pc$ docker run -it --network=host --env DISPLAY=$DISPLAY --privileged -v /tmp/.X11-unix:/tmp/.X11-unix -v $PWD:/root/zbouncer --rm teozoia/optee32:latest`
* `docker-optee32$ cd ~/zbouncer/build`
* `docker-optee32$ make optee32-run`
* Do this if there is an error for opteeos emulation `your-pc$ xhost + (ubuntu only)` then `docker-optee32$ make optee32-run`

Running the verifier 
* `your-pc$ cd verifier`
* `your-pc$ python -m venv .venv`
* `your-pc$ pip install -r requirements.txt`
* `your-pc$ source .venv/bin/activate`
* `your-pc$ ./wrapper_verifier.sh <test-name>` (e.g. `your-pc$ ./wrapper_verifier.sh hello`)
* _Now the verifier is ready to receive data_
* Inside qemu normal world terminal run `qemu-normal-world$ <test-name>`

---

### Issue and errors
* https://github.com/OP-TEE/optee_os/issues/4649
* https://optee.readthedocs.io/en/latest/building/prerequisites.html#prerequisites + git wget cpio python-is-python3 + pip3 install pycrytpodomex
* sh: - not found -> fix by copy library https://unix.stackexchange.com/questions/18061/why-does-sh-say-not-found-when-its-definitely-there
