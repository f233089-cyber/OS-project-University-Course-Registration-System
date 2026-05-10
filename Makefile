# ============================================================
# Makefile – University Course Registration System
# CL-2006 Operating Systems Lab | Final Project
# ============================================================
# Usage:
#   make          – build both binaries
#   make run      – run the full simulation
#   make demo     – run the mandatory test scenario (Section 13)
#   make clean    – remove compiled binaries and log file
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -O2
TARGETS = registration test_scenario

.PHONY: all run demo clean

all: $(TARGETS)

registration: registration.c
	$(CC) $(CFLAGS) -o registration registration.c

test_scenario: test_scenario.c
	$(CC) $(CFLAGS) -o test_scenario test_scenario.c

run: registration
	./registration

demo: test_scenario
	./test_scenario

clean:
	rm -f $(TARGETS) registration_log.txt
