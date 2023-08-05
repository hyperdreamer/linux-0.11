# linux-0.11
An old Kernel that can be compiled on modern machines 

# For Debugging
Check the symbolic link "kernel.h ---> include/linux/kernel.h". I added a debugger switch in it by defining some macros. 

I know it is urgly :-(, but it is handy and really works :-)! 

If you don't need the debugging function, just uncomment "#define DEBUG" and the compiler will take care of the rest.
