#include "contiki.h"
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

#include "arch/cpu/simplelink-cc13xx-cc26xx/lib/coresdk_cc13xx_cc26xx/source/ti/drivers/I2C.h"

struct __attribute__((__packed__)) sensor_packet {
  uint16_t node_id;
  uint16_t light;           // 100x scale
  int16_t  temperature;     // 100x scale
  uint16_t humidity_air;    // 100x scale
  uint32_t humidity_ground; // percentage
  int16_t  rx_drift;
  int16_t  tx_drift;
};

#define ADS1115_ADDR 0x48
#define CONFIG_REG 0x01
#define CONV_REG   0x00

/* Kalibrasyon değerleri (mV cinsinden) */
#define DRY_VOLTAGE_MV  2900   // 3.0V = 3000mV (kuru hava)
#define WET_VOLTAGE_MV  0   // 1.0V = 1000mV (tam ıslak)
#define VOLTAGE_RANGE_MV (DRY_VOLTAGE_MV - WET_VOLTAGE_MV)

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SENSOR_READING_PERIOD (CLOCK_SECOND * 5)

/* I2C_Handle tanımlama */
static I2C_Handle i2cHandle;
I2C_Params i2cParams;

/* I2C işlemi için transaction yapısı */
static I2C_Transaction i2cTransaction;

/* Config verisi */
static uint8_t config_data[3] = {
  CONFIG_REG,
  0xC3, // A0, ±4.096V, single-shot
  0x83  // 128 SPS
};

static struct simple_udp_connection udp_conn;
static struct ctimer opt_timer, hdc_timer;

/* Sensör değerleri */
static int tempValue = 0;
static int humidValue = 0;
static int optValue = 0;

// Dfrobot statics
static uint8_t read_buf[2];
static uint8_t reg_addr = CONV_REG;
static int16_t adc_value;
static int32_t voltage_mV, moisture_percentage;
static bool status;

// Clock drift değerleri
static int rx_drift;
static int tx_drift;

static uip_ipaddr_t dest_ipaddr;

PROCESS(final_project, "Final Project");
AUTOSTART_PROCESSES(&final_project);

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
send_data(struct sensor_packet *pkt)
{
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    simple_udp_sendto(&udp_conn, pkt, sizeof(struct sensor_packet), &dest_ipaddr);
    printf("Sensor data sent\n");
  } else {
    printf("Network not ready, data not sent\n");
  }
}

static void
get_hdc_reading()
{
  clock_time_t next = SENSOR_READING_PERIOD;

  tempValue = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);

  humidValue = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);

  ctimer_set(&hdc_timer, next, init_hdc_reading, NULL);
}

static void
get_light_reading()
{
  clock_time_t next = SENSOR_READING_PERIOD;

  optValue = opt_3001_sensor.value(0);

  ctimer_set(&opt_timer, next, init_opt_reading, NULL);
}

void send_drift_if_tx(struct tsch_log_t *log) {
    if(log->type == tsch_log_tx && log->tx.drift_used) {
      uip_ipaddr_t dest_ipaddr;
      if(NETSTACK_ROUTING.node_is_reachable() && 
         NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
            tx_drift = log->tx.drift;
        }
    } else if(log->type == tsch_log_rx && log->rx.drift_used) {
        uip_ipaddr_t dest_ipaddr;
        if(NETSTACK_ROUTING.node_is_reachable() && 
           NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
            rx_drift = log->rx.drift;
        }
    }
}

static void init_dfrobot() {
    /* I2C parametrelerini başlat ve I2C'yi aç */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_100kHz;
}

void dfrobot_sensor() {
    /* 1. Konfigürasyon yazma işlemi */
    i2cTransaction.slaveAddress = ADS1115_ADDR;
    i2cTransaction.writeBuf = config_data;
    i2cTransaction.writeCount = 3;
    i2cTransaction.readBuf = NULL;
    i2cTransaction.readCount = 0;

    status = I2C_transfer(i2cHandle, &i2cTransaction);
    if (!status) {
      return;
    }

    /* Dönüşüm için 10 ms bekle */
    clock_delay_usec(10000);

    /* 2. Okuma işlemi */
    i2cTransaction.writeBuf = &reg_addr;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = read_buf;
    i2cTransaction.readCount = 2;

    status = I2C_transfer(i2cHandle, &i2cTransaction);
    if (!status) {
      return;
    }

    /* Raw değeri hesapla */
    adc_value = (read_buf[0] << 8) | read_buf[1];
    
    /* Voltajı mV cinsinden hesapla (4096 = 4.096V * 1000) */
    voltage_mV = (adc_value * 4096L) / 32767;
    
    /* Nem yüzdesini hesapla (0-100 arası) */
    if (voltage_mV >= DRY_VOLTAGE_MV) {
      moisture_percentage = 0;
    } else if (voltage_mV <= WET_VOLTAGE_MV) {
      moisture_percentage = 100;
    } else {
      /* Integer hesaplama: (kuru - ölçülen) / (kuru - ıslak) * 100 */
      moisture_percentage = ( (DRY_VOLTAGE_MV - voltage_mV) * 100 ) / VOLTAGE_RANGE_MV;
    }
}

PROCESS_THREAD(final_project, ev, data)
{
  static struct etimer sensor_timer;
  
  PROCESS_BEGIN();
  
  /* TSCH MAC katmanını başlat */
  NETSTACK_MAC.on();

  init_dfrobot();

  /* Sensörleri başlat */
  init_sensors();
  init_sensor_readings();
  
  /* UDP bağlantısını başlat */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);
  
  /* Periyodik timer'ı başlat */
  etimer_set(&sensor_timer, SENSOR_READING_PERIOD);
  
  while(1) {
    PROCESS_YIELD();
    if(ev == sensors_event) {
      if(data == &opt_3001_sensor) {
        get_light_reading();
      } else if(data == &hdc_1000_sensor) {
        get_hdc_reading();
      }
    } else if(ev == PROCESS_EVENT_TIMER && etimer_expired(&sensor_timer)) {
      static struct sensor_packet pkt;
      
      i2cHandle = I2C_open(0, &i2cParams);
      dfrobot_sensor();
      I2C_close(i2cHandle);
      
      pkt.node_id = UIP_HTONS(node_id);
      pkt.light = UIP_HTONS((uint16_t)optValue);
      pkt.temperature = UIP_HTONS((int16_t)tempValue);
      pkt.humidity_air = UIP_HTONS((uint16_t)humidValue);
      pkt.humidity_ground = UIP_HTONL((uint32_t)moisture_percentage);
      pkt.rx_drift = UIP_HTONS((int16_t)rx_drift);
      pkt.tx_drift = UIP_HTONS((int16_t)tx_drift);
      
      send_data(&pkt);
      etimer_reset(&sensor_timer);
    }
  }
  
  PROCESS_END();
}