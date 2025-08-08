/*
 * TSCH Coordinator - Receives drift data from nodes via UDP
 */

 #include "contiki.h"
 #include "net/routing/routing.h"
 #include "net/netstack.h"
 #include "net/ipv6/simple-udp.h"
 #include "sys/log.h"
 #include "net/mac/tsch/tsch.h"
 
 #define LOG_MODULE "Coordinator"
 #define LOG_LEVEL LOG_LEVEL_INFO
 
 #define UDP_CLIENT_PORT 8765
 #define UDP_SERVER_PORT 5678
 
 static struct simple_udp_connection udp_conn;
 
 /* Structure for drift data packet */
 typedef struct {
   uint16_t node_id;
   int32_t current_drift;
   int32_t estimated_drift;
   uint32_t timestamp;
   uint8_t valid;
 } drift_packet_t;
 
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
   if(datalen == sizeof(drift_packet_t)) {
     drift_packet_t *packet = (drift_packet_t *)data;
     
     printf("[COORDINATOR] Received drift from node %u:\n", packet->node_id);
     printf("  Current Drift: %ld\n", (long)packet->current_drift);
     printf("  Estimated Drift: %ld\n", (long)packet->estimated_drift);
     printf("  Timestamp: %lu\n", (unsigned long)packet->timestamp);
     printf("  Valid: %s\n", packet->valid ? "Yes" : "No");
     printf("  From: ");
     printf("\n");
     
     LOG_INFO("Node %u drift: curr=%ld, est=%ld, ts=%lu\n", 
              packet->node_id, (long)packet->current_drift, 
              (long)packet->estimated_drift, (unsigned long)packet->timestamp);
   } else {
     printf("[COORDINATOR] Received invalid packet size: %d\n", datalen);
   }
 }
 
 /*---------------------------------------------------------------------------*/
 PROCESS(coordinator_process, "TSCH Drift Coordinator");
 AUTOSTART_PROCESSES(&coordinator_process);
 
 PROCESS_THREAD(coordinator_process, ev, data)
 {
   PROCESS_BEGIN();
 
   /* Initialize the server */
   simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                       UDP_CLIENT_PORT, udp_rx_callback);
 
   printf("TSCH Drift Coordinator started\n");
   printf("Listening on UDP port %u\n", UDP_SERVER_PORT);
   LOG_INFO("TSCH Drift Coordinator started on port %u\n", UDP_SERVER_PORT);
 
   /* Set as coordinator */
   NETSTACK_ROUTING.root_start();
   NETSTACK_MAC.on();
   /* Start TSCH */
   tsch_set_coordinator(linkaddr_cmp(&linkaddr_node_addr, &linkaddr_null));
   
   printf("Coordinator address: ");
   printf("\n");
   
   /* Main loop */
   while(1) {
     PROCESS_YIELD();
   }
 
   PROCESS_END();
 }