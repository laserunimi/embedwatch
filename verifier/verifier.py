import threading, queue, socket, sys, re

from pprint import pprint
from pympler.asizeof import asizeof

import llvmlite.binding as llvm

import time
import datetime
import psutil
import copy


class Trace_line:
    ident = 0
    def __init__(self,traceline):
        values = traceline.split(";")

        self.id = int(values[0])
        self.type = values[1]
        self.father = int(values[2]) if values[2] != '' else None
        self.info = values[-1]

        if("DEF" in self.type):
            self.cls = values[3]
            self.length = {}
            self.length[0] = int(values[4])
            
            if self.type == "SDEF":
                self.offset = int(values[5])
                self.fields = eval(values[6])
        else:
            self.cls = None
            self.length = None
        
        if("USE" in self.type):
            self.fun = values[3]
        else:
            self.fun = None

        self.children = []
        self.address = {}

    def get_len(self, gdef):
        if "DEF" not in self.type:
            raise ValueError("Cannot get length of something that isn't a definition")
        
        return self.length[gdef] if gdef in self.length.keys() else self.length[0]
    
    def get_base(self, frame_counter):
        if "DEF" not in self.type:
            raise ValueError("Cannot get base address of something that isn't a definition")
        
        if frame_counter not in self.address.keys():
            frame_counter = 0

        try:
            return self.address[frame_counter] + (self.offset if self.type == "SDEF" else 0)
        except KeyError:
            return 0

    def __repr__(self):
        from pprint import pformat
        out = ("{type}:\n{ident_base}- Id: {id}" + \
              "\n{ident_base}- Info:{info}"+ \
              "\n{ident_base}- Father: {father}"
              "\n{ident_base}- Len: {length}"+ \
              "\n{ident_base}- Children:\n{ident_children}" ).format(\
                   ident_base = "\t" * (Trace_line.ident + 1), \
                   type = self.type, id = self.id, info = self.info, \
                   length = self.length if self.length else 0, \
                   ident_children="\t" * (Trace_line.ident + 2), \
                   father = self.father)
            #f"{self.type}:\n{'\t' * Trace_line.ident + 1  }- Id: {self.id}\n{'\t' * Trace_line.ident + 1}- Info:{self.info}\n{'\t' * Trace_line.ident + 1}- Len: {self.length if self.length else 0}\n{'\t' * Trace_line.ident + 1}- Children:\n{'\t' * Trace_line.ident  + 2}"

        Trace_line.ident += 1
        out += pformat(self.children,indent = 3 + Trace_line.ident)
        Trace_line.ident -= 1

        return out



data_queue = queue.Queue()

def_map = dict()
static_trace = []
nodes = []
mod = None
mod_code = None
tlen = 0
mean_tlen = 0
tcount = 0
entry_count = {}
current_trace = None
time_local_list = [] # questa è quella che alloca
time_global = None

hstats_tlen = None
hstats_mean_tlen = None
hstats_tcount = 0

total_bytes_recv = 0
usedef_dump_data_points = [] # questa no? boh questa sembra vuota boh no in realtà non ricordo
start_time = time.time()

def debug_decorator(fun):
    def wrapper(*args,**kwargs):
        print(f"[DEBUG] Calling {fun.__name__}")
        print(args)
        print(kwargs)

        return fun(*args,**kwargs)
    return wrapper

def trace_receiver(port, ip):
    global total_bytes_recv
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
          s.bind((ip,port))
          s.listen()
    
          while True:
            conn,addr = s.accept()
            data = b""
            with conn:
              while True:
                d = conn.recv(1024)
                if not d:
                  break
                data += d
              total_bytes_recv += len(data)
              data_queue.put(data)
              conn.close()
    except Exception as e:
        print(f"Got exception: {e}")
# def leading_tabs(line):
#     return len(line) - len(line.lstrip("\t"))

def trace_parser(trace):
    for line in trace.strip().split("\n"):
        print(line)
        tl = Trace_line(line.strip())
        nodes.append(tl)

        if "DEF" in tl.type:
            if tl.id in def_map.keys():
                def_map[tl.id].append(tl)
            else:
                def_map[tl.id] = [tl]

        if tl.father == -1:
            static_trace.append(tl)
        else:
            father = next(t for t in nodes if t.id == tl.father)
            father.children.append(tl)

def get_meta_value(_,instr,name):
    global mod
    
    for value in str(mod).split('\n'):
      pass

def find_instr(id_num,llvm_mod,func=None):
    global mod
    print(f"Searching for {id_num}")
    types = ['zdef','zsuse','zluse']

    for i in mod.global_variables:
        for name in types:
            try:
                of = str(i)[str(i).index(name):].split(' ')[1].strip().rstrip(",")
                #print(f"Found {str(i)} with value {of}")

                for x in str(mod).split('\n'):
                    #if  of + " =" in x:
                    #    print("Metadata is:",x)
                    if of + " =" in x and str(id_num) in x.split("=")[1]:
                        #print(f"Found {name} in {str(value)} with value: {x}")
                        return i
            except:
                pass
            
    for f in mod.functions:
        #Iter over functions printed (Fa schifo ma llvmlite è osceno)
        for l in str(f).split("\n"):
            for name in types:
                try:
                    of = l[l.index(name):].split(' ')[1].strip().rstrip(",")
                except Exception as e:
                    continue
               # print(f"Found {l} with value {of}")
                for n1,x in enumerate(str(mod).split('\n')):
                    #if " = !{" in x:
                    #    print(x, of +" =" in x, of + " =")
                    #if  of + " =" in x:
                    #    print("Metadata is:",x, x.split("=")[1], str(id_num),str(id_num) in x.split("=")[1])
                    if of + " =" in x and (str(id_num) in x.split("=")[1]):
                        #print("AAAAAAAAAAAAAAAA")
                        for bb in f.blocks:
                            for n2,i in enumerate(bb.instructions):
                                instr = re.sub(",\s+!z[isld].*$","",str(i)).strip()
                                if instr in l and n2 in range(max(0,n2-10),n2+10):
                                    return i
                
    print(id_num,"not found")

                


def check_use(def_struct : Trace_line,use_struct,end_address, gdef):
    print(f"Checking use of\n\t- Begin: {hex(def_struct.get_base(gdef))}\n\t- End: {hex(end_address)}\n\t- Len: {def_struct.get_len(gdef)}\n\t- Diff: {end_address - def_struct.get_base(gdef)}")
    return (end_address - def_struct.get_base(gdef)) > (def_struct.get_len(gdef))

def my_print(*args):
    global current_trace
    
    print(*args)
    
    if not current_trace:
        return
    
    with open(current_trace + ".log","a") as f:
        for x in args:
            f.write(x if type(x) == str else str(x))
        f.write("\n")

def get_id_var(x : int):
    return (x >> 16, x & 0x0000ffff)

def verify_input(trace):
  global tlen
  global mean_tlen
  global tcount
  global entry_count
  global current_trace
  global time_local_list
  global time_global
  global usedef_dump_data_points
  global hstats_tlen
  global hstats_mean_tlen
  global hstats_tcount
  global start_time
  
  tz = datetime.timezone.utc
  ft = "%Y-%m-%dT%H:%M:%S%z"
  t = datetime.datetime.now(tz=tz).strftime(ft)

  formatted_trace = [ [x.strip() for x in line.split("|")] for line in (str(trace)).strip().split("\n") if line != ""]

  pprint(formatted_trace)
  
  hstats_tcount += 1
  hstats_tlen = len(formatted_trace)

  for entry in formatted_trace:  
      time_local =  time.perf_counter()

      #pprint(entry)
      
      use_gdef = None
      
      if entry[0] not in ["INFO","TRACE"]:
        if entry[0] in entry_count.keys():
            entry_count[entry[0]] = entry_count[entry[0]] + 1
        else:
            entry_count[entry[0]] = 1
          
      if entry[0] == "TRACE":
          current_trace = entry[1]
          
      hstats_tlen = tlen
      hstats_mean_tlen = mean_tlen
          
      if entry[0] == "INFO":
        if tlen is not None:
            my_print("Previous Trace lenght: ",tlen)
            my_print("Mean Trace Lenght: ", mean_tlen)
            my_print("Total traces received: ", tcount)
        
        if len(entry) > 1 and entry[1] != "":
            my_print(entry[1])

        for key, count in entry_count.items():
            my_print(key,"\t",count)
            entry_count[key] = 0
            
        mean_tlen = (tlen + (mean_tlen * tcount)) / (tcount + 1)
        tcount += 1
        tlen = 0

        # TIMING
        if(len(time_local_list) != 0):
            my_print("Average entry processing time: {:.20f}".format(sum(time_local_list) / len(time_local_list)))
        time_local_list = []

        if time_global is not None:
            my_print("Elapsed time for complete execution: {:.20f}".format(time.perf_counter() - time_global))
        time_global = time.perf_counter()
        
        my_print("Total bytes: {}".format(total_bytes_recv))


      else:
        tlen += sum(len(x) for x in entry)

      if entry[0] == "DEF":
          #usedef_dump_data_points.append((t, 'DEF'))
          (id, gdef) = get_id_var(int(entry[1]))
          for df in def_map[id]:
              df.address[gdef] = int(entry[2],16)
        
      if entry[0] == "DDEF":
          #usedef_dump_data_points.append((t, 'DDEF'))
          (id, gdef) = get_id_var(int(entry[1]))
          for df in def_map[id]:
              #df.children[0][gdef].address = int(entry[2],16)
              #df.children[0][gdef].length  = int(entry[3])
              df.address[gdef] = int(entry[2],16)
              df.address[0] = int(entry[2],16)
              df.length[gdef] = int(entry[3])
              df.length[0] = int(entry[3])

      if entry[0] == "USE":
          #usedef_dump_data_points.append((t, 'USE'))
          (use_id,gdef) = get_id_var(int(entry[1]))
          end_addr = int(entry[2],16)

          use_struct = next(x for x in nodes if x.id == use_id)
          
          #assert len(use_struct.children) == 1

          def_struct = use_struct.children[0]

          if len(use_struct.children) > 1:
              def_struct= next(x for x in use_struct.children if x.cls == "BASE")

          if def_struct.type == "DDEF":
            new_def = copy.deepcopy(def_struct.children[0])
            new_def.address = def_struct.address
            new_def.lenght = def_struct.length

            def_struct = new_def
        
          use_gdef = gdef

      elif entry[0] == "I-USE":
          #usedef_dump_data_points.append((t, 'IUSE'))
          (use_id,use_gdef) = get_id_var(int(entry[2]))
          (def_id, def_gdef) = get_id_var(int(entry[1]))
          end_addr = int(entry[3],16)

          use_struct = next( x for x in nodes if x.id == use_id )
          def_struct = def_map[def_id][0]

          #if def_struct.type == "DDEF":
          #  def_struct = def_struct.children[0]
          if def_struct.type == "DDEF":
            new_def = copy.deepcopy(def_struct.children[0])
            new_def.address = def_struct.address
            new_def.lenght = def_struct.length

            def_struct = new_def

          use_gdef = def_gdef

      if "USE" in entry[0] and check_use(def_struct, use_struct, end_addr, use_gdef):
          #usedef_dump_data_points.append((t, 'USE'))
          i_use = find_instr(use_struct.id,mod, use_struct.fun)
          i_def = None

          for deflist in def_map.values():
            for def_v in deflist:
                if def_v.info == def_struct.info:
                    temp = find_instr(def_v.id,mod) 

                    if temp is not None:
                        i_def = temp  
          
          if i_use is None or i_def is None:
            print("Cannot find informations about:\n\t - {}\n\t - {}\n in llvm module".format(use_struct.info, def_struct.info))
          else:
            source = f"in function {i_def.function.name}" if i_def.function else "(global variable) "
            def_str = re.sub(",\s+!z[isld].*$","",str(i_def)).strip()
            use_str = re.sub(",\s+!z[isld].*$","",str(i_use)).strip()
            print(f"VIOLATION\nOverflow of data defined at {def_str} {source} cause by instruction {use_str} in function {i_use.function.name}")
      
      use_gdef = None

      time_local_list.append(time.perf_counter() - time_local)
      
      #print('size usedef dump: {}'.format(len(usedef_dump_data_points)))
      
      with open("24_def_map_size.txt","a") as f:

        def_size = asizeof(def_map)
        f.write("{},{},{},{}\n".format(def_size,memory(),resident(),stacksize()))
      if len(usedef_dump_data_points) > 200:
          with open("24h_usedef_data_points.csv", "a") as myfile:
            for x,y in usedef_dump_data_points:
              myfile.write('{};{}\n'.format(x, y))
          usedef_dump_data_points = []
          # A posto questa la liberi ogni 200 quindi easy
          # vediamo l'altra
          memusage = memory()
          h_line = '{};{};{};{};{};{};{};{};{};{};{};{};{};{}\n'.format(t,
            memusage,
            resident(),
            stacksize(),
            total_bytes_recv,
            hstats_tlen,
            hstats_tcount,
            sum(time_local_list) / len(time_local_list),
            len(def_map),
            len(static_trace),
            len(entry_count),
            len(time_local_list),
            len(usedef_dump_data_points),
            sys.getsizeof(time_local_list))

          my_print("----- STATS -----\nmemusage: {}".format(memusage))
          my_print("Total bytes: {}".format(total_bytes_recv))
          my_print("Previous Trace lenght: ",tlen)
          my_print("Total traces received: ", hstats_tcount)
          my_print("Average entry processing time: {}".format(sum(time_local_list) / len(time_local_list)))
          my_print("def_map: ", len(def_map))
          my_print("sizeof time_local_list: ", sys.getsizeof(time_local_list))
          
          with open("24h_stats.csv", "a") as myfile:
            myfile.write(h_line)

import os
_proc_status = '/proc/%d/status' % os.getpid()

_scale = {'kB': 1024.0, 'mB': 1024.0*1024.0, 'KB': 1024.0, 'MB': 1024.0*1024.0}

def _VmB(VmKey):
    '''Private.'''
    global _proc_status, _scale
     # get pseudo file  /proc/<pid>/status
    try:
        t = open(_proc_status)
        v = t.read()
        t.close()
    except:
        return 0.0  # non-Linux?
     # get VmKey line e.g. 'VmRSS:  9999  kB\n ...'
    i = v.index(VmKey)
    v = v[i:].split(None, 3)  # whitespace
    if len(v) < 3:
        return 0.0  # invalid format?
     # convert Vm value to bytes
    return float(v[1]) * _scale[v[2]]

def memory(since=0.0):
    '''Return memory usage in bytes.'''
    return _VmB('VmSize:') - since

def resident(since=0.0):
    '''Return resident memory usage in bytes.'''
    return _VmB('VmRSS:') - since

def stacksize(since=0.0):
    '''Return stack size in bytes.'''
    return _VmB('VmStk:') - since

def main():
  global mod
  global mod_code

  tr = threading.Thread(target=trace_receiver,args=(8080,"0.0.0.0"))
  tr.start()

  trace_parser(open(sys.argv[1],"r").read())
  mod_code = open(sys.argv[2],"r").read()
  mod = llvm.parse_assembly(mod_code)
  #print(mod_code)

  while True:
    trace = data_queue.get().decode()
    #print("[DEBUG] Received data: ", trace)
    verify_input(trace)




if __name__ == "__main__":
  main()
  #trace_parser(open(sys.argv[1],"r").read())
  #for x in static_trace:
  #  print(x)
  # mod = llvm.parse_assembly(open(sys.argv[2],"r").read())

  # for inp in open(sys.argv[3],"r").read().strip().split("\n"):
  #     print("Received line " + inp)
  #     if(inp.strip() != ""):
  #       verify_input(inp)
