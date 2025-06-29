#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include <stdio.h>

// Kullanacağımız UDP port numarası
#define UDP_PORT 1234

// Process tanımı ve otomatik başlatılması
PROCESS(udp_server_process, "UDP Server");
AUTOSTART_PROCESSES(&udp_server_process);

// UDP bağlantı yapısını tutacak değişken
static struct simple_udp_connection udp_conn;

/* Bu fonksiyon, UDP paketi geldiğinde çağrılır.
 * - c: bağlantı nesnesi
 * - sender_addr: gönderenin IPv6 adresi
 * - sender_port: gönderenin portu
 * - receiver_addr: bu node’un adresi
 * - receiver_port: bu node’un portu
 * - data: gelen veri
 * - datalen: veri uzunluğu
 */
static void udp_rx_callback(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen) {
  printf("1 - UDP mesaji alindi: '%.*s'\n", datalen, (char *) data);
}

PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  // UDP bağlantısını açıyoruz: her yerden (NULL) gelen paketleri dinleyecek
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);

  printf("2 - UDP Server baslatildi. Port: %d\n", UDP_PORT);

  PROCESS_END();
}