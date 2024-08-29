import angr, claripy
import sys
import os
from pathlib import Path
import random

# docker run -it --rm -v $PWD/:/input_finder angr/angr:latest /bin/bash

def launch():

    firmware = [
    
        # ['./extra_firmware/json-parser/a.out', 'json-parser'], ##############
        ['./../test/json-parser/json-parser.normal', 'json-parser'],
        # ['./../test/hello-ptr/hello-ptr.normal', 'hello-ptr'],
        # ['./../test/ledmatrixpainter/ledmatrixpainter.normal', 'ledmatrixpainter'], ##########
        # ['./../test/rc/rc.normal', 'rc'], ###########
        # ['./../test/hello/hello.normal', 'hello'],
        # ['./../test/disco-keyboard/disco-keyboard.normal', 'disco-keyboard'], ###########
        # ['./../test/hello_uart/hello_uart.normal', 'hello_uart'], 
        # ['./../test/music-controller/music-controller.normal', 'music-controller'], 
        # ['./../test/syringePump/syringePump.normal', 'syringePump'], 
        # ['./../test/xml-parser/xml-parser.normal', 'xml-parser'] ######### non prende input

        # ['./../test/pixel_painter/pixel_painter.normal', 'pixel_painter'], # tutto in deadended
        #['./../test/clock_phone/clock_phone.normal', 'clock_phone'], ############################# da fixare
        #['./../test/mini-ig-stats/mini-ig-stats.normal', 'mini-ig-stats'],  
        #['./../test/hue-motion/hue-motion.normal', 'hue-motion'], 
    ]


    for i in range(len(firmware)):
        binary_name = firmware[i][0]
        output_name = firmware[i][1]
        main(binary_name, output_name)
    
def main(binary_name, output_name):

    input_size = [10, 20, 50, 100]
    #input_size = [1, 2, 3, 10, 50, 100]

    for size in input_size:
        p = angr.Project(binary_name, auto_load_libs=False)

        input_chars = [claripy.BVS(f'input_{i}', 8) for i in range(size)]
        #input = claripy.Concat(*([claripy.BVV(b'GET ')] + input_chars))
        input = claripy.Concat(*input_chars + [])

        
        # input_chars1 = [claripy.BVS(f'input1_{i}', 8) for i in range(size)]
        # input1 = claripy.Concat(*input_chars1 + [])

        # input_chars2 = [claripy.BVS(f'input2_{i}', 8) for i in range(size)]
        # input2 = claripy.Concat(*input_chars2 + [])

        # input_chars3 = [claripy.BVS(f'input3_{i}', 8) for i in range(size)]
        # input3 = claripy.Concat(*input_chars3 + [])
        
        #filename = 'image.png'
        #filename = 'song.mp3'
        #filename = 'svr'
        # filename1 = 'followers'
        # filename2 = 'likes'
        # filename3 = 'comments'
        #simfile = angr.SimFile(filename, content=input)
        # simfile1 = angr.SimFile(filename1, content=input1)
        # simfile2 = angr.SimFile(filename2, content=input2)
        # simfile3 = angr.SimFile(filename3, content=input3)


        init_state = p.factory.full_init_state(
            args=[f'./{binary_name}'],
            stdin=angr.SimFileStream(name='stdin', content=input, has_end=False),
            #fs={filename: simfile},
            add_options={angr.options.LAZY_SOLVES}
        )
        init_state.libc.buf_symbolic_bytes = 128

        cfg = p.analyses.CFGFast(normalize=True)
        func_name = ['hook_exit']
        addr_input = 0

        for input_function in func_name:
            for addr,func in cfg.kb.functions.items():
                if input_function in func.name:
                    print(f'addr={hex(addr)}, func={func.name}')
                    addr_input = addr

        # init_state.solver.add(input[0] == 'G')
        # init_state.solver.add(input[1] == 'E')
        # init_state.solver.add(input[2] == 'T')
        # init_state.solver.add(input[3] == ' ')

        # for c in input_chars1:
        #     init_state.solver.add(c >= ord(b'\x30'))
        #     init_state.solver.add(c <= ord(b'\x39'))

        # for c in input_chars2:
        #     init_state.solver.add(c >= ord(b'\x30'))
        #     init_state.solver.add(c <= ord(b'\x39'))

        # for c in input_chars3:
        #     init_state.solver.add(c >= ord(b'\x30'))
        #     init_state.solver.add(c <= ord(b'\x39'))


        #Create Simulation Manager
        sm = p.factory.simgr(init_state)
        sm.use_technique(angr.exploration_techniques.DFS())
        sm.use_technique(angr.exploration_techniques.LoopSeer(cfg=cfg, bound=100))

        while sm.active:
            print(f'active={len(sm.active)}')
            sm.explore(find=addr_input, enable_veritesting=True)
            #sm.explore(find=0x10bdc, enable_veritesting=True)
        
        print(f'Finished, found={len(sm.found)}, active={len(sm.active)}, deadended={len(sm.deadended)}')

        result = []

        if len(sm.found) > 0:
            for item in sm.found:
                print(f'{item.posix.dumps(0)} --> {item.posix.dumps(1)}')
                result = result + item.solver.eval_upto(input, 1, cast_to=bytes)
                print(result)
                exit(0)


        # Path(f'in/{output_name}').mkdir(parents=True, exist_ok=True)
        
        # i_path = 0
        # for r in result:
        #     f = open(f'./in/{output_name}/{output_name}_{size}_{i_path}.txt', 'wb')
        #     f.write(r)
        #     f.close()
        #     i_path = i_path + 1

if __name__ == '__main__':
    launch()