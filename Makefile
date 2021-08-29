DISK_SCHEDULER ?= disk_mgr_scheduler
all: disco1 #scheduler preempcao contab
scheduler:
	gcc -Wall -o ppos-scheduler ppos-core-aux.c pingpong-scheduler.c libppos_static.a
preempcao:
	gcc -Wall -o ppos-preempcao ppos-core-aux.c pingpong-preempcao.c libppos_static.a
contab:
	gcc -Wall -o ppos-contab ppos-core-aux.c pingpong-contab-prio.c libppos_static.a
disco1:	
	cp disk-original.dat disk.dat
	gcc -Wall -o disco1 ppos_disk.c disk.c pingpong-disco1.c ppos-core-aux.c libppos_static.a -lrt -DDEBUG -DDISK_MGR_SCHEDULER=$(DISK_SCHEDULER)
disco2:	
	cp disk-original.dat disk.dat
	gcc -Wall -o disco2 ppos_disk.c disk.c pingpong-disco2.c ppos-core-aux.c libppos_static.a -lrt -DDISK_MGR_SCHEDULER=$(DISK_SCHEDULER)
clean:
	rm -f ppos-scheduler ppos-preempcao ppos-contab disco1 disco2