regular:
	gcc -D_REENTERANT -o client client.c -lpthread
	gcc -D_REENTERANT -o server server.c -lpthread

memsan:
	gcc -D_REENTERANT -o client client.c -fsanitize=address -lpthread
	gcc -D_REENTERANT -o server server.c -fsanitize=address -lpthread
