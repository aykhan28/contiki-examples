/*
* Copyright (c) 2024, Clock Drift Monitor
* All rights reserved.
*
* TSCH Clock Drift Node
* Sends clock drift data to coordinator every 5 seconds
*/
#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/routing/routing.h"
#include "net/mac/tsch/tsch.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch-log.h"
#include "sys/clock.h"

#define LOG_MODULE "Node"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (5 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static int32_t last_measured_drift = 0;
static uint8_t drift_available = 0;

typedef struct {
    uint16_t node_id;
    int32_t clock_drift;
    uint32_t timestamp;
} drift_packet_t;

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "TSCH Clock Drift Node");
AUTOSTART_PROCESSES(&node_process);

/*---------------------------------------------------------------------------*/
/* Custom TSCH log processing to extract drift values */
static void
extract_drift_from_tsch_log(void)
{
  /* This function would be called to process TSCH logs and extract drift */
  /* For simulation purposes, we'll use a simple approach */
  
  /* In real implementation, this would parse TSCH log entries 
   * similar to tsch_log_process_pending() but extract drift values */
  
  static int32_t simulated_drift = 0;
  static uint8_t drift_counter = 0;
  
  /* Simulate some drift variation for demonstration */
  drift_counter++;
  if(drift_counter % 3 == 0) {
    simulated_drift += (node_id * 10 - 50); /* Some variation based on node ID */
    if(simulated_drift > 1000) simulated_drift = 1000;
    if(simulated_drift < -1000) simulated_drift = -1000;
    
    last_measured_drift = simulated_drift;
    drift_available = 1;
    
    LOG_INFO("Measured clock drift: %ld ppm\n", last_measured_drift);
  }
}

/*---------------------------------------------------------------------------*/
static void
send_drift_data(void)
{
  uip_ipaddr_t dest_ipaddr;
  drift_packet_t packet;
  
  if(NETSTACK_ROUTING.node_is_reachable() && 
     NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    
    /* Prepare packet */
    packet.node_id = node_id;
    packet.clock_drift = last_measured_drift;
    packet.timestamp = clock_seconds();
    
    LOG_INFO("Sending drift data to coordinator: drift=%ld ppm\n", packet.clock_drift);
    
    simple_udp_sendto(&udp_conn, &packet, sizeof(packet), &dest_ipaddr);
  } else {
    LOG_WARN("Not connected to network, cannot send drift data\n");
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer periodic_timer, drift_timer;
  
  PROCESS_BEGIN();

  LOG_INFO("Starting TSCH Clock Drift Node (Node ID: %u)\n", node_id);
  
  /* Initialize the UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);
  
  /* Enable TSCH */  
  NETSTACK_MAC.on();
  
  LOG_INFO("Node started. Will send drift data every %u seconds\n", SEND_INTERVAL / CLOCK_SECOND);
  
  /* Set up periodic timers */
  etimer_set(&periodic_timer, CLOCK_SECOND);
  etimer_set(&drift_timer, SEND_INTERVAL);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer) || 
                           etimer_expired(&drift_timer));
    
    if(etimer_expired(&periodic_timer)) {
      /* Process TSCH logs and extract drift information */
      //tsch_log_process_pending();
      extract_drift_from_tsch_log();
      
      etimer_reset(&periodic_timer);
    }
    
    if(etimer_expired(&drift_timer)) {
      /* Send drift data to coordinator */
      if(drift_available) {
        send_drift_data();
      } else {
        LOG_INFO("No drift data available yet\n");
      }
      
      etimer_reset(&drift_timer);
    }
  }

  PROCESS_END();
}