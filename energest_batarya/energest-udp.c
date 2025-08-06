#include "contiki.h"
#include "sys/energest.h"
#include "sys/log.h"
#include "sys/etimer.h"
#include "net/routing/routing.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include <stdio.h>
#include <string.h>

#define LOG_MODULE "Energest"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Ölçüm periyodu (saniye) - 5 dakika */
#define MEASUREMENT_INTERVAL 300

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;

PROCESS(energest_monitor_process, "Energest Monitor Process");
AUTOSTART_PROCESSES(&energest_monitor_process);

static void
send_energest_data(void)
{
  unsigned long current_cpu, current_lpm, current_deep_lpm;
  unsigned long current_tx, current_rx, current_listen;
  char buffer[200];
  
  /* Güncel toplam değerleri al */
  energest_flush();
  
  current_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  current_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  current_deep_lpm = energest_type_time(ENERGEST_TYPE_DEEP_LPM);
  current_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  current_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  current_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  
  /* JSON formatında toplam veriyi hazırla */
  snprintf(buffer, sizeof(buffer),
    "{\"total_cpu\":%lu,\"total_lpm\":%lu,\"total_deep_lpm\":%lu,\"total_tx\":%lu,\"total_rx\":%lu,\"total_listen\":%lu}",
    current_cpu, current_lpm, current_deep_lpm, current_tx, current_rx, current_listen
  );
  
  /* UDP ile server'a gönder */
  uip_ipaddr_t dest_ipaddr;
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    simple_udp_sendto(&udp_conn, buffer, strlen(buffer), &dest_ipaddr);
    printf("Energest data sent: %s\n", buffer);
  } else {
    printf("Network not ready, data not sent\n");
  }
}

PROCESS_THREAD(energest_monitor_process, ev, data)
{
  static struct etimer periodic_timer;
  
  PROCESS_BEGIN();
  
  printf("Energest UDP Client Started on CC1352R\n");
  printf("Clock frequency: %lu Hz\n", (unsigned long)CLOCK_SECOND);
  printf("Measurement interval: %d seconds (5 minutes)\n", MEASUREMENT_INTERVAL);
  
  /* TSCH MAC katmanını başlat */
  NETSTACK_MAC.on();
  
  /* UDP bağlantısını başlat */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);
  
  /* Periyodik timer'ı başlat */
  etimer_set(&periodic_timer, MEASUREMENT_INTERVAL * CLOCK_SECOND);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    /* Toplam enerji verilerini UDP ile gönder */
    send_energest_data();
    
    /* Timer'ı yeniden başlat */
    etimer_reset(&periodic_timer);
  }
  
  PROCESS_END();
}