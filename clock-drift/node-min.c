/*
 * Simple Node - Sends drift values via UDP when TSCH TX happens
 */

 #include "contiki.h"
 #include "net/routing/routing.h"
 #include "net/ipv6/simple-udp.h"
 #include "net/mac/tsch/tsch.h"
 #include "lib/ringbufindex.h"
 #include <stdio.h>
 
 #define UDP_PORT 1234
 
 static struct simple_udp_connection udp_conn;
 
 /* Hook function to capture drift from TSCH logs */
void send_drift_if_tx(struct tsch_log_t *log) {
    if(log->type == tsch_log_tx && log->tx.drift_used) {
      uip_ipaddr_t dest_ipaddr;
      if(NETSTACK_ROUTING.node_is_reachable() && 
         NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        printf("Sending drift TX: %d\n", log->tx.drift);
        simple_udp_sendto(&udp_conn, &log->tx.drift, sizeof(int), &dest_ipaddr);
      }
    } else if(log->type == tsch_log_rx && log->rx.drift_used) {
        uip_ipaddr_t dest_ipaddr;
        if(NETSTACK_ROUTING.node_is_reachable() && 
           NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
          printf("Sending drift RX: %d\n", log->rx.drift);
          simple_udp_sendto(&udp_conn, &log->rx.drift, sizeof(int), &dest_ipaddr);
        }
      }
    /*
    Buranın çalışması için os/net/mac/tsch/tsch-log.c dosyasında ringbufindex_get(&log_ringbuf); bu satırın üstüne aşağıdakı eklenmeli:
    #ifdef SEND_DRIFT_CALLBACK
    void send_drift_if_tx(struct tsch_log_t *log) __attribute__((weak));
    if(send_drift_if_tx) {
      send_drift_if_tx(log);
    }
    #endif */
  }
 
 /*---------------------------------------------------------------------------*/
 PROCESS(node_process, "Node");
 AUTOSTART_PROCESSES(&node_process);
 
 PROCESS_THREAD(node_process, ev, data)
 {
   PROCESS_BEGIN();
 
   simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, NULL);
   
   printf("Node started\n");
   
   NETSTACK_MAC.on();
 
   while(1) {
     PROCESS_YIELD();
   }
 
   PROCESS_END();
 }