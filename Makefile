SRC=sump.c sump_ctrl.c sump_data.c sump_util.c
CMD=gcc -g -O0  -fPIC -shared -o /usr/lib64/ump.so $^ -ldl

all: ${SRC}
	${CMD}

# debug: sump.c sump_ctrl.c sump_data.c sump_util.c
# 	gcc -g -O0  -fPIC -shared -o /usr/lib64/ump.so $^ -ldl -DDEBUG_OUT
debug: ${SRC}
	${CMD} -DDEBUG_OUT


# 负载均衡算法
time: ${SRC}
	${CMD} -DTIME

robin: ${SRC}
	${CMD} -DROBIN

queue: ${SRC}
	${CMD} -DQUEUE

random: ${SRC}
	${CMD} -DRANDOM

weight: ${SRC}
	${CMD} -DWEIGHT

hash: ${SRC}
	${CMD} -DHASH
	
rate: ${SRC}
	${CMD} -DRATE

avg_rate: ${SRC}
	${CMD} -DAVG_RATE
	

run:
	cd ./nvme; sudo make; sudo make install; cd ../; cd ./nvmf; sudo make; sudo make install; cd ../; cd ./iscsi; sudo make; sudo make install; cd ../
	@LD_PRELOAD=/usr/lib64/ump.so ../../build/bin/vhost -m 0x2 -r /var/tmp/vhost.sock 

push:
	git add .; git commit -m update; git push; git push gitee
