#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/ipv6/uip-ds6.h"
#include <stdio.h>

// UDP port numarası — alıcı ile aynı olmalı
#define UDP_PORT 1234

// Mesaj gönderme aralığı (saniye cinsinden)
#define SEND_INTERVAL CLOCK_SECOND

// Process tanımı ve otomatik başlatma
PROCESS(udp_client_process, "UDP Client");
AUTOSTART_PROCESSES(&udp_client_process);

// UDP bağlantısı için değişken
static struct simple_udp_connection udp_conn;
// Etimer değişkeni — zamanlama için kullanılır
static struct etimer periodic_timer;

PROCESS_THREAD(udp_client_process, ev, data)
{
  // Alıcının (server'ın) IPv6 adresi
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  // Server node’un link-local IPv6 adresini manuel tanımlıyoruz (Cooja için yaygın adres)
  // Not: Bu adres Cooja’da genelde 02:01:00:00:00:01 olan node’un adresidir
  uip_ip6addr(&dest_ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7401, 0x0001, 0x0101);

  // UDP bağlantısı başlatılır — hedef adres ve port belirtilir
  simple_udp_register(&udp_conn, UDP_PORT, &dest_ipaddr, UDP_PORT, NULL);

  // İlk gönderim için zamanlayıcı ayarlanır
  etimer_set(&periodic_timer, SEND_INTERVAL);

  while(1) {
    // Timer dolana kadar bekle
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    // Timer’ı sıfırla
    etimer_reset(&periodic_timer);

    // Gönderilecek mesaj
    char msg[] = "3 - Merhaba UDP";
    // UDP mesajı gönderilir
    simple_udp_send(&udp_conn, msg, sizeof(msg));
    printf("4 - UDP mesaji gönderildi\n");
  }

  PROCESS_END();
}