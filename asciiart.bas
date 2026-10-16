10 REM RANDOM ASCII ART GENERATOR
20 REM RANDOMIZE TIMER
23 INPUT "Press Enter",I
25 RANDOMIZE
30 A=INT(RND(5))
40 PRINT "YOUR RANDOM ART LOOKS LIKE:"
50 IF A <> 1 THEN GOTO 120
60 PRINT "  /\_/\  "
70 PRINT " ( o.o ) "
80 PRINT "  > ^ <  "
90 REM --- ART 2 ---
120 IF A <> 2 THEN GOTO 180
130 PRINT "  _____  "
140 PRINT " /|_|_|\ "
150 PRINT " | |_| | "
160 PRINT " \_|_|_/ "
170 REM --- ART 3 ---
180 IF A <> 3 THEN GOTO 240
190 PRINT "   /\    "
200 PRINT "  /  \   "
210 PRINT " /____\  "
220 PRINT "  ||||   "
230 REM --- ART 4 ---
240 IF A <> 4 THEN GOTO 300
250 PRINT "  (o)(o) "
260 PRINT "   \__/  "
270 PRINT "  ==||== "
280 REM --- ART 5 ---
300 IF A <> 5 THEN GOTO 350
310 PRINT "   _""_   "
320 PRINT "  (o_o)  "
330 PRINT "  ==|==  "
340 PRINT "  _/ \_  "
350 GOTO 23
