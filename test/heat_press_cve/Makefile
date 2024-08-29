CC = /usr/local/cross_armhf_clang_9.0.0/bin/clang
DIS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-dis
LINK = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-link

TEST = heat_press_cve

all: clean build

build-others:
	@mkdir -p bc
	@mkdir -p objects
	@mkdir -p ll

	@printf "%-60s \n" "Compiling bitcode dependencies"

	@$(CC) -O0 -fno-discard-value-names -emit-llvm Arduino.c -c -o bc/Arduino.bc
	@$(DIS) bc/Arduino.bc -o ll/Arduino.ll
	@printf "%-60s ...OK\n" "- bc/Arduino.bc"

build: build-others
	@$(CC) -O0 -fno-discard-value-names -emit-llvm $(TEST).c -c -o bc/$(TEST).bc

	@printf "%-60s ...OK\n" "Link and squashing all bcs to bc/$(TEST).bc"
	@$(LINK) bc/$(TEST).bc bc/Arduino.bc -o bc/$(TEST).bc

	@printf "%-60s ...OK\n" "Disassembling bitcode ll/$(TEST).ll"
	@$(DIS) bc/$(TEST).bc -o ll/$(TEST).ll

	@printf "%-60s ...OK\n" "Generating base object objects/$(TEST).o"
	@$(CC) bc/$(TEST).bc -c -o objects/$(TEST).o 

	@printf "%-60s ...OK\n" "Generating executable $(TEST).normal"
	@$(CC) bc/$(TEST).bc -o $(TEST).normal

clean: 
	@find . -type f ! -name '*.c' ! -name '*.h' ! -name 'Makefile' -exec rm -f {} +
	@find . -type d ! -name '.' ! -name '.*' -exec rm -rf {} +
	@printf "%-60s ...OK\n" "Cleaning up $(TEST)"
