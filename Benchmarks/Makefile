PROJECT_DIR = $(shell pwd)/..
FIRMWARE_DIR = $(PROJECT_DIR)/test_svf

TARGET ?= localhost
HOST ?= localhost
PORT ?= 1234

SUBDIRS := $(sort $(dir $(wildcard $(FIRMWARE_DIR)/*/)))
CLEAN_DIRS = $(sort $(dir $(wildcard ./*/)))

BASE_COMMAND = python3 conn_handler.py -t $(TARGET) -s $(HOST) -p $(PORT)

.PHONY: all input-gen clean send send-dir


# For each directory in test_svf this rule creates a correspondin directory, named <dir-name>_verifier
# cotaining:
# 	- The instrumented binary
# 	- The input used to test the verifier
verifier-input:
	for d in $(SUBDIRS); \
	do \
		$(MAKE) verifier-gen FIRM_PATH=$$d; \
	done;

# For each directory in test_svf this rule creates a correspondin directory, named <dir-name>_verifier
# cotaining:
# 	- The instrumented binary
# 	- The input used to benchmark the firmware
benchmark-input:
	for d in $(SUBDIRS); \
	do \
		$(MAKE) benchmark-gen FIRM_PATH=$$d; \
	done;


# Sends all the directories inside Benchmark to the host specified with HOST and PORT variables
send:
	$(foreach d, $(CLEAN_DIRS), $(MAKE) send-dir TARGET_DIR=$(d);)

# Send the directory specified with TARGET_DIR to the remote host
send-dir:
	@:$(if $(value TARGET_DIR),,$(error Undefined variable TARGET_DIR))
	$(BASE_COMMAND) -c "mkdir $(TARGET_DIR)"
	echo 
	for f in $(shell ls $(TARGET_DIR)); do \
		$(BASE_COMMAND) -c s -f $(TARGET_DIR)$$f:$(TARGET_DIR)$$f;\
	done

benchmark-firwmare:
	@:$(if $(value FIRMWARE_DIR),,$(error Undefined variable FIRMWARE_DIR))

	tmp=$(FIRMWARE_DIR) && tmp=$${tmp%_benchmark/} \
	&& tmp=$${tmp#./}.wrapped.svf.deploy.exec \
	&& $(BASE_COMMAND) -c "chmod +x $(FIRMWARE_DIR)$$tmp" \
	&& $(BASE_COMMAND) -c "for t in \$$(ls $(FIRMWARE_DIR) | grep -E \".*input\"); do for line in \$$(cat $(FIRMWARE_DIR)\$$t); do  echo \"Line: \$$line \" >> $(FIRMWARE_DIR)$$tmp.ts ; echo \$$line | time -p -ao $(FIRMWARE_DIR)$$tmp.ts $(FIRMWARE_DIR)$$tmp; done; sleep 45; done";

benchmark:
	$(foreach d, $(CLEAN_DIRS), $(MAKE) benchmark-firwmare FIRMWARE_DIR=$(d))

benchmark-gen:
	@:$(if $(value FIRM_PATH),,$(error Undefined variable FIRM_PATH))
	$(eval BASE := $(shell basename $(FIRM_PATH)))

	mkdir ./$(BASE)_benchmark
	cp $(FIRM_PATH)*.exec ./$(BASE)_benchmark/
	python3 inputgen.py ./$(BASE)_benchmark/*.exec ./$(BASE)_benchmark/ r2_getbranch

verifier-gen:
	@:$(if $(value FIRM_PATH),,$(error Undefined variable FIRM_PATH))
	$(eval BASE := $(shell basename $(FIRM_PATH)))

	mkdir ./$(BASE)_verifier
	cp $(FIRM_PATH)*.exec ./$(BASE)_verifier/
	python3 inputgen.py ./$(BASE)_verifier/*.exec ./$(BASE)_verifier/


# Remove all the benchmark and verifier directory from here and from the remote host
clean: 
	#$(foreach d, $(CLEAN_DIRS), rm -rf $(d);)
	for d in $(CLEAN_DIRS); do\
		if ! echo $$d | grep -q "benchmark"; then \
			rm -r $$d; \
		fi; \
		if ! echo $$d | grep -q "verifier"; then \
			rm -r $$d; \
		fi; \
		$(BASE_COMMAND) -c "rm -r $$d";\
	done

