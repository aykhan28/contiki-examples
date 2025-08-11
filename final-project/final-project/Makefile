CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

# CC1352R SensorTag target
TARGET = simplelink

# Contiki-NG root dizini (kendi sisteminize göre ayarlayın)
CONTIKI = ../..

MODULES += os/net/app-layer/coap

# TSCH
MAKE_MAC = MAKE_MAC_TSCH

# Gerekli modüller
MODULES += os/net/ipv6
MODULES += os/net/routing
MODULES += os/net/mac/tsch

include $(CONTIKI)/Makefile.include


all: $(CONTIKI_PROJECT)