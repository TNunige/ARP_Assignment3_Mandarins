# Assignment description
The project is an interactive simulator for a drone with two degrees of freedom.
The drone is operated by keys of the keyboard: 8 directions, plus keys for stopping, resetting, and quitting.
The drone dynamics is a 2 degrees of freedom dot with mass (inertia) and viscous resistance. Any key pressed increases (decreases if reversed) in steps a force pushing the drone in the appropriate direction.
The sides of the operation window are obstacles as well. They simulate the geo-fences used in drone operations.

# Assignment 3


## Installation & Usage
For the project's configuration, we have used `Makefile`.

To build executables, run this :
```
make
```
in the project directory.

Then move to `Build`:
```
cd /Build
```
To start the game, run this:
```
./master
```

To remove the executables and logs, run this:
```
make clean
```

```
make clean logs
```
To connect the socket as client, run this:
```
./master clients <hostname> <portnumber>

```
To connect the socket as server, run this: 
```
./master server <portnumber>
```





###  Operational instructions, controls ###
To operate the drone use the following keybindings
- `w`: move left up
- `s`: move left
- `x`: move left down
- `e`: move up
- `c`: move down
- `r`: move right up
- `f`: move right
- `v`: move right down
- `d`: brake
- `k`: restart
- `esc` : exit



## Overview 

![ARP_Assignment3_architecture](https://github.com/TNunige/ARP_Assignment3_Mandarins/assets/145358917/ad35bdc5-b901-4158-9200-637781fc670b)




The components are 9 processes:
- Master
- Server
- Window (User interface)
- Watchdog
- Drone
- Keyboard (User interface)
- Targets
- Obstacles
- Socket Server

All of the above mentioned components were created.

### Master
Spawns all other processes mentioned above. There are two ways to start the game. First when entering the server argument (with port number) when running the master then all the processes except targets and obstacles are spawned. Otherwise, when running the master with argument client (with hostname and port number) then only targets and obstacles are spawned. Master process waits until all other processes have been terminated and then terminates. 

### Server
Server communicates with the other processes through pipes and logs the information it receives. It is the central process for all communication. The process receives and sends data to processes drone, window and socket server. The server checks the data exchanged via pipes and updates the contents to a logfile. Additionally, it periodically sends a signal to the watchdog after a certain number of iterations. Upon exiting the loop, it closes the used file descriptors and terminates.


### Watchdog
The watchdog process's job is to monitor the activities of all the other processes (except the master). Watchdog monitors window, drone, keyboard, server, and socket server processes. At the beginning of the process, it writes its PID to a temporary file for the other processes to read its PID and reads the PIDs of other monitored child processes from temporary files. We have chosen to implement a watchdog that only receives signals from monitored processes. It receives signal SIGUSR1 from other child processes, indicating that child processes are active during the monitored period. In the infinite loop, it monitors the elapsed time since the last data reception from each child process. When the elapsed time exceeds the timeout, the watchdog sends SIGKILL signals to all monitored processes and terminates all child processes.

### Window
The window process creates the game window and displays the drone, obstacles, and targets positions using the ncurses library. It communicates with other processes via pipes, sending and receiving the necessary data. The window process dynamically updates the drone's position based on calculations made in the drone process. Additionally, it updates obstacle positions after a certain time interval , based on the data sent by the server process. Targets are positioned according to the data from the server process and are updated whenever the drone reaches a target. Upon reaching a target, the window process increments the score and removes the reached target from the window. When reaching all the targets it sends a game end message to the server. Furthermore, the window process periodically sends a signal to the watchdog process to indicate its activity.


### Drone
The drone process models the drone character movement by dynamically calculating the force impacting the drone based on the user key input (direction), command, and repulsive force(border and obstacles). The repulsive force is activated when the drone is near the game window borders or obstacles. The process calculates the forces based on the received user key from the keyboard process. It utilizes the dynamic motion equation to determine the new position of the drone considering the sum of input forces and repulsive forces. Subsequently, the drone process sends the drone's new position and the user key input to the server via pipes. Additionally, it periodically sends a signal to the watchdog process to inform its activity.

### Keyboard 
The keyboard handles the user key inputs and displays messages on the inspection window. It scans the key pressed by the user by the getch() command in the ncurses library and sends the values of the pressed key to the drone process through a FIFO (named pipe). Additionally, it periodically sends a signal to the watchdog process to inform its activity.

### Targets
The target process serves as a client for socket communication. Initially, it establishes a connection with the socket server through specified port and hostname and sends an initial message to ensure connectivity. Subsequently, the process receives the window size from the server to determine the target coordinates. Then the process generates initial target positions and sends them to the server.
It continuously waits for messages from the server, processing them accordingly. If it receives a game end message, it generates new target positions and sends them to the server. Upon receiving an exit message, it logs the event and terminates.



### Obstacles
The obstacle process serves as a client for socket communication. Initially, it establishes a connection with the socket server through specified port and hostname and sends an initial message to ensure connectivity. Subsequently, the process receives the window size from the server to determine the obstacle coordinates. The process then generates initial obstacle positions and sends them to the server. After a certain time interval, it updates the obstacle positions and sends the new coordinates to the server. It continuously waits for messages from the server and processes accordingly. Upon receiving an exit message, it logs the event and terminates.

### Socket Server
The socket server process is a socket server designed to handle communication with multiple clients simultaneously. The process binds the socket to a specified port and starts listening for incoming connections. Additionally, it utilizes pipes to communicate with the server process. Upon receiving  the initial message from clients, it sends the window size to them. When data is received from either clients or the server process, it processes the information accordingly. If the data from the server indicates a game end or an exit signal, it sends corresponding messages to the clients. In cases where clients provide data about target positions or obstacle positions, it sends them to the server via pipes. Furthermore, the socket server process periodically sends a signal to the watchdog process to indicate its activity.



### Constants.h ###
All the necessary constants and structures are defined here.




   




