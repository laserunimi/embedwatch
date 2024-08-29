import angr, claripy, r2pipe, pyvex, importlib, os, traceback
import sys
from pprint import pprint

bin_name = None 
out_dir = sys.argv[2] if len(sys.argv) >= 3 else "./" 

p = None  
solution = []

def write_to_file(p, solution, name = "",outdir="./"):
    for i,item in enumerate(solution):
        with open( outdir + os.path.basename(p.filename) + "_" + name +"_input_" + str(i), "ab") as f:
            word = item + b"\n"
            print(word)
            f.write(word)
            f.close()

def init_sm():
    input_v = claripy.BVS('input', 64)
    init_state = p.factory.full_init_state(
          args=[ bin_name],
          stdin=input_v,
    )

    return (input_v ,p.factory.simgr(init_state))

def get_input(address,input_v,sm):
    try:
        sm.explore(find=address)
        found = sm.found[0]
        return found.solver.eval_upto(input_v, 10, cast_to=bytes)
    except Exception as e:
        traceback.print_exc()
        print("[Error] Finding input for address ", hex(address))

    
def myfunc():

    mod = importlib.__import__("r2_getzlines" if len(sys.argv) < 4 else sys.argv[3], globals=globals())

    input_v,sm = init_sm()

    addresses = mod.get_addresses(bin_name)
    pprint({f : [hex(x) for x in l] for f,l in addresses.items()})

    for f in addresses.keys():
        for address in addresses[f]:
            print(f"Finding input for {f} : {hex(address)}")
               #print(repr(solution))
            solution = get_input(address,input_v,sm)
            #write angr out to file
            if solution:
                write_to_file(p, solution, str(hex(address)), out_dir)

if __name__ == '__main__':
    bin_name = sys.argv[1]
    p = angr.Project(bin_name, main_opts={'backend':'elf', 'arch': 'armhf', 'base_addr': 0x0}, auto_load_libs=False)
    myfunc();


