import angr, claripy, r2pipe, pyvex, importlib
import sys
from pprint import pprint

zbouncer_funcs = ["zinit","zbouncer_collect_alloca","zbouncer_use","zbouncer_luse","zbouncer_iuse","zbouncer_iluse"]


def run_command(rad_inst,cmd):
    return rad_inst.cmd(cmd)

def get_lines(rad_inst,res):
    function_list = run_command(rad_inst,"afl")
    ret = []
    
    for entry in res.split("\n"):
        s = entry.split()

        if len(s) < 2:
            continue

        if s[0] + "_instrumented" not in function_list:
            ret.append(entry)
    
    return ret


def get_func_to_instr(bin_name):
    r = r2pipe.open(bin_name)
    
    run_command(r,"aaaa")
    func_address = dict()
    
    for f in zbouncer_funcs:
        run_command(r,"s sym." + f)
        res = run_command(r,"axt")
        func_address[f] = []
        for ref in get_lines(r,res):
            s = ref.split()
            if(len(s) < 2):
                continue
    
            if s[0].strip() in zbouncer_funcs:
                continue
    
            func_address[f].append(int(s[1].strip(),16))
    
    return func_address

def get_addresses(bin_name):
    return get_func_to_instr(bin_name)
