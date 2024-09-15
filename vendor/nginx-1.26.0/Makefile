
default:	build

clean:
	rm -rf Makefile objs

.PHONY:	default clean

build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

modules:
	$(MAKE) -f objs/Makefile modules

upgrade:
	/home/gyl/work/mprpc/vendor/nginx-1.26.0/sbin/nginx -t

	kill -USR2 `cat /home/gyl/work/mprpc/vendor/nginx-1.26.0/logs/nginx.pid`
	sleep 1
	test -f /home/gyl/work/mprpc/vendor/nginx-1.26.0/logs/nginx.pid.oldbin

	kill -QUIT `cat /home/gyl/work/mprpc/vendor/nginx-1.26.0/logs/nginx.pid.oldbin`

.PHONY:	build install modules upgrade
