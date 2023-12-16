You can compile both server and client together simply running `make`.
To compile each individually do `make server` or `make client` respectively

# Usage
Run both the `AS` and `user` with `-h` to get the usage help message.

# Configuration
The AS creates an ASDIR for its database on the working directory from where it is evoked.

The user and client use the port 58078 for both TCP and UDP.

Both the client and server have a set timeout of 5s to receive TCP and UDP responses.

The AS uses one thread for accepting TCP connections, 30 worker threads to serve the TCP connections and one thread to receive and serve UDP messages.

The default configuration above can be tweaked in the `utils/config.h` file.

#Task list
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

