make:
	gcc -pthread main.c -o LANMessanger.out
clean:
	rm *.out
run:
	./LANMessanger.out