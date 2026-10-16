10 REM test2.bas - integer BASIC tests for basic.c on the 3b2
20 REM Self-checking lines print ok or FAIL. RAW lines are for eyeballing.
30 REM Risky tests are commented out near the end. Enable one at a time.
100 REM ---- 1. integer arithmetic ----
110 IF 7/2=3 THEN PRINT "div trunc ok" ELSE PRINT "div trunc FAIL"
120 IF -7/2=-3 THEN PRINT "div neg toward zero ok" ELSE PRINT "div neg differs"
130 IF 7 MOD 3=1 THEN PRINT "mod pos ok" ELSE PRINT "mod pos FAIL"
140 IF -7 MOD 3=-1 THEN PRINT "mod neg sign of dividend ok" ELSE PRINT "mod neg differs"
150 IF 7 MOD -3=1 THEN PRINT "mod neg divisor ok" ELSE PRINT "mod neg divisor differs"
160 IF 2^30=1073741824 THEN PRINT "2^30 ok" ELSE PRINT "2^30 FAIL"
170 IF 3^0=1 THEN PRINT "x^0 ok" ELSE PRINT "x^0 FAIL"
180 IF 2^3^2=512 OR 2^3^2=64 THEN PRINT "pow assoc: ";2^3^2 ELSE PRINT "pow assoc FAIL"
190 A=2147483647
200 B=A+1
210 PRINT "RAW max+1 = ";B;" (expect -2147483648 on 2s-complement wrap)"
220 PRINT "RAW 100000*100000 = ";100000*100000;" (expect 1410065408 wrapped)"
230 PRINT "RAW -(-2147483647-1) = ";-(-2147483647-1)
240 PRINT "RAW ABS of min int = ";ABS(-2147483647-1)
250 PRINT "RAW -2^2, (-2)^2 = ";-2^2;" ";(-2)^2
260 PRINT "RAW -7/2 = ";-7/2;"  -7 MOD 2 = ";-7 MOD 2
300 REM ---- 2. comparisons and logic (true is -1) ----
310 IF (1=1)=-1 THEN PRINT "true is -1 ok" ELSE PRINT "true value FAIL"
320 IF (1=2)=0 THEN PRINT "false is 0 ok" ELSE PRINT "false value FAIL"
330 IF NOT 0=-1 THEN PRINT "NOT 0 ok" ELSE PRINT "NOT 0 FAIL"
340 IF (5 AND 3)=1 THEN PRINT "bitwise AND ok" ELSE PRINT "bitwise AND FAIL"
350 IF (5 OR 3)=7 THEN PRINT "bitwise OR ok" ELSE PRINT "bitwise OR FAIL"
360 PRINT "RAW NOT 5 = ";NOT 5;" (MS-style bitwise gives -6, so IF NOT 5 is TRUE)"
370 IF (1=1) AND (2=2) OR (3=4) THEN PRINT "AND/OR ok" ELSE PRINT "AND/OR FAIL"
380 IF 3>=3 AND 3<=3 AND 3<>4 THEN PRINT "relops ok" ELSE PRINT "relops FAIL"
400 REM ---- 3. RND: RND(n) returns 1..n, seeded with fixed 7919 until RANDOMIZE ----
410 PRINT "RAW RND(100) x8 (identical every run unless RANDOMIZE used):"
420 FOR I=1 TO 8
430 PRINT RND(100);" ";
440 NEXT I
450 PRINT
460 S=0
470 FOR I=1 TO 2000
480 X=RND(6)
490 IF X<1 OR X>6 THEN S=S+1
500 NEXT I
510 IF S=0 THEN PRINT "RND(6) range 1..6 ok" ELSE PRINT "RND(6) range FAIL"
520 DIM C(6)
530 FOR I=1 TO 6000
540 X=RND(6)
550 C(X)=C(X)+1
560 NEXT I
570 PRINT "RAW dice counts (each ~1000): ";C(1);" ";C(2);" ";C(3);" ";C(4);" ";C(5);" ";C(6)
580 RANDOMIZE 42
590 A=RND(1000)
600 RANDOMIZE 42
610 B=RND(1000)
620 IF A=B THEN PRINT "RANDOMIZE n repeatable ok" ELSE PRINT "RANDOMIZE n FAIL"
630 RANDOMIZE
640 PRINT "RAW after RANDOMIZE (time seed), RND(1000)=";RND(1000)
700 REM ---- 4. strings (source is uppercased on entry, so case tests are moot) ----
710 A$="HELLO WORLD"
720 IF LEN(A$)=11 THEN PRINT "LEN ok" ELSE PRINT "LEN FAIL"
730 IF LEFT$(A$,5)="HELLO" THEN PRINT "LEFT$ ok" ELSE PRINT "LEFT$ FAIL"
740 IF RIGHT$(A$,5)="WORLD" THEN PRINT "RIGHT$ ok" ELSE PRINT "RIGHT$ FAIL"
750 IF MID$(A$,7,5)="WORLD" THEN PRINT "MID$ 3-arg ok" ELSE PRINT "MID$ 3-arg FAIL"
760 IF MID$(A$,7)="WORLD" THEN PRINT "MID$ 2-arg ok" ELSE PRINT "MID$ 2-arg FAIL"
770 IF ASC("A")=65 THEN PRINT "ASC ok" ELSE PRINT "ASC FAIL"
780 IF CHR$(66)="B" THEN PRINT "CHR$ ok" ELSE PRINT "CHR$ FAIL"
790 IF VAL("123")=123 THEN PRINT "VAL ok" ELSE PRINT "VAL FAIL"
800 IF VAL("-45")=-45 THEN PRINT "VAL neg ok" ELSE PRINT "VAL neg FAIL"
810 PRINT "RAW VAL(ABC)=";VAL("ABC");" VAL(12XY)=";VAL("12XY")
820 PRINT "RAW STR$(42) len=";LEN(STR$(42));" [";STR$(42);"]  STR$(-7)=[";STR$(-7);"]"
830 IF LEN(SPACE$(5))=5 THEN PRINT "SPACE$ ok" ELSE PRINT "SPACE$ FAIL"
840 B$=A$+"!"
850 IF B$="HELLO WORLD!" THEN PRINT "concat ok" ELSE PRINT "concat FAIL"
860 IF "ABC"<"ABD" THEN PRINT "str < ok" ELSE PRINT "str < FAIL"
870 IF "AB"<"ABC" THEN PRINT "prefix shorter < ok" ELSE PRINT "prefix FAIL"
880 E$=""
890 IF LEN(E$)=0 THEN PRINT "empty string ok" ELSE PRINT "empty string FAIL"
900 IF LEFT$(A$,0)="" THEN PRINT "LEFT$ 0 ok" ELSE PRINT "LEFT$ 0 FAIL"
910 IF LEFT$(A$,99)=A$ THEN PRINT "LEFT$ overlong ok" ELSE PRINT "LEFT$ overlong FAIL"
920 F$=SPACE$(60)+SPACE$(60)
930 PRINT "RAW 60+60 char concat, LEN=";LEN(F$);" (80 = truncated at STR_LEN; else error/overrun)"
1000 REM ---- 5. loops (basic.c FOR always runs the body once: no pre-test) ----
1010 S=0
1020 FOR I=10 TO 1 STEP -3
1030 S=S+I
1040 NEXT I
1050 IF S=22 THEN PRINT "neg STEP ok" ELSE PRINT "neg STEP FAIL"
1060 S=0
1070 FOR I=1 TO 0
1080 S=S+1
1090 NEXT I
1100 PRINT "RAW zero-trip loop S=";S;" (1 = body runs once, classic MS BASIC; 0 = ANSI)"
1110 S=0
1120 FOR I=1 TO 3
1130 FOR J=1 TO 4
1140 S=S+1
1150 NEXT J
1160 NEXT I
1170 IF S=12 THEN PRINT "nested FOR ok" ELSE PRINT "nested FOR FAIL"
1180 S=0
1190 FOR I=1 TO 3:FOR J=1 TO 3:S=S+1:NEXT J,I
1200 IF S=9 THEN PRINT "NEXT J,I ok" ELSE PRINT "NEXT J,I FAIL"
1210 I=0
1220 I=I+1
1230 IF I<5 THEN GOTO 1220
1240 IF I=5 THEN PRINT "GOTO loop ok" ELSE PRINT "GOTO loop FAIL"
1250 IF I=5 THEN 1270
1260 PRINT "IF THEN lineno FAIL"
1270 PRINT "IF THEN lineno ok"
1300 REM ---- 6. arrays (1-D only; default 0..10; DIM gives 0..n) ----
1310 DIM D(20)
1320 FOR I=0 TO 20
1330 D(I)=I*I
1340 NEXT I
1350 IF D(20)=400 AND D(7)=49 THEN PRINT "DIM array ok" ELSE PRINT "DIM array FAIL"
1360 X(10)=99
1370 IF X(10)=99 THEN PRINT "default array top index ok" ELSE PRINT "default array FAIL"
1380 DIM N$(3)
1390 N$(0)="ZERO"
1400 N$(3)="THREE"
1410 IF N$(3)="THREE" AND N$(0)="ZERO" THEN PRINT "string array ok" ELSE PRINT "string array FAIL"
1420 K=3
1430 D(K+1)=D(K)+1
1440 IF D(4)=10 THEN PRINT "array expr subscript ok" ELSE PRINT "array expr subscript FAIL"
1500 REM ---- 7. GOSUB nesting ----
1510 T=0
1520 GOSUB 1600
1530 IF T=3 THEN PRINT "nested GOSUB ok" ELSE PRINT "nested GOSUB FAIL T=";T
1540 GOTO 1700
1600 T=T+1
1610 GOSUB 1650
1620 RETURN
1650 T=T+1
1660 GOSUB 1680
1670 RETURN
1680 T=T+1
1690 RETURN
1700 REM ---- 8. DATA / READ / RESTORE ----
1710 READ P,Q$,R
1720 IF P=7 AND Q$="SEVEN" AND R=-3 THEN PRINT "READ ok" ELSE PRINT "READ FAIL"
1730 RESTORE
1740 READ P
1750 IF P=7 THEN PRINT "RESTORE ok" ELSE PRINT "RESTORE FAIL"
1760 DATA 7,"SEVEN",-3
1800 REM ---- 9. misc ----
1810 X=5:Y=6:PRINT "multi-stmt ok: ";X+Y
1820 IF X=5 THEN PRINT "then-part ";:PRINT "continues" ELSE PRINT "else-part"
1830 IF X=6 THEN PRINT "WRONG" ELSE PRINT "else ok":PRINT "else continues"
1840 PRINT "semi";"colon";1;2
1850 PRINT SQR(144);SQR(10);SQR(0);SQR(1);SQR(2)
1860 PRINT "RAW SQR(144),SQR(10),SQR(0),SQR(1),SQR(2) above (expect 12 3 0 1 1)"
1870 PRINT ABS(-5);ABS(5);SGN(-9);SGN(0);SGN(9)
1880 PRINT INT(5)
1890 PRINT "RAW comma zones: ";
1900 PRINT 1,2,3
1910 REM GOTO 3000
2000 REM ---- 10. performance: remove 'GOTO 3000' on line 1910, run with 'time' ----
2010 S=0
2020 REM FOR I=1 TO 1000000
2021 FOR I=1 TO 1000
2030 S=S+1
2040 NEXT I
2050 PRINT "perf done S=";S
2100 REM ---- 11. interactive INPUT: uncomment, type 42 then HELLO ----
2110 INPUT "NUMBER";N
2120 PRINT "you typed ";N
2130 INPUT "WORD";W$
2140 PRINT "you typed ";W$
2150 INPUT "TWO NUMBERS";A,B
2160 PRINT A+B
2200 REM ---- 12. risky/error tests: uncomment ONE at a time, expect a clean ?ERROR, not a hang or crash ----
2210 PRINT 1/0
2220 PRINT 5 MOD 0
2230 PRINT SQR(-1)
2240 PRINT SQR(2147483647)
2250 PRINT RND(0)
2260 D(21)=1
2270 D(-1)=1
2280 DIM Z(6000)
2290 GOTO 99999
2300 RETURN
2310 NEXT
2320 READ Q:READ Q:READ Q:READ Q:READ Q:READ Q
2330 PRINT CHR$(300)
2340 PRINT MID$("AB",0,1)
2350 PRINT 1.5
2360 GOSUB 2360   (endless recursion: expect a clean stack error at depth 32)
3000 PRINT "test2 complete"
3010 END
