#include "contiki.h"
#include "sys/energest.h"
#include "sys/etimer.h"
#include <stdio.h>

PROCESS(energest_simple_process, "Simple Energest Test");
AUTOSTART_PROCESSES(&energest_simple_process);

PROCESS_THREAD(energest_simple_process, ev, data)
{
  static struct etimer timer;
  
  PROCESS_BEGIN();
  
  printf("Simple Energest Test Started\n");
  printf("Clock frequency: %lu Hz\n", (unsigned long)CLOCK_SECOND);
  
  etimer_set(&timer, 5 * CLOCK_SECOND);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    
    energest_flush();
    
    printf("\n=== Energest Values ===\n");
    printf("CPU:  %lu ticks\n", (unsigned long)energest_type_time(ENERGEST_TYPE_CPU));
    printf("LPM:  %lu ticks\n", (unsigned long)energest_type_time(ENERGEST_TYPE_LPM));
    printf("TX:   %lu ticks\n", (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT));
    printf("RX:   %lu ticks\n", (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN));
    printf("=======================\n");
    
    etimer_reset(&timer);
  }
  
  PROCESS_END();
}