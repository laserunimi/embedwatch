TMP := $(shell mktemp -d)
BIN_DIR ?= $(TMP)
PROJECT_DIR = $(shell pwd)/..
EXAMPLE_SVF ?= $(PROJECT_DIR)/SVF/tools/Example/svf-ex.cpp
INST_DIRNAME = Debug-build
RES_DIR = $(PROJECT_DIR)/SVF/$(INST_DIRNAME)

$(INST_DIRNAME): $(EXAMPLE_SVF)
	@echo $(BIN_DIR)
	@echo $(PROJECT_DIR)
	@echo $(EXAMPLE_SVF)
	
	test -d $(BIN_DIR)/.git || git clone https://github.com/SVF-tools/SVF.git $(BIN_DIR)
	cd $(BIN_DIR) && git checkout 9609395647cf5c791e9ab7d56bd90cbfcc3151c0
	@echo $(EXAMPLE_SVF)
	cp $(EXAMPLE_SVF) $(BIN_DIR)/tools/Example/svf-ex.cpp
	cd $(BIN_DIR) && ./build.sh debug
	test -d $(RES_DIR) && rm -rf $(RES_DIR)
	/bin/cp -r $(BIN_DIR)/Debug-build $(RES_DIR)


clean:
	rm -rf $(PROJECT_DIR)/SVF/$(INST_DIRNAME)


