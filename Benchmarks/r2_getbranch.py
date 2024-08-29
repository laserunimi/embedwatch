import angr, claripy, r2pipe, pyvex, json, functools
import sys
from pprint import pprint

zbouncer_funcs = ["zinit","zbouncer_collect_alloca","zbouncer_use","zbouncer_luse","zbouncer_iuse","zbouncer_iluse"]
visited_funcs = ["main"]


def run_command(rad_inst,cmd):
    return rad_inst.cmd(cmd)


def is_valid(fun_name):
    return fun_name.split(".")[1].strip() not in zbouncer_funcs and \
            fun_name not in visited_funcs and \
            not fun_name.strip().startswith("sym.imp") and \
            not fun_name.strip().startswith("sym.TEEC") and \
            not fun_name.strip().startswith("sym.teec") 

def update_addr(dictionary,rad_inst,fun):
    #print(f"Analyzing {fun}")

    run_command(rad_inst,"s " + fun)

    bbs = run_command(rad_inst,"afb").split("\n")

    dictionary[fun] = []

    #print("bbs:",bbs)

    for bb in bbs:
        if not bb:
            continue
        
        dictionary[fun].append(int(bb.strip().split()[0],16))

    called_funcs = []

    r = json.loads(run_command(rad_inst,"agcj"))

    return functools.reduce(lambda x,y: x + y, [x['imports'] for x in r],[]) 



def get_addresses(bin_name):
    rad_inst = r2pipe.open(bin_name)
    run_command(rad_inst,"aaaa")

    addresses = dict()


    funcs = ["main"]

    while funcs:
        tmp = []
        for f in funcs:
            tmp += [x for x in update_addr(addresses,rad_inst,f) if is_valid(x)]
            #print(tmp)
            visited_funcs.append(f)

        funcs = tmp

    return addresses


