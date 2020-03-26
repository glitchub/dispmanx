CFLAGS += -Wall -Werror -I/opt/vc/include/

LDFLAGS += -L/opt/vc/lib/ -lbcm_host

dispmanx: %: %.c

clean:; rm -f dispmanx
