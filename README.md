This repository contains modifications for the Dead Store Elimination (DSE) assignment. An example of a graphical view of the MemorySSA graph (using demo2.c as the source code) can be found in mssa-main.png. The dead store elimination pass can be ran with the following command (using Ubuntu):  
/usr/lib/llvm-21/bin/opt   -load-pass-plugin ./lib/libDeadStore.so   -passes="local-dse"   {input file name} -o {output file name}  
  
demo.bc is one compiled file that can be used to test the pass. demo_out.bc already exists and is the product of running the local-dse pass.