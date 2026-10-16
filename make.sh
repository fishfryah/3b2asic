set -vx
case `uname -m` in
	3B2)
		cc -o basic basic.c
		cc -O -o basicO basic.c
		sum basic*
		;;
	x86_64)
		gcc -std=c90 -Wno-implicit-int -Wno-implicit-function-declaration basic.c -o basicL
		sum -s basic*
		;;
esac
