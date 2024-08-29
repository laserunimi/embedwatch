import os
from pathlib import Path

dir_test = 'test/'
dir_bin = 'Debug-build/bin/svf-uaf'
list_ll = []

for path in Path(dir_test).rglob('*.wrapped.ll'):
    list_ll.append(str(path))

for x in list_ll:
    if x.startswith('uaf_'):
        print(x)
        os.system("./" + dir_bin + " " + x + " --pta-dda")