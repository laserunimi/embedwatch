CC = /usr/local/cross_armhf_clang_9.0.0/bin/clang
OPT = /usr/local/cross_armhf_clang_9.0.0/bin/opt
LLC = /usr/local/cross_armhf_clang_9.0.0/bin/llc
DIS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-dis
LINK = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-link
AS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-as
LIB_LLVM = /root/llvm_all/build_llvm/lib
LLVM_TRANSFORMS = /root/llvm_all/llvm/lib/Transforms
PASS_NAME = zbouncer
PASS_INSTR = FunctionInstrumentor
PASS_WRAPPER = InputWrapper
WRAPPED_FN = wrapped-fn

ROOT_BUILD = ..

FILE_NAME = hello
C_FILE_NAME = $(FILE_NAME).c
BC_FILE_NAME = $(FILE_NAME).bc
NATIVE_FILE_NAME = $(FILE_NAME).native
INSTR_FILE_NAME = $(FILE_NAME).instr
WRAPPED_FILE_NAME = $(FILE_NAME).wrapped
ASM_FILE_NAME = $(FILE_NAME).S
BC_WRAPPED_FILE_NAME = $(WRAPPED_FILE_NAME).bc
BC_INSTR_FILE_NAME = $(INSTR_FILE_NAME).bc

SVF_WRAPPED_LL = $(FILE_NAME).wrapped.svf.ll
SVF_WRAPPED = $(FILE_NAME).wrapped.svf.bc
SVF_WRAPPED_DEPLOY_BC = $(FILE_NAME).wrapped.svf.deploy.bc
SVF_WRAPPED_DEPLOY = $(FILE_NAME).wrapped.svf.deploy

SVF_VSA_ANALIZED = $(FILE_NAME).wrapped.svf.vsa.bc

instr-llvm: run build.lib.compile run.instr

pass-all:
	yes | cp -f $(ROOT_BUILD)/CMakeLists.txt $(LLVM_TRANSFORMS)/CMakeLists.txt
	
	# Copy InputWrapper Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)
	cp -r $(ROOT_BUILD)/$(PASS_WRAPPER) $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)

	# Copy Instrumentor Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_INSTR)
	cp -r $(ROOT_BUILD)/$(PASS_INSTR) $(LLVM_TRANSFORMS)/$(PASS_INSTR)

	# Copy Zbouncer Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_NAME)
	cp -r $(ROOT_BUILD)/$(PASS_NAME) $(LLVM_TRANSFORMS)/$(PASS_NAME)
	
	# Compile all new Passes
	cd /root/llvm_all/build_llvm && ninja install

vsa:
	$(OPT) -mem2reg ../llvm-abstractinterpretation/samples/add-1.bc -o ../llvm-abstractinterpretation/samples/add-1-mem2reg.bc
	#$(OPT) -load $(LIB_LLVM)/LLVMValueSetAnalysisTutorial.so -vsatutorialpass -time-passes < ../llvm-abstractinterpretation/samples/add-1-mem2reg.bc > $(SVF_VSA_ANALIZED) 2>vsa.txt
	$(OPT) -load $(LIB_LLVM)/LLVMValueSetAnalysisTutorial.so -vsatutorialpass -time-passes < hello.bc > $(SVF_VSA_ANALIZED) 2>vsa.txt
	cat vsa.txt

vsa2:
	$(OPT) -mem2reg ../llvm-abstractinterpretation/samples/add-1.bc -o ../llvm-abstractinterpretation/samples/add-1-mem2reg.bc
	#$(OPT) -load $(LIB_LLVM)/llvm-pain.so -painpass -time-passes < ../llvm-abstractinterpretation/samples/add-1-mem2reg.bc > $(SVF_VSA_ANALIZED) 2>vsa2.txt
	$(OPT) -load $(LIB_LLVM)/llvm-pain.so -painpass -time-passes < hello.bc > $(SVF_VSA_ANALIZED) 2>vsa2.txt
	cat vsa2.txt

instr-svf-wrapped: pass-all
	$(AS) $(SVF_WRAPPED_LL)
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_NAME).so -$(PASS_NAME) -time-passes < $(SVF_WRAPPED) > $(SVF_WRAPPED_DEPLOY_BC) 2>log_svf_wrapped.txt
	cat log_svf_wrapped.txt
	cp $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/zbouncer/libzbouncer.a
	mkdir -p /usr/local/lib/zbouncer/libzbouncer
	cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
	$(CC) $(SVF_WRAPPED_DEPLOY_BC) -L/usr/local/lib/zbouncer -lzbouncer -o $(SVF_WRAPPED_DEPLOY) 
	cp $(SVF_WRAPPED_DEPLOY) $(ROOT_BUILD)/zbouncer-ta/host/optee_hello
	$(DIS) $(SVF_WRAPPED_DEPLOY_BC)

function-instr: instr-wrapper pass-all
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_INSTR).so -$(PASS_INSTR) -time-passes < $(BC_WRAPPED_FILE_NAME) > temp.bc 2>log_wrapped.txt
	cat log_wrapped.txt
	mv temp.bc $(BC_WRAPPED_FILE_NAME)
	$(DIS) $(BC_WRAPPED_FILE_NAME)

instr-wrapper: wrapper-bc pass-all
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_WRAPPER).so -$(PASS_WRAPPER) -time-passes < join-wrapped.bc > $(BC_WRAPPED_FILE_NAME) 2>log_wrapped.txt
	cat log_wrapped.txt
	$(DIS) $(BC_WRAPPED_FILE_NAME)
	cp $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/zbouncer/libzbouncer.a
	mkdir -p /usr/local/lib/zbouncer/libzbouncer
	cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
	$(CC) $(BC_WRAPPED_FILE_NAME) -Xclang -disable-O0-optnone -L/usr/local/lib/zbouncer -lzbouncer -o $(WRAPPED_FILE_NAME) 
	cp $(WRAPPED_FILE_NAME) $(ROOT_BUILD)/zbouncer-ta/host/optee_hello
	$(DIS) $(BC_WRAPPED_FILE_NAME)

wrapper-bc: $(ROOT_BUILD)/$(C_FILE_NAME)
	$(CC) --version
	$(CC) -O0 -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/$(C_FILE_NAME) -c -o $(BC_FILE_NAME)
	$(CC) -O0 -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/$(WRAPPED_FN)/$(WRAPPED_FN).c -c -o wrapped_fn.bc
	$(LINK) $(BC_FILE_NAME) wrapped_fn.bc -o join-wrapped.bc
	$(DIS) join-wrapped.bc

bc: $(ROOT_BUILD)/$(C_FILE_NAME)
	$(CC) --version
	$(CC) -O0 -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/$(C_FILE_NAME) -c -o $(BC_FILE_NAME)

dis.native: bc
	$(DIS) $(BC_FILE_NAME)

native: bc dis.native
	$(CC) $(BC_FILE_NAME) -L/usr/local/lib/zbouncer -lteec -o $(NATIVE_FILE_NAME)

instr.bc: bc
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_NAME).so -$(PASS_NAME) -time-passes < $(BC_FILE_NAME) > $(BC_INSTR_FILE_NAME) 2>log.txt
	cat log.txt

dis.instr: instr.bc
	$(DIS) $(BC_INSTR_FILE_NAME)

asm: instr.bc
	$(LLC) $(BC_INSTR_FILE_NAME) -o $(ASM_FILE_NAME)

# -static-libgcc -static-libstdc++
instr: asm dis.instr
	cp $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/zbouncer/libzbouncer.a
	mkdir -p /usr/local/lib/zbouncer/libzbouncer
	cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
	$(CC) $(BC_INSTR_FILE_NAME) -L/usr/local/lib/zbouncer -lzbouncer -o $(INSTR_FILE_NAME) 
	cp $(INSTR_FILE_NAME) $(ROOT_BUILD)/zbouncer-ta/host/optee_hello


extractor:
	$(CC) -O0 -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/trace_extractor/trace_extractor.c -c -o trace_extractor.bc
	cp $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/zbouncer/libzbouncer.a
	mkdir -p /usr/local/lib/zbouncer/libzbouncer
	cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
	$(CC) trace_extractor.bc  -L/usr/local/lib/zbouncer -lzbouncer -o trace_extractor_deploy

run: native
	#./$(NATIVE_FILE_NAME)
	echo "Execute with qemu"

run.instr: instr
	#./$(INSTR_FILE_NAME)
	echo "Execute with qemu"

build.lib.copy: 
	yes | cp -f $(ROOT_BUILD)/CMakeLists.txt $(LLVM_TRANSFORMS)/CMakeLists.txt
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_NAME)
	cp -r $(ROOT_BUILD)/$(PASS_NAME) $(LLVM_TRANSFORMS)/$(PASS_NAME)


build.lib.compile: build.lib.copy
	cd /root/llvm_all/build_llvm && ninja install

# ------------------------- qemu-optee ------------------------------
# NOTE: this part of the makefile belongs to another pipeline

# this should be changed --> producing library instead of binary
# the binary should be later link (using clang) with zbouncer (normal world) 
optee-lib: optee-ta 

optee-cp:
	cp -rf $(ROOT_BUILD)/zbouncer-ta /root/rpi3-qemu/optee_examples/zbouncer_ta

optee-host: optee-cp
	cd /root/rpi3-qemu/optee_examples/zbouncer_ta/host && make CROSS_COMPILE=/root/rpi3-qemu/toolchains/aarch32/bin/arm-linux-gnueabihf- TEEC_EXPORT=/usr/local --no-builtin-variables

optee-ta: optee-cp optee-host
	cd /root/rpi3-qemu/optee_examples/zbouncer_ta/ta && make CROSS_COMPILE=/root/rpi3-qemu/toolchains/aarch32/bin/arm-linux-gnueabihf- PLATFORM=vexpress-qemu_virt TA_DEV_KIT_DIR=/root/rpi3-qemu/optee_os/out/arm/export-ta_arm32

optee-hello-cp:
	cp hello.native /root/qemu-optee32/out-br/target/usr/bin/hello.native
	cp hello.instr /root/qemu-optee32/out-br/target/usr/bin/hello.instr

# this should be done on your own enviorment
optee-qemu: optee-host optee-ta
	cd build && make QEMU_VIRTFS_ENABLE=y QEMU_USERNET_ENABLE=y
# -------------------------------------------------------------------

# ------------------------- qemu-optee ------------------------------
# NOTE: this part of the makefile belongs to another pipeline
optee32: optee32-compile
optee32-run: optee32-qemu

optee32-cp:
	rm -rf /root/qemu-optee32/optee_examples/zbouncer_ta
	cp -rf $(ROOT_BUILD)/zbouncer-ta /root/qemu-optee32/optee_examples/zbouncer_ta
	#cp -rf /root/zbouncer/include/tee_client_api.h /usr/local/include/tee_client_api.h
	#cp -rf /root/zbouncer/include/tee_client_api.h /root/qemu-optee32/out-br/build/optee_client_ext-1.0/public/tee_client_api.h
	#cp -rf /root/zbouncer/include/tee_client_api.h /root/qemu-optee32/out-br/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/tee_client_api.h
	#cp -rf /root/zbouncer/include/tee_client_api.h /root/qemu-optee32/optee_client/public/tee_client_api.h
	#rm -rf /root/qemu-optee32/out-br/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/tee_client_api.h
	#cp -rf /root/zbouncer/include/tee_client_api.h /root/qemu-optee32/out-br/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/tee_client_api.h
	#cp -rf $(ROOT_BUILD)/lib/libzbouncer.a /root/qemu-optee32/out-br/build/optee_client_ext-1.0/build/libteec/libteec.a
	#cp -rf $(ROOT_BUILD)/lib/libzbouncer.a /root/qemu-optee32/optee_client/build/libteec/libteec.a
	#cp -rf $(ROOT_BUILD)/lib/libzbouncer.a /root/zbouncer/lib/libteec.a
	#cp -rf $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/libteec.a
	#cp -rf /root/zbouncer/include/tee_api_types.h /root/qemu-optee32/optee_os/out/arm/export-ta_arm32/include/tee_api_types.h 

optee32-compile: optee32-cp
	#cp -rf $(INSTR_FILE_NAME) /root/qemu-optee32/out-br/target/usr/bin/$(INSTR_FILE_NAME) 
	#cp -rf $(NATIVE_FILE_NAME) /root/qemu-optee32/out-br/target/usr/bin/$(NATIVE_FILE_NAME)
	#cp -rf $(WRAPPED_FILE_NAME) /root/qemu-optee32/out-br/target/usr/bin/$(WRAPPED_FILE_NAME)
	#cp -rf $(SVF_WRAPPED_DEPLOY) /root/qemu-optee32/out-br/target/usr/bin/$(SVF_WRAPPED_DEPLOY)
	# Test
	#cp -rf $(ROOT_BUILD)/test_svf/clock_phone/clock_phone.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_clock_phone.instr
	#cp -rf $(ROOT_BUILD)/test_svf/disco-keyboard/disco-keyboard.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_disco-keyboard.instr
	#cp -rf $(ROOT_BUILD)/test_svf/heat_press/heat_press.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_heat_press.instr
	#cp -rf $(ROOT_BUILD)/test_svf/hello/hello.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_hello.instr
	#cp -rf $(ROOT_BUILD)/test_svf/hello_uart/hello_uart.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_hello_uart.instr
	#cp -rf $(ROOT_BUILD)/test_svf/hello-ptr/hello-ptr.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_hello-ptr.instr
	#cp -rf $(ROOT_BUILD)/test_svf/hue-motion/hue-motion.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_hue-motion.instr
	#cp -rf $(ROOT_BUILD)/test_svf/json-parser/json-parser.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_json-parser.instr
	#cp -rf $(ROOT_BUILD)/test_svf/ledmatrixpainter/ledmatrixpainter.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_ledmatrixpainter.instr
	#cp -rf $(ROOT_BUILD)/test_svf/mini-ig-stats/mini-ig-stats.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_mini-ig-stats.instr
	#cp -rf $(ROOT_BUILD)/test_svf/music-controller/music-controller.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_music-controller.instr
	#cp -rf $(ROOT_BUILD)/test_svf/pixel_painter/pixel_painter.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_pixel_painter.instr
	#cp -rf $(ROOT_BUILD)/test_svf/rc/rc.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_rc.instr
	#cp -rf $(ROOT_BUILD)/test_svf/syringePump/syringePump.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_syringePump.instr
	#cp -rf $(ROOT_BUILD)/test_svf/xml-parser/xml-parser.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_xml-parser.instr
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.exec"); do \
		cp -r $$d /root/qemu-optee32/out-br/target/usr/bin;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/Benchmarks/* -type d); do \
		cp -r $$d /root/qemu-optee32/out-br/target/root;\
   done
	#cp $(ROOT_BUILD)/Benchmarks/utils/busybox-armv7l /root/qemu-optee32/out-br/target/root
	# cp -rf $(ROOT_BUILD)/test_svf/dns/dns.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_dns.instr
	# cp -rf $(ROOT_BUILD)/test_svf/Rovertcp/Rovertcp.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_Rovertcp.instr
	# cp -rf $(ROOT_BUILD)/test_svf/gateway/gateway.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_gateway.instr
	# cp -rf $(ROOT_BUILD)/test_svf/textor/textor.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_textor.instr
	# cp -rf $(ROOT_BUILD)/test_svf/Wifi2Eth/Wifi2Eth.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_Wifi2Eth.instr

	# cp -rf $(ROOT_BUILD)/test_svf/xml-parser/xml-parser.wrapped.svf.deploy.exec /root/qemu-optee32/out-br/target/usr/bin/test_xml-parser.instr


optee32-qemu: optee32-compile
	mkdir /root/qemu-optee32/out-br/target/root/input
	cp -r $(ROOT_BUILD)/artifacts/in /root/qemu-optee32/out-br/target/root/input
	cp -r $(ROOT_BUILD)/artifacts/24h-script /root/qemu-optee32/out-br/target/root/24h-script
	cp -r $(ROOT_BUILD)/xml-sample /root/qemu-optee32/out-br/target/root/xml-sample
	cd /root/qemu-optee32/build && make -j12 run GDBSERVER=y 

optee32-compile-only: optee32-compile
	cd /root/qemu-optee32/build && make -j12 GDBSERVER=y
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.exec"); do \
		cp -r $$d /root/zbouncer/artifacts/instr;\
   done
    #
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.normal"); do \
		cp -r $$d /root/zbouncer/artifacts/normal;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.wrapped.svf.deploy.ll"); do \
		cp -r $$d /root/zbouncer/artifacts/svfll;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.ztrace"); do \
		cp -r $$d /root/zbouncer/artifacts/ztrace;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.pag.dot"); do \
		cp -r $$d /root/zbouncer/artifacts/pag;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.icfg.dot"); do \
		cp -r $$d /root/zbouncer/artifacts/icfg;\
   done
	#
	for d in $(shell find $(ROOT_BUILD)/test_svf/* -type f -name "*.svfg.dot"); do \
		cp -r $$d /root/zbouncer/artifacts/svfg;\
   done
	#
	cp -rf /root/qemu-optee32/out-br/build/optee_examples_ext-1.0/zbouncer_ta /root/zbouncer/artifacts/zbouncer_ta

# -------------------------------------------------------------------

# ------------------------- compile libzbouncer ---------------------

optee32-lib: optee32-lib-ar

optee32-lib-cp:
	rm -rf /root/qemu-optee32/optee_examples/libzbouncer
	cp -rf $(ROOT_BUILD)/libzbouncer /root/qemu-optee32/optee_examples/libzbouncer
	cp -rf /root/zbouncer/include/tee_client_api.h /usr/local/include/tee_client_api.h

optee32-lib-compile: optee32-lib-cp
	cd /root/qemu-optee32/optee_examples/libzbouncer/host && make CROSS_COMPILE=/root/qemu-optee32/toolchains/aarch32/bin/arm-linux-gnueabihf- TEEC_EXPORT=/usr/local --no-builtin-variables

optee32-lib-ar: optee32-lib-compile
	cp /root/zbouncer/lib/libteec/tee_client_api.c.o /root/qemu-optee32/optee_examples/libzbouncer/host/tee_client_api.c.o
	cp /root/zbouncer/lib/libteec/teec_trace.c.o /root/qemu-optee32/optee_examples/libzbouncer/host/teec_trace.c.o
	cd /root/qemu-optee32/optee_examples/libzbouncer/host && ar -rcs libzbouncer.a *.o
	cp /root/qemu-optee32/optee_examples/libzbouncer/host/libzbouncer.a $(ROOT_BUILD)/lib/libzbouncer.a
	rm -rf $(ROOT_BUILD)/lib/libzbouncer
	mkdir $(ROOT_BUILD)/lib/libzbouncer
	cd $(ROOT_BUILD)/lib && ar x libzbouncer.a --output libzbouncer
# -------------------------------------------------------------------

clean-instr:
	rm -f $(FILE_NAME).native
	rm -f $(FILE_NAME).instr
	rm -f $(FILE_NAME).S
	rm -f $(FILE_NAME).instr.bc
	rm -f $(FILE_NAME).bc
	rm -f $(FILE_NAME).ll
	rm -f $(FILE_NAME).instr.ll
	rm -f log.txt

# make CROSS_COMPILE=/root/qemu-optee32/toolchains/aarch32/bin/arm-linux-gnueabihf- TEEC_EXPORT=/usr/local --no-builtin-variables

# make CROSS_COMPILE32=/root/qemu-optee32/toolchains/aarch32/bin/arm-linux-gnueabihf- PLATFORM=vexpress-qemu_virt TA_DEV_KIT_DIR=/root/rpi3-qemu/optee_os/out/arm/export-ta_arm32

# exec optee_example_hello_world

# sudo docker run --expose=22 -it --rm -v $PWD/:/root/test-ci qemu-armv8-optee /bin/bash
