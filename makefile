all: master server window drone keyboard watchdog server_socket2 targets_client obstacles_client

clean:
	rm -f Build/master Build/server Build/window Build/drone Build/keyboard Build/watchdog Build/server_socket2 Build/targets_client Build/obstacles_client

clean-logs:
	rm Log/*

master: master.c
	gcc master.c -o Build/master

server: server.c
	gcc server.c -o Build/server

window: window.c
	gcc window.c -o Build/window -lncurses -lm

drone: drone.c
	gcc drone.c -o Build/drone -lm

keyboard: keyboard.c
	gcc keyboard.c -o Build/keyboard -lncurses

watchdog: watchdog.c
	gcc watchdog.c -o Build/watchdog

server_socket2: server_socket2.c
	gcc server_socket2.c -o Build/server_socket2	

targets_client: targets_client.c
	gcc targets_client.c -o Build/targets_client	

obstacles_client: obstacles_client.c
	gcc obstacles_client.c -o Build/obstacles_client		

