CC = /usr/local/cross_armhf_clang_9.0.0/bin/clang
OPT = /usr/local/cross_armhf_clang_9.0.0/bin/opt
LLC = /usr/local/cross_armhf_clang_9.0.0/bin/llc
DIS = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-dis
LINK = /usr/local/cross_armhf_clang_9.0.0/bin/llvm-link
LIB_LLVM = /root/llvm_all/build_llvm/lib
LLVM_TRANSFORMS = /root/llvm_all/llvm/lib/Transforms
PASS_NAME = zbouncer
PASS_WRAPPER = InputWrapper
WRAPPED_FN = wrapped-fn
JOIN_WRAPPED = join-wrapped

ROOT_BUILD = ../..

TEST = hue-motion
TEST_PATH = $(TEST)
TEST_OUTPATH = $(TEST)
JOIN_W_LIB = join-w-lib-$(TEST)

build-others:
	#no-build-others

build: build-others
	$(CC) -O0 -fno-discard-value-names -emit-llvm $(TEST).c -c -o $(TEST).bc
	$(LINK) $(TEST).bc -o $(JOIN_W_LIB).bc
	$(DIS) $(JOIN_W_LIB).bc