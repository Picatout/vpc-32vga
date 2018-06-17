rem file system test
open "hello.txt" for output as #1
print #1,"hello world\r"
putc #1, 33  : putc #1, 10
v$="bonjour\r": n=3*5
? #1, v$, n, 4*5+20, 32.5e-3
close(#1)
dim hello$
open "hello.txt" for input as #1
input #1, hello$
c=fgetc(#1)
input #1,x$,n1,n2,n3!
close(#1)
? hello$,c,x$,n1,n2,n3!

