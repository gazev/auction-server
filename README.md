# Build
You can compile both server and client together simply running `make`.

To compile each individually do `make server` or `make client` respectively

# Usage
```
$ ./AS -h
usage: ./AS [-h] [-v] [-p ASport] [-o log_file]

options:
  -h,          show this message and exit
  -v,          set log level to verbose
  -d,          set log level to debug
  -p ASport,   port where the server will be listening (default: 58078)
  -o log_file, set log file (default: stdout and stderr)
```

```
$ ./user -h
usage: ./user [-h] [-n ASIP] [-p ASport]

options:
  -h,        show this message and exit
  -n ASIP,   IP of AS server
  -p ASPORT, port where the server will be listening on (default: 58078)
```

# Configuration
The default configuration described in this section can be tweaked in the `utils/config.h` file.

The AS creates an ASDIR for its database on the working directory from where it is evoked.

The user and client use the port 58078 for both TCP and UDP.

Both the client and server have a set timeout of 5s to receive TCP and UDP responses.

The AS uses one thread for accepting TCP connections, 30 worker threads to serve the TCP connections and one thread to receive and serve UDP messages.


# Task list
X - Done, + - Needs revision
Client:
[X] - login
[X] - logout 
[X] - unregister 
[X] - exit
[X] - list 
[X] - myauctions 
[X] - mybids
[X] - showrecord

[X] - open
[X] - close
[X] - showasset
[X] - bid

Server:
[X] - login
[X] - logout 
[X] - unregister 
[X] - exit
[X] - list 
[X] - myauctions 
[X] - mybids
[X] - showrecord

[X] - open
[X] - close
[X] - showasset
[X] - bid

