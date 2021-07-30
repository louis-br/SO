all: scheduler preempcao contab
scheduler:
	gcc -Wall -o ppos-scheduler ppos-core-aux.c pingpong-scheduler.c libppos_static.a
preempcao:
	gcc -Wall -o ppos-preempcao ppos-core-aux.c pingpong-preempcao.c libppos_static.a
contab:
	gcc -Wall -o ppos-contab ppos-core-aux.c pingpong-contab-prio.c libppos_static.a
clean:
	rm -f ppos-scheduler ppos-preempcao ppos-contab