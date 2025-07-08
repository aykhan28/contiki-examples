#include "contiki.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/ipv6/uip.h"
#include "sys/node-id.h"
#include "net/linkaddr.h"
#include "sys/log.h"
#include "net/routing/routing.h"
#include <stdio.h>
#include <string.h>
#include "net/mac/tsch/tsch.h"

#define UDP_PORT 1234
#define SEND_INTERVAL (5 * CLOCK_SECOND)
#define LOG_LEVEL LOG_LEVEL_INFO

PROCESS(udp_node_process, "UDP Node with TSCH");
AUTOSTART_PROCESSES(&udp_node_process);

static struct simple_udp_connection udp_conn;
static struct etimer periodic_timer;
static uip_ipaddr_t dest_ipaddr;

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr, uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr, uint16_t receiver_port,
                const uint8_t *data, uint16_t datalen)
{
  char msg[datalen + 1];
  memcpy(msg, data, datalen);
  msg[datalen] = '\0';
  if (node_id == 1) { // Sunucu
    printf("Sunucu: '%s' mesaji alindi\n", msg);
    simple_udp_sendto(&udp_conn, "aleykum selam", 14, sender_addr);
  } else { // İstemci
    printf("Istemci: '%s' mesaji alindi\n", msg);
  }
}

PROCESS_THREAD(udp_node_process, ev, data)
{
  PROCESS_BEGIN();

  //NETSTACK_MAC.on();

  // Veri geldiğinde udp_rx_callback fonksiyonunu çalıştır
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);

  // ID'si 1 ise koordinatördür
  if(node_id == 1) {
    printf("Node %d bir sunucudur. UDP port %d dinleniyor\n", node_id, UDP_PORT);
    NETSTACK_ROUTING.root_start();
  } else {
    printf("Node %d bir istemcidir. RPL DAG'a katilmayi bekliyor...\n", node_id);

    // RPL DAG oluşana kadar bekle
    while(!NETSTACK_ROUTING.node_is_reachable()) {
      PROCESS_PAUSE();
    }

    // Root IP adresini al
    if(NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      printf("Sunucunun IP adresi bulundu, mesaj gondermeye baslanacak\n");
      LOG_INFO_6ADDR(&dest_ipaddr);
    } else {
      printf("HATA: Sunucu IP adresi alinamadi\n");
      PROCESS_EXIT();
    }
    etimer_set(&periodic_timer, SEND_INTERVAL);

    while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

      const char *msg = "selam";
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
      printf("Istemci %d: '%s' gonderildi\n", node_id, msg);

      etimer_reset(&periodic_timer);
    }
  }

  PROCESS_END();
}