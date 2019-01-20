
The basic idea is that the monitor repeatedly tries to find what state the 
monitored system is in. When it starts up, it enters a state called 'START' 
and if it can't find any matching state after trying the various, user-supplied 
conditions, it enters the stated called 'UNKNOWN'. Some changes are afoot for 
how it selects a state if multiple states are valid but at present it just 
picks any that match.

This version builds and runs under Mac OS X and linux with their standard C 
libraries and also under uClibc. The program should build on any system with 
lex/flex and yacc/bison. On original yacc versions or on early bison systems, 
there is a small change required in monitor.y but this should not affect you.

To run the attached:

  tar zxf monstate.tar
  cd monstate
  ln -s makefile.linux Makefile
or
  ln -s makefile.macosx Makefile
  make
  ./monitor <sample1.conf

then, in another terminal (you have 10 seconds before the monitor exits)

  cd monitor
  echo 123 >myproc.pid

after watching the monitor respond to the above (it should say 'running ok' 
and switch to the 'running' state.

  echo "junk" >myproc.pid

and again, note that the monitor now changes to the UNKNOWN state.  After 
being in this state for 10 seconds the monitor will exit.

You can add a -v flag to the monitor command to see various extra information:

   ./monitor -v <sample1.conf


A monitor configuration provides a set of conditions to be tested and actions 
to be performed, both of which may be loaded from external, precompiled modules. 
[not implemented in this version]

The results of a condition may be retained into variables during each test phase, 
using the COLLECT verb. Note that all of these assignments are done at the beginning 
of a test sequence to ensure the variables are ready for use in conditions. 
There is no guarantee about the order the data is collected.
