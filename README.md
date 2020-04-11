# System Architecture
## Client:
Each client is multi-threaded in nature.  While the main thread of the program reads from the terminal and writes on the server’s socket file descriptor ( in an infinite loop ), the secondary thread remains in an infinite loop (called every second), checking if something has been written to the socket file descriptor. 
## Server:
The server makes use of multiplexed I/O by initially starting an infinite loop and putting the Terminal’s Input file descriptor and the listening socket into the poll() API.

If there is a return event (POLLIN) on the Terminal’s Input (STDIN_FILEIO) file descriptor, the server parses the command and writes to a pipe (this will be further discussed later) if necessary. It then reads from another pipe and displays an output on the terminal as required.

If there is a return event (POLLIN) on the listening Socket file descriptor, the server proceeds to accept the connection from a potential client. Once accepted, the server stores the listening socket file descriptor of the client. Now the server establishes two pipes, one that it will write on and another that it will be reading from. The server follows this by an immediate fork() which spawns a child process of the server. This child process will now specifically cater to the clients requests.

After creation, the child process starts another infinite loop. Now it puts the server’s writing pipe’s reading end’s file descriptor and the client’s listening socket’s file descriptors into the poll() API.

Now the child waits for a return event on either of the file descriptors. 

If the client program sends a request, returning an event for the client’s listening socket’s file descriptor, the child process responds to the command. In a special case, if the command is “run …” the child will perform another fork() and then execute the requested program in the child process’ memory space.

If the server writes to the pipe, the child process responds by writing to its own pipe so the server may sequentially read from that pipe.
