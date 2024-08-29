import os

dir_test = 'test/'
dir_bin = 'Debug-build/bin/svf-ex'
list_ll = []

from pathlib import Path
import subprocess

for path in Path(dir_test).rglob('*.wrapped.ll'):
    list_ll.append(str(path))

print(list_ll)
trace_count = []

for x in list_ll:
    print(x)
    #os.system("./" + dir_bin + " " + x + " --pta-ander")
    #os.system("./" + dir_bin + " " + x + " --pta-fs")
    os.system("./" + dir_bin + " " + x + " --pta-dda")