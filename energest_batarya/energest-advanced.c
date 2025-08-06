#include "contiki.h"
#include "sys/energest.h"
#include "sys/log.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "net/routing/routing.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "random.h"
#include "net/netstack.h"
#include "batmon-sensor.h"
#include "board-peripherals.h"
#include "sys/node-id.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define LOG_MODULE "Energest"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Ölçüm periyodu (saniye) - 5 dakika */
#define MEASUREMENT_INTERVAL (CLOCK_SECOND * 10)

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SENSOR_READING_PERIOD (CLOCK_SECOND * 5)

static struct simple_udp_connection udp_conn;
static struct ctimer opt_timer, hdc_timer;

/* Sensör değerleri */
static int tempValue = 0;
static int humidValue = 0;
static int optValue = 0;

static uip_ipaddr_t dest_ipaddr;


PROCESS(energest_monitor_process, "Energest Monitor Process");
AUTOSTART_PROCESSES(&energest_monitor_process);

/* Sensör fonksiyonları */
static void
init_sensors(void)
{
  SENSORS_ACTIVATE(batmon_sensor);
}

static void
init_sensor_readings(void)
{
#if BOARD_SENSORTAG
  SENSORS_ACTIVATE(opt_3001_sensor);
  SENSORS_ACTIVATE(hdc_1000_sensor);
#endif
}

static void
init_opt_reading(void *not_used)
{
  SENSORS_ACTIVATE(opt_3001_sensor);
}

static void
init_hdc_reading(void *not_used)
{
  SENSORS_ACTIVATE(hdc_1000_sensor);
}

static void
send_data(char *data, int length)
{
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    simple_udp_sendto(&udp_conn, data, length, &dest_ipaddr);
    printf("Data sent: %s\n", data);
  } else {
    printf("Network not ready, data not sent\n");
  }
}
static void
get_hdc_reading()
{
  clock_time_t next = SENSOR_READING_PERIOD;

  tempValue = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
  if(tempValue != HDC_1000_READING_ERROR) {
    printf("HDC: Temp=%d.%02d C\n", tempValue / 100, tempValue % 100);
  } else {
    printf("HDC: Temp Read Error\n");
  }

  humidValue = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);
  if(humidValue != HDC_1000_READING_ERROR) {
    printf("HDC: Humidity=%d.%02d %%RH\n", humidValue / 100, humidValue % 100);
  } else {
    printf("HDC: Humidity Read Error\n");
  }

  ctimer_set(&hdc_timer, next, init_hdc_reading, NULL);
}

static void
get_light_reading()
{
  clock_time_t next = SENSOR_READING_PERIOD;

  optValue = opt_3001_sensor.value(0);
  if(optValue != OPT_3001_READING_ERROR) {
    printf("OPT: Light=%d.%02d lux\n", optValue / 100, optValue % 100);
  } else {
    printf("OPT: Light Read Error\n");
  }

  ctimer_set(&opt_timer, next, init_opt_reading, NULL);
}

static void
send_energest_data(void)
{
  unsigned long current_cpu, current_lpm, current_deep_lpm;
  unsigned long current_tx, current_rx, current_listen;
  char buffer[300];
  
  /* Güncel toplam değerleri al */
  energest_flush();
  
  current_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  current_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  current_deep_lpm = energest_type_time(ENERGEST_TYPE_DEEP_LPM);
  current_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  current_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  current_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  
  /* JSON formatında toplam veriyi ve sensör değerlerini hazırla */
  snprintf(buffer, sizeof(buffer),
    "{\"is_energest\":1,\"node_id\":%d,\"total_cpu\":%lu,\"total_lpm\":%lu,\"total_deep_lpm\":%lu,\"total_tx\":%lu,\"total_rx\":%lu,\"total_listen\":%lu}",
    node_id, current_cpu, current_lpm, current_deep_lpm, current_tx, current_rx, current_listen
  );
  
  /* UDP ile server'a gönder */
  send_data(buffer, strlen(buffer));
}

PROCESS_THREAD(energest_monitor_process, ev, data)
{
  static struct etimer sensor_timer;
  static struct etimer energest_timer;
  static char str[128];
  
  PROCESS_BEGIN();
  
  printf("Energest + Sensor UDP Client Started on CC1352R\n");
  printf("Clock frequency: %lu Hz\n", (unsigned long)CLOCK_SECOND);
  printf("Measurement interval: %d seconds (5 minutes)\n", MEASUREMENT_INTERVAL);
  printf("Node ID: %d\n", node_id);
  
  /* TSCH MAC katmanını başlat */
  NETSTACK_MAC.on();
  
  /* Sensörleri başlat */
  init_sensors();
  init_sensor_readings();
  
  /* UDP bağlantısını başlat */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);
  
  /* Periyodik timer'ı başlat */
  etimer_set(&sensor_timer, SENSOR_READING_PERIOD);
  etimer_set(&energest_timer, MEASUREMENT_INTERVAL);
  
  while(1) {
    PROCESS_YIELD();
    
    if(ev == sensors_event) {
      if(data == &opt_3001_sensor) {
        get_light_reading();
      } else if(data == &hdc_1000_sensor) {
        get_hdc_reading();
      }
    } else if(ev == PROCESS_EVENT_TIMER && etimer_expired(&sensor_timer)) {
      snprintf(str, sizeof(str),
      "{\"is_energest\":0,\"node_id\":%d,\"light\":%d.%02d,\"temperature\":%d.%02d,\"humidity\":%d.%02d}",
          node_id, 
          optValue / 100, optValue % 100,
          tempValue / 100, tempValue % 100,
          humidValue / 100, humidValue % 100);
        send_data(str, strlen(str));
        etimer_reset(&sensor_timer);
    } else if (ev == PROCESS_EVENT_TIMER && etimer_expired(&energest_timer)) {
        /* Toplam enerji verilerini ve sensör değerlerini UDP ile gönder */
        send_energest_data();
        etimer_reset(&energest_timer);
    }
  }
  
  PROCESS_END();
}