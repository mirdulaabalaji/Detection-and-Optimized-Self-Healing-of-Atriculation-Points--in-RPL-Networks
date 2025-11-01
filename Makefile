CONTIKI = /home/sid/contiki-ng
TARGET = native

CFLAGS += -DPLATFORM_MAIN_ACCEPTS_ARGS=1

# Link math library
LDFLAGS += -lm

CONTIKI_PROJECT = rpl_cutvertex_detection
all: $(CONTIKI_PROJECT)

# Disable IPv6 if not needed
CONTIKI_WITH_IPV6 = 0

include $(CONTIKI)/Makefile.include