SRC = ud_ucase_sv.c ud_ucase_cl.c

all: ud_ucase_sv ud_ucase_cl

ud_ucase_sv: ud_ucase_sv.o error_functions.o
	gcc -o $@ $^

ud_ucase_cl: ud_ucase_cl.o error_functions.o
	gcc -o $@ $^

%.o: %.c
	gcc -c $^

clean:
	rm -rf *.o
