#include "contiki.h"
#include "sys/energest.h"
#include "sys/log.h"
#include "sys/etimer.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include <stdio.h>

#define LOG_MODULE "Energest"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Ölçüm periyodu (saniye) */
#define MEASUREMENT_INTERVAL 10

PROCESS(energest_monitor_process, "Energest Monitor Process");
AUTOSTART_PROCESSES(&energest_monitor_process);

/* Önceki ölçüm değerleri */
static unsigned long prev_cpu = 0;
static unsigned long prev_lpm = 0;
static unsigned long prev_deep_lpm = 0;
static unsigned long prev_tx = 0;
static unsigned long prev_rx = 0;
static unsigned long prev_listen = 0;

static void
print_energest_values(void)
{
  unsigned long current_cpu, current_lpm, current_deep_lpm;
  unsigned long current_tx, current_rx, current_listen;
  unsigned long delta_cpu, delta_lpm, delta_deep_lpm;
  unsigned long delta_tx, delta_rx, delta_listen;
  unsigned long total_time;
  
  /* Güncel değerleri al */
  energest_flush();
  
  current_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  current_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  current_deep_lpm = energest_type_time(ENERGEST_TYPE_DEEP_LPM);
  current_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  current_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  current_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  
  /* Delta değerlerini hesapla */
  delta_cpu = current_cpu - prev_cpu;
  delta_lpm = current_lpm - prev_lpm;
  delta_deep_lpm = current_deep_lpm - prev_deep_lpm;
  delta_tx = current_tx - prev_tx;
  delta_rx = current_rx - prev_rx;
  delta_listen = current_listen - prev_listen;
  
  total_time = delta_cpu + delta_lpm + delta_deep_lpm;
  
  printf("\n=== Energest Measurements (Last %d seconds) ===\n", MEASUREMENT_INTERVAL);
  printf("CPU Active:    %lu ticks (%.2f%%)\n", 
         (unsigned long)delta_cpu,
         total_time > 0 ? (100.0 * delta_cpu) / total_time : 0.0);
         
  printf("LPM:           %lu ticks (%.2f%%)\n", 
         (unsigned long)delta_lpm,
         total_time > 0 ? (100.0 * delta_lpm) / total_time : 0.0);
         
  printf("Deep LPM:      %lu ticks (%.2f%%)\n", 
         (unsigned long)delta_deep_lpm,
         total_time > 0 ? (100.0 * delta_deep_lpm) / total_time : 0.0);
         
  printf("Radio TX:      %lu ticks\n", (unsigned long)delta_tx);
  printf("Radio RX:      %lu ticks\n", (unsigned long)delta_rx);
  printf("Radio Listen:  %lu ticks\n", (unsigned long)delta_listen);
  printf("Total Time:    %lu ticks\n", (unsigned long)total_time);
  
  /* Kümülatif değerleri de yazdır */
  printf("\n=== Cumulative Values ===\n");
  printf("Total CPU:     %lu ticks\n", (unsigned long)current_cpu);
  printf("Total LPM:     %lu ticks\n", (unsigned long)current_lpm);
  printf("Total Deep LPM:%lu ticks\n", (unsigned long)current_deep_lpm);
  printf("Total TX:      %lu ticks\n", (unsigned long)current_tx);
  printf("Total RX:      %lu ticks\n", (unsigned long)current_rx);
  printf("Total Listen:  %lu ticks\n", (unsigned long)current_listen);
  printf("================================================\n\n");
  
  /* Önceki değerleri güncelle */
  prev_cpu = current_cpu;
  prev_lpm = current_lpm;
  prev_deep_lpm = current_deep_lpm;
  prev_tx = current_tx;
  prev_rx = current_rx;
  prev_listen = current_listen;
}

PROCESS_THREAD(energest_monitor_process, ev, data)
{
  static struct etimer periodic_timer;
  
  PROCESS_BEGIN();
  
  printf("Energest Monitor Started on CC1352R\n");
  printf("Clock frequency: %lu Hz\n", (unsigned long)CLOCK_SECOND);
  printf("Measurement interval: %d seconds\n", MEASUREMENT_INTERVAL);
  
  /* Radio'yu başlat */
  NETSTACK_RADIO.on();
  
  /* İlk değerleri kaydet */
  energest_flush();
  prev_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  prev_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  prev_deep_lpm = energest_type_time(ENERGEST_TYPE_DEEP_LPM);
  prev_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  prev_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  prev_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  
  /* Periyodik timer'ı başlat */
  etimer_set(&periodic_timer, MEASUREMENT_INTERVAL * CLOCK_SECOND);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    /* Enerji ölçümlerini yazdır */
    print_energest_values();
    
    /* Timer'ı yeniden başlat */
    etimer_reset(&periodic_timer);
  }
  
  PROCESS_END();
}