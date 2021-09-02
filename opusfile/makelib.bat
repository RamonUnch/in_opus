:: To make the modified version of libopusfile
gcc -c *.c -I. -Os -fno-stack-check -fno-stack-protector -mno-stack-arg-probe -march=i486 -mtune=i686 -mpreferred-stack-boundary=2
