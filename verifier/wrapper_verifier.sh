#!/bin/sh

python -u verifier.py ../test_svf/$1/$1.wrapped.ll.dda.ztrace ../test_svf/$1/$1.wrapped.svf.deploy.ll
