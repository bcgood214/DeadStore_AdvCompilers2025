import sys

config.llvm_tools_dir = "/usr/lib/llvm-21/bin"
config.llvm_shlib_ext = ".so"
config.llvm_shlib_dir = "/home/bencgd/llvm-tutor_v2/llvm-tutor/lib"

import lit.llvm
# lit_config is a global instance of LitConfig
lit.llvm.initialize(lit_config, config)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join("/home/bencgd/llvm-tutor_v2/llvm-tutor/test")

# Let the main config do the real work.
lit_config.load_config(config, "/home/bencgd/llvm-tutor_v2/llvm-tutor/test/lit.cfg.py")
