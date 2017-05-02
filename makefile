regular:
	gcc -D_REENTERANT -o client client.c -lpthread -lncurses
	gcc -D_REENTERANT -o server server.c -lpthread

memsan:
	gcc -D_REENTERANT -o client client.c -fsanitize=address -lpthread -lncurses
	gcc -D_REENTERANT -o server server.c -fsanitize=address -lpthread
	
remai:
	gcc -D_REENTERANT -o "reMAI Client" ./reMAI/reMAI-client.c -lpthread -lncurses
	gcc -D_REENTERANT -o "reMAI Server" ./reMAI/reMAI-server.c -lpthread -lncurses
