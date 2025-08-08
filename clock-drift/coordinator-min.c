/*
 * Simple Coordinator - Receives drift values via UDP
 */

 #include "contiki.h"
 #include "net/ipv6/simple-udp.h"
 #include "net/mac/tsch/tsch.h"
 #include <stdio.h>
 
 #define UDP_PORT 1234
 
 static struct simple_udp_connection udp_conn;
 
 /*---------------------------------------------------------------------------*/
 static void
 udp_rx_callback(struct simple_udp_connection *c,
                 const uip_ipaddr_t *sender_addr,
                 uint16_t sender_port,
                 const uip_ipaddr_t *receiver_addr,
                 uint16_t receiver_port,
                 const uint8_t *data,
                 uint16_t datalen)
 {
   if(datalen == sizeof(int)) {
     int drift = *((int*)data);
     printf("Drift received: %d\n", drift);
   }
 }
 
 /*---------------------------------------------------------------------------*/
 PROCESS(coordinator_process, "Coordinator");
 AUTOSTART_PROCESSES(&coordinator_process);
 
 PROCESS_THREAD(coordinator_process, ev, data)
 {
   PROCESS_BEGIN();
 
   simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
   
   printf("Coordinator started\n");
   
   NETSTACK_ROUTING.root_start();
   NETSTACK_MAC.on();
 
   while(1) {
     PROCESS_YIELD();
   }
 
   PROCESS_END();
 }