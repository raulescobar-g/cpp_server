# cpp_server

To run, navigate to root directory, and run the following commands:
```
make
./server -r <port_number>
```

open another terminal, or clone repo on remote machine.

To send file from client to server run the following command:
```
./client -w <worker_count> -b <bounded_buffer_sise> -f <filename> -h <host_name> -r <port_number>
```
