#include "contiki.h"
#include "net/routing/routing.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include <stdio.h>

struct __attribute__((__packed__)) sensor_packet {
  uint16_t node_id;
  uint16_t light;           // 100x scale
  int16_t  temperature;     // 100x scale
  uint16_t humidity_air;    // 100x scale
  uint32_t humidity_ground; // percentage
  int16_t  rx_drift;
  int16_t  tx_drift;
};

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

static struct simple_udp_connection udp_conn;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
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
  if(datalen == sizeof(struct sensor_packet)) {
    struct sensor_packet *pkt = (struct sensor_packet *)data;
    
    // Network byte order'dan host byte order'a çevir
    uint16_t node_id = UIP_HTONS(pkt->node_id);
    uint16_t light = UIP_HTONS(pkt->light);
    int16_t temperature = UIP_HTONS(pkt->temperature);
    uint16_t humidity_air = UIP_HTONS(pkt->humidity_air);
    uint32_t humidity_ground = UIP_HTONL(pkt->humidity_ground);
    int16_t rx_drift = UIP_HTONS(pkt->rx_drift);
    int16_t tx_drift = UIP_HTONS(pkt->tx_drift);
    
    printf("Node %d: Light=%d.%02d, Temp=%d.%02d, Humid_air=%d.%02d, Humid_ground=%ld%%, RX_drift=%d, TX_drift=%d\n",
           node_id,
           light/100, light%100,
           temperature/100, temperature%100,
           humidity_air/100, humidity_air%100,
           humidity_ground,
           rx_drift, tx_drift);
  } else {
    printf("Invalid packet size received: %d bytes\n", datalen);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();
  NETSTACK_ROUTING.root_start();

  NETSTACK_MAC.on();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/ 