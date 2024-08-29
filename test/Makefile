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
PASS_REC = RecursionTracker
WRAPPED_FN = wrapped-fn
JOIN_WRAPPED = join-wrapped

SVF-EX-OPT = --pta-dda

ROOT_BUILD = $(shell pwd)/..
PATH_2_REACH_SVF = you_path_here

SUBDIRS := $(wildcard */.)
SUBDIRS_TEST := $(addsuffix -test,$(SUBDIRS))
SUBDIRS_SVF := $(addsuffix -svf,$(SUBDIRS))
SUBDIRS_ZBOUNCER := $(addsuffix -zbouncer,$(SUBDIRS))
SUBDIRS_SIZE := $(addsuffix -size,$(SUBDIRS))

container-not-required:
	if [ ! -f "/proc/1/cgroup" ]; then \
        @echo "Your MUST execute the recipe OUT container"; \
        exit 1; \
    fi

svf-bin:
	make -C $(ROOT_BUILD)/SVF

pass-all:
	@printf "%-60s" "Copy CMakeLists to LLVM directory"
	@yes | cp -f $(ROOT_BUILD)/CMakeLists.txt $(LLVM_TRANSFORMS)/CMakeLists.txt
	@printf "...OK\n"

	@printf "%-60s" "Copy $(PASS_WRAPPER) pass"
	@rm -rf $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)
	@cp -r $(ROOT_BUILD)/$(PASS_WRAPPER) $(LLVM_TRANSFORMS)/$(PASS_WRAPPER)
	@printf "...OK\n"

	@printf "%-60s" "Copy $(PASS_FUNC) pass"
	@rm -rf $(LLVM_TRANSFORMS)/$(PASS_FUNC)
	@cp -r $(ROOT_BUILD)/$(PASS_FUNC) $(LLVM_TRANSFORMS)/$(PASS_FUNC)
	@printf "...OK\n"

	@printf "%-60s" "Copy $(PASS_REC) pass"
	@rm -rf $(LLVM_TRANSFORMS)/$(PASS_REC)
	@cp -r $(ROOT_BUILD)/$(PASS_REC) $(LLVM_TRANSFORMS)/$(PASS_REC)
	@printf "...OK\n"

	@printf "%-60s" "Copy $(PASS_NAME) pass"
	@rm -rf $(LLVM_TRANSFORMS)/$(PASS_NAME)
	@cp -r $(ROOT_BUILD)/$(PASS_NAME) $(LLVM_TRANSFORMS)/$(PASS_NAME)
	@printf "...OK\n"

	@printf "%-60s" "Now recompiling LLVM with new passes"
	@cd /root/llvm_all/build_llvm && ninja install
	@printf "%-60s ...OK\n" "Compiled LLVM with new passes"

test-compile: pass-all $(SUBDIRS_TEST)

$(SUBDIRS_TEST):
	$(eval TEST := $(patsubst %/.-test,%,$@))
	@make -C $(TEST) build
	@chmod -R 777 $(TEST)
	@mkdir -p -m 777 $(TEST)/logs

	@printf "%-60s" "Running LLVM$(PASS_WRAPPER).so"
	@$(OPT) -O0 -load $(LIB_LLVM)/LLVM$(PASS_WRAPPER).so -$(PASS_WRAPPER) < $(TEST)/bc/$(TEST).bc > $(TEST)/bc/$(TEST).inputwrapper.bc 2>$(TEST)/logs/pass_inputwrapper.txt
	@$(DIS) $(TEST)/bc/$(TEST).inputwrapper.bc -o $(TEST)/ll/$(TEST).inputwrapper.ll
	@printf " ...OK\n"
	@$(CC) -Os $(TEST)/bc/$(TEST).inputwrapper.bc -c -o $(TEST)/objects/$(TEST).inputwrapper.o
	@printf "%-60s ...OK\n" "InputWrapper object $(TEST).inputwrapper.o"

	@printf "%-60s" "Running LLVM$(PASS_FUNC).so"
	@$(OPT) -O0 -load $(LIB_LLVM)/LLVM$(PASS_FUNC).so -$(PASS_FUNC) < $(TEST)/bc/$(TEST).inputwrapper.bc > $(TEST)/bc/$(TEST).funcinstr.bc 2>$(TEST)/logs/pass_funcinstr.txt
	@$(DIS) $(TEST)/bc/$(TEST).funcinstr.bc -o $(TEST)/ll/$(TEST).funcinstr.ll
	@printf " ...OK\n"
	@$(CC) -Os $(TEST)/bc/$(TEST).funcinstr.bc -c -o $(TEST)/objects/$(TEST).funcinstr.o
	@printf "%-60s ...OK\n" "FunctionInstrumentor object $(TEST).funcinstr.o"

	@printf "%-60s" "Running LLVM$(PASS_REC).so"
	@$(OPT) -O0 -load $(LIB_LLVM)/LLVM$(PASS_REC).so -$(PASS_REC) < $(TEST)/bc/$(TEST).funcinstr.bc  > $(TEST)/bc/$(TEST).rectracker.bc 2>$(TEST)/logs/pass_rectracker.txt
	@$(DIS) $(TEST)/bc/$(TEST).rectracker.bc -o $(TEST)/ll/$(TEST).rectracker.ll
	@printf " ...OK\n"
	@$(CC) -Os $(TEST)/bc/$(TEST).rectracker.bc -c -o $(TEST)/objects/$(TEST).rectracker.o
	@printf "%-60s ...OK\n" "RecursionTracker object $(TEST).rectracker.o"
	@printf "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"


instr-svf: container-not-required prepare-svf $(SUBDIRS_SVF)

prepare-svf:
	@printf "%-60s" "Copy Example intro SVF path"
	@cp -r $(ROOT_BUILD)/SVF/tools/Example $(PATH_2_REACH_SVF)/tools
	@printf "...OK\n"

	@printf "%-60s\n" "Compile SVF"
	@cd $(PATH_2_REACH_SVF)/Debug-build && make -j8
	@printf "%-60s ...OK\n" "Compile SVF"

$(SUBDIRS_SVF):
	$(eval TEST := $(patsubst %/.-svf,%,$@))
	@mkdir -p -m 777 $(TEST)/graph
	@mkdir -p -m 777 $(TEST)/ztrace
	@printf "%-60s" "Pass SVF on $(TEST)"
	@-$(PATH_2_REACH_SVF)/Debug-build/bin/svf-ex $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).rectracker.bc $(SVF-EX-OPT) $(ROOT_BUILD)/test/$(TEST) >$(TEST)/logs/pass_svf.txt
	@-mv $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).rectracker.svf.bc $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).svf.bc
	@printf " ...OK\n"

instr-zbouncer: pass-all $(SUBDIRS_ZBOUNCER)

$(SUBDIRS_ZBOUNCER):
	$(eval TEST := $(patsubst %/.-zbouncer,%,$@))

# parte del passaggio precedente che peró non é possibile eseguire perché
# fuori dal container manca llvm
	@printf "%-60s" "Concluding steps: $(TEST)"
	@-$(DIS) $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).svf.bc -o $(ROOT_BUILD)/test/$(TEST)/ll/$(TEST).svf.ll
	@-$(CC) -Os $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).svf.bc -c -o $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).svf.o
	@printf "...OK\n"

	@printf "%-60s" "Running LLVM$(PASS_NAME).so"
	@-$(OPT) -load $(LIB_LLVM)/LLVM$(PASS_NAME).so -$(PASS_NAME) < $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).svf.bc > $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer.bc  2>$(TEST)/logs/pass_zbouncer.txt
	@-$(DIS) $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer.bc -o $(ROOT_BUILD)/test/$(TEST)/ll/$(TEST).zbouncer.ll
	@-$(CC) -Os $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer.bc -c -o $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).zbouncer.o
	@printf "...OK\n"

	@printf "%-60s" "Adding wrapper"
	@-$(CC) -Os -fno-discard-value-names -emit-llvm $(ROOT_BUILD)/$(WRAPPED_FN)/$(WRAPPED_FN).c -c -o $(ROOT_BUILD)/$(WRAPPED_FN)/$(WRAPPED_FN).bc
	@-$(LINK) $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer.bc $(ROOT_BUILD)/$(WRAPPED_FN)/$(WRAPPED_FN).bc -o $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer_wfn.bc
	@-$(DIS) $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer_wfn.bc -o $(ROOT_BUILD)/test/$(TEST)/ll/$(TEST).zbouncer_wfn.ll
	@-$(CC) -Os $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer_wfn.bc -c -o $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).zbouncer_wfn.o
	@printf "...OK\n"

	@printf "%-60s" "Producing embedwatch binary"
	@mkdir -p /usr/local/lib/zbouncer/libzbouncer
	@cp $(ROOT_BUILD)/libzbouncer/host/libzbouncer.h /usr/local/lib/zbouncer/libzbouncer/libzbouncer.h
	@cp $(ROOT_BUILD)/include/tee_client_api.h /usr/local/lib/zbouncer/libzbouncer/tee_client_api.h
	@cp $(ROOT_BUILD)/include/teec_trace.h /usr/local/lib/zbouncer/libzbouncer/teec_trace.h
	@-$(CC) -Os $(ROOT_BUILD)/test/$(TEST)/bc/$(TEST).zbouncer_wfn.bc -L$(ROOT_BUILD)/lib -lzbouncer -o $(ROOT_BUILD)/test/$(TEST)/$(TEST).embw.exec
	@printf "...OK\n"
	@printf "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"

instr-size: $(SUBDIRS_SIZE)

LIB_SIZE := 17924

$(SUBDIRS_SIZE):
	$(eval TEST := $(patsubst %/.-size,%,$@))
	@-printf "$(TEST) "
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).o)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).inputwrapper.o)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).funcinstr.o)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).rectracker.o)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).svf.o)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).zbouncer_wfn.o)"
	@-printf "%d " "$$(( $(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/objects/$(TEST).zbouncer_wfn.o) + $(LIB_SIZE) ))"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/$(TEST).normal)"
	@-printf "%d " "$(shell stat -c%s $(ROOT_BUILD)/test/$(TEST)/$(TEST).embw.exec)"
	@-printf "\n"

.PHONY: all $(SUBDIRS)

clean:
	find . -type f ! \( -name 'Makefile' -o -name '*.c' \) -delete
