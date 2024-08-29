CC = /usr/local/cross_armhf_clang_9.0.0/bin/clang
OPT = /usr/local/cross_armhf_clang_9.0.0/bin/opt
LLC = /usr/local/cross_armhf_clang_9.0.0/bin/llc
DIS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-dis
LINK = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-link
AS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-as
LIB_LLVM = /root/llvm_all/build_llvm/lib
LLVM_TRANSFORMS = /root/llvm_all/llvm/lib/Transforms
PASS_NAME = zbouncer
PASS_WRAPPER = InputWrapper
PASS_FUNC = FunctionInstrumentor
WRAPPED_FN = wrapped-fn
JOIN_WRAPPED = join-wrapped

ROOT_BUILD = $(shell pwd)/..
PATH_2_REACH_SVF ?= $(shell pwd)/../SVF
PATH_2_REACH_SVF = /home/teozoia/Desktop/zbouncer/SVF

SUBDIRS := $(wildcard */.)

svf-bin:
	make -C $(ROOT_BUILD)/SVF

pass-all:
	yes | cp -f $(ROOT_BUILD)/CMakeLists.txt $(LLVM_TRANSFORMS)/CMakeLists.txt
# Copy InputWrapper Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)
	cp -r $(ROOT_BUILD)/$(PASS_WRAPPER) $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)
# Copy Function Instrumentor Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_FUNC)
	cp -r $(ROOT_BUILD)/$(PASS_FUNC) $(LLVM_TRANSFORMS)/$(PASS_FUNC)
# Copy Zbouncer Pass
	rm -rf $(LLVM_TRANSFORMS)/$(PASS_NAME)
	cp -r $(ROOT_BUILD)/$(PASS_NAME) $(LLVM_TRANSFORMS)/$(PASS_NAME)
# Compile all new Passes
	cd /root/llvm_all/build_llvm && ninja install -j1

test-compile: pass-all $(SUBDIRS)
$(SUBDIRS):
# ---- native ---- 
	$(eval TEST := $(patsubst %/.,%,$@))
	make -C $(TEST) build
# ---- InputWrapper ----
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_WRAPPER).so -$(PASS_WRAPPER) -time-passes < $(TEST)/join-w-lib-$(TEST).bc > $(TEST)/$(TEST).wrapped.bc 2>$(TEST)/log_1_InputWrapper.txt
	cat $(TEST)/log_1_InputWrapper.txt
	$(DIS) $(TEST)/$(TEST).wrapped.bc
# ---- FunctionInstrumentor ----
	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_FUNC).so -$(PASS_FUNC) -time-passes < $(TEST)/$(TEST).wrapped.bc > temp.bc 2>$(TEST)/log_1_function_instrumentor.txt
	cat $(TEST)/log_1_function_instrumentor.txt
	mv temp.bc $(TEST)/$(TEST).wrapped.bc
	$(DIS) $(TEST)/$(TEST).wrapped.bc

instr-svf:
# ---- SVF EXEC ----
	rm -rf $(PATH_2_REACH_SVF)/test/
	cp -r ../test $(PATH_2_REACH_SVF)/test/
	#cd $(PATH_2_REACH_SVF) && python3 trace.py > log_instr_svf.txt 
	#cp -r $(PATH_2_REACH_SVF)/test/. ../test_svf/
	#cp $(PATH_2_REACH_SVF)/log_instr_svf.txt ../test_svf/log_instr_svf.txt
	#cp $(PATH_2_REACH_SVF)/ztrace_stats.csv ../test_svf/ztrace_stats.csv
	#rm -rf ../test_svf/Makefile
	#cp ../test/Makefile_svf ../test_svf/Makefile
	
#instr-zbouncer: pass-all $(SUBDIRS)
#$(SUBDIRS):
# ---- Zbouncer ----
#	$(eval TEST := $(patsubst %/.,%,$@))
#	$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_NAME).so -$(PASS_NAME) -time-passes < $(TEST)/$(TEST).wrapped.svf.bc > $(TEST)/$(TEST).wrapped.svf.deploy.bc 2>$(TEST)/log_3_Zbouncer.txt
#	$(DIS) $(TEST)/$(TEST).wrapped.svf.deploy.bc
#	cat $(TEST)/log_3_Zbouncer.txt
# ---- Link function wrapper ----
#	$(CC) -O0 -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/$(WRAPPED_FN)/$(WRAPPED_FN).c -c -o $(TEST)/wrapped_fn.bc
#	$(LINK) $(TEST)/$(TEST).wrapped.svf.deploy.bc wrapped_fn.bc -o $(TEST)/join-wrapped.bc
#	$(DIS) $(TEST)/join-wrapped.bc
# ---- Zbouncer ----
#	cp $(ROOT_BUILD)/lib/libzbouncer.a /usr/local/lib/zbouncer/libzbouncer.a
#	mkdir -p /usr/local/lib/zbouncer/libzbouncer
#	cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
#	$(CC) $(TEST)/join-wrapped.bc -L/usr/local/lib/zbouncer -lzbouncer -o $(TEST)/$(TEST).wrapped.svf.deploy.exec
#	cp $(TEST)/$(TEST).wrapped.svf.deploy.exec $(ROOT_BUILD)/zbouncer-ta/host/optee_hello


.PHONY: all $(SUBDIRS)

clean:
	find . -type f ! \( -name 'Makefile' -o -name '*.c' \) -delete
