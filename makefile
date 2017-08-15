test : test.c json.c
	gcc -o test test.c json.c -lm
