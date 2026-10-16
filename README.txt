Integer Basic Interpreter for the AT&T 3b2 computer.
Created using Lumo and Claude 2026 October 16

I have a minature build of a 3b2 running AT&T Unix SysV on a raspberry pi.
https://www.gaunt.org/pub/3b2/
https://www.gaunt.org/pub/3b2/3b2-matrix.jpg

The emulator runs the simh implementation as provided by loomcom.com
https://loomcom.com/3b2

So why does this exist? Why not? I figured this has never been done and probably
will never be repeated. A nice experiment to implement a new thing for an old
computer using modern tools.

--------------------------------------------------------------------------------
AT&T 3b2
3b2$ uname -a
att3b2 att3b2 3.2 2 3B2

sum <file>
8899 83 basic.c
15317 106 basic
5556 104 basicO
3972 1 errs.sh
4158 1 make.sh
36894 15 test.bas
32799 15 test_full.bas

--------------------------------------------------------------------------------
Ubuntu 24
$ uname -a
Linux hal2k 7.0.0-29-generic #29~24.04.2-Ubuntu SMP PREEMPT_DYNAMIC Wed Aug 12 17:25:56 UTC 2 x86_64 x86_64 x86_64 GNU/Linux

sum -s <file>
8909 83 basic.c
32108 25 basic.1
52686 34 basic.man
65340 29 basic.txt
55054 19 claude1.zip
4158 1 make.sh
36894 15 test.bas
32799 15 test_full.bas

--------------------------------------------------------------------------------

