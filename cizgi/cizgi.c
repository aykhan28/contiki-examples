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
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch-schedule.h"

#define DEBUG 0
#include "net/ipv6/uip-debug.h"

#define UDP_PORT 1234
#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_MODULE "KANAL-LOG"

#define TAHMINI_SLOTFRAME_BOYUTU 101

PROCESS(cizgi, "Cizgi");
PROCESS(kanal_log_process, "Kanal Log");
AUTOSTART_PROCESSES(&cizgi, &kanal_log_process);

static struct simple_udp_connection udp_conn;
static uip_ipaddr_t dest_ipaddr;
static struct etimer kanal_timer;

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr, uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr, uint16_t receiver_port,
                const uint8_t *data, uint16_t datalen)
{
  char msg[datalen + 1];
  memcpy(msg, data, datalen);
  msg[datalen] = '\0';
  printf("Sunucu: '%s' mesaji alindi\n", msg);
}

// Aktif kanalın bastırılması
static void log_channel_info(void) {
  if(tsch_is_associated) {
    int channels_len = sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE) / sizeof(uint8_t);

    uint16_t current_slot = tsch_current_asn.ls4b % TAHMINI_SLOTFRAME_BOYUTU;
    uint8_t channel_offset = current_slot % channels_len;
    printf("Aktif kanal: %d\n", TSCH_DEFAULT_HOPPING_SEQUENCE[channel_offset]);
    
  } else {
    printf("TSCH durumu: Henuz baglanmamis\n");
  }
}

PROCESS_THREAD(kanal_log_process, ev, data)
{
  PROCESS_BEGIN();
  
  etimer_set(&kanal_timer, CLOCK_SECOND * 60);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&kanal_timer));
    
    if(tsch_is_associated) {
      log_channel_info();
    }
    
    etimer_reset(&kanal_timer);
  }
  
  PROCESS_END();
}

// Çizgi topolojisinde bir düğümün en fazla kaç düğüm uzaklıktakı düğüme veri iletebileceği
PROCESS_THREAD(cizgi, ev, data)
{
  PROCESS_BEGIN();

  NETSTACK_MAC.on();
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);

  if(node_id == 1) {
    NETSTACK_ROUTING.root_start();
    printf("Root node baslatildi\n");
  } else {
    printf("Node %d baslaniyor, root'u bekliyor...\n", node_id);
    while(!NETSTACK_ROUTING.node_is_reachable()) {
        PROCESS_PAUSE();
    }
    
    if(NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      printf("Sunucunun IP adresi bulundu, mesaj gondermeye baslanacak\n");
    } else {
      printf("HATA: Sunucu IP adresi alinamadi\n");
      PROCESS_EXIT();
    }
    
    if (node_id==12) {
      const char *msg = "b";
      simple_udp_sendto(&udp_conn, msg, strlen(msg), &dest_ipaddr);
      printf("Istemci %d: '%s' gonderildi\n", node_id, msg);
    } else {
      printf("Ben ortadaki dugumum (Node %d)\n", node_id);
    }
  }

  PROCESS_END();
}