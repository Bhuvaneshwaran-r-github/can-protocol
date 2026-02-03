CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lgpiod

# Common sources
COMMON_SRC = can_utils.c

# Targets
all: dashboard_thread engine seatbelt door bcm

dashboard_thread: dashboard_thread.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@ -pthread

engine: engine.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

seatbelt: seatbelt.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

door: door.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

bcm: bcm.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f dashboard_thread engine seatbelt door bcm

.PHONY: all clean
