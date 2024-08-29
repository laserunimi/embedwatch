import sys, re, os

from pprint import pprint

import llvmlite.binding as llvm

walk_path = "../test_svf"
funcs = ["zinit", "zbouncer"]

def count_instr(llvm_mod):
    instr_tot = 0
    zbouncer_calls = 0
    for f in llvm_mod.functions:
        for b in f.blocks:
            for i in b.instructions:
                instr_tot += 1
                if any(x in str(i) for x in funcs):
                    zbouncer_calls += 1
    return (instr_tot,zbouncer_calls)

def main():
    res = ""
    if len(sys.argv) < 2:
        lst = []
        for root, dirs, files in os.walk(walk_path):
            for file in files:
                if file.endswith(".deploy.ll"):
                    path = os.path.join(root,file)
                    print("On file: ",path)
                    mod = llvm.parse_assembly(open(path,"r").read())
                    tot,instr = count_instr(mod)
                    lst.append([file[:file.find(".")], str(tot),str(instr)])
        csv = zip(*lst)

        for line in csv:
            res += ",".join(line) + "\n"

    else:
        name = sys.argv[2]
        mod = llvm.parse_assembly(open(name,"r").read())
        tot,instr = count_instr(mod)
        res += "\n".join(name,tot,instr)

    print(res)
    open("count_instr.csv","w").write(res)
if __name__ == "__main__":
  main()
