# linux-0.11
An old Kernel that can be compiled on modern machines 

# For Debugging
I added a debugger switch in "include/linux/kernel.h" by defining some macros. 

I know it is urgly :-(, but it is handy and really works :-)! 

If you don't need the debugging functionality, disable it in "BUILD_CONFIG.mk" 
and the compiler will take care of the rest.
