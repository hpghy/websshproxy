headfile= common.h utils.h sock.h works.h heap.h buffer.h epoll.h 
gc= gcc -c -g -Wall 
websshproxyd: websshproxy.o utils.o sock.o works.o heap.o buffer.o epoll.o 
	gcc -o websshproxyd websshproxy.o utils.o sock.o works.o heap.o buffer.o epoll.o
websshproxy.o: websshproxy.c $(headfile)
	$(gc) websshproxy.c
utils.o: utils.c $(headfile) 	 
	$(gc) utils.c  
sock.o: sock.c $(headfile) 		 
	$(gc) sock.c 
works.o: works.c $(headfile)	
	$(gc) works.c  
heap.o: heap.c $(headfile)		 
	$(gc) heap.c  
buffer.o: buffer.c $(headfile) 	 
	$(gc) buffer.c  
epoll.o: epoll.c $(headfile) 	 
	$(gc) epoll.c  

clean:
	rm *.o
