CONTIKI_PROJECT = energest-advanced
all: $(CONTIKI_PROJECT)

# CC1352R SensorTag target
TARGET = simplelink
BOARD = sensortag/cc1352r1

# Contiki-NG root dizini (kendi sisteminize göre ayarlayın)
CONTIKI = ../..

# Energest modülünü etkinleştir
MAKE_WITH_ENERGEST = 1

# TSCH
MAKE_MAC = MAKE_MAC_TSCH

# Gerekli modüller
MODULES += os/net/ipv6
MODULES += os/net/routing
MODULES += os/net/mac/tsch

include $(CONTIKI)/Makefile.include

#CONTIKI_PROJECT = energest-advanced
#all: $(CONTIKI_PROJECT)
#
## CC1352R SensorTag target
#TARGET = simplelink
#BOARD = sensortag/cc1352r1
#
## Contiki-NG root dizini (kendi sisteminize göre ayarlayın)
#CONTIKI = ../..
#
## Energest modülünü etkinleştir
#MAKE_WITH_ENERGEST = 1
#
## Debug modunu etkinleştir
#MAKE_WITH_LOG = 1
#
## Serial baud rate
#MAKE_WITH_SERIAL_LINE_INPUT = 1
#
## Radio modüllerini dahil et
#MODULES += os/net/nullnet
#
#include $(CONTIKI)/Makefile.include