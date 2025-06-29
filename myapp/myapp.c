#include "contiki.h"
#include <stdio.h>
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/rtimer.h"
#include "sys/log.h"

// 1. Process tanımlama
PROCESS(etimer_tutorial, "Event timer");
PROCESS(etimer_restart_example, "Etimer Restart Örneği");
PROCESS(ctimer_tutorial, "Callback timer");
PROCESS(rtimer_logger, "Rtimer Logger Process");

// 2. Otomatik başlatma
AUTOSTART_PROCESSES(&etimer_tutorial, &etimer_restart_example, &ctimer_tutorial, &rtimer_logger);

/******************************************************************/

PROCESS_THREAD(etimer_tutorial, ev, data)
{
	static struct etimer zamanlayici;
	static int sayac = 0;

	PROCESS_BEGIN();

	// 4. Zamanlayıcıyı 2 saniye olarak ayarlama
	etimer_set(&zamanlayici, CLOCK_SECOND * 5);

	while(1) {
		// 5. Zamanlayıcı dolana kadar bekle
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER); // veya PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&zamanlayici));

		// 6. Her dolumda ekrana yaz
		printf("[etimer_tutorial] Bu bir zamanlanmis event\n");

		// 7. Zamanlayıcıyı sıfırla (böylece sürekli tekrar eder)
		etimer_reset(&zamanlayici);

		sayac++;

		if (sayac >= 5) {
			printf("[etimer_tutorial] Sayac 5 oldu, zamanlayici durduruluyor :)\n");
			etimer_stop(&zamanlayici);
		}
	}

	PROCESS_END();

	/*
		etimer'ın 5 fonksiyonu var:
			1. etimer_set - zamanlayıcı tanımlamak için
			2. etimer_reset - zamanlayıcıyı sıfırlamak için
			3. etimer_stop - zamanlayıcıyı sonlandırmak için
			4. etimer_expired - zamanlayicının doluluğunu kontrol için
			5. etimer_restart - zamanlayıcıyı kalan süreye göre yeniden başlatır
	*/
}

PROCESS_THREAD(etimer_restart_example, ev, data)
{
  static struct etimer timer;
  static int tekrar = 0;

  PROCESS_BEGIN();

  // Başlangıçta 5 saniyelik zamanlayıcı ayarla
  etimer_set(&timer, CLOCK_SECOND * 3);

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == PROCESS_EVENT_TIMER && etimer_expired(&timer)) {
      tekrar++;
      printf("[etimer_restart_example] Restart ile tekrar: %d\n", tekrar);

      if(tekrar >= 3) {
        printf("[etimer_restart_example] 3 tekrar tamamlandi.\n");
        etimer_stop(&timer);
      } else {
        etimer_restart(&timer); // Dolduktan sonra tekrar başlat (kalan zaman hesaba katılır)
      }
    }
  }

  PROCESS_END();
}

/******************************************************************/

static struct ctimer zamanlayici_ctimer;
static int ctimer_sayac = 0;

// Bu fonksiyon zamanlayıcı dolunca çağrılacak
void zamanlayici_callback(void *ptr)
{
  ctimer_sayac++;
  printf("[ctimer_tutorial] Callback tetiklendi. Sayac: %d\n", ctimer_sayac);

  if(ctimer_sayac < 4) {
    // Kendini yeniden başlat
    ctimer_set(&zamanlayici_ctimer, CLOCK_SECOND * 10, zamanlayici_callback, NULL);
  } else {
    printf("[ctimer_tutorial] 4 tekrar tamamlandi. Zamanlayici durdu.\n");
  }
}

PROCESS_THREAD(ctimer_tutorial, ev, data)
{
  PROCESS_BEGIN();

  printf("[ctimer_tutorial] Ctimer baslatiliyor\n");

  ctimer_set(&zamanlayici_ctimer, CLOCK_SECOND * 3, zamanlayici_callback, NULL);

  PROCESS_END();

  /*
	ctimer etimer'dan farklı olarak event'e göre çalışmaz. Sadece verilen zamana göre çalışır.
  */
}

/******************************************************************/

#define LOG_MODULE "RTimerApp"
#define LOG_LEVEL LOG_LEVEL_DBG

static struct rtimer my_rtimer;
volatile static int rtimer_fired = 0;
static int rtimer_sayac = 0;

void rtimer_callback(struct rtimer *t, void *ptr) {
  rtimer_fired = 1;
}

PROCESS_THREAD(rtimer_logger, ev, data)
{
  PROCESS_BEGIN();

  LOG_DBG("RTimer baslatiliyor\n");
  rtimer_set(&my_rtimer, RTIMER_NOW() + RTIMER_SECOND, 1, rtimer_callback, NULL);

  while(1) {
    if(rtimer_fired) {
      LOG_DBG("RTimer tetiklendi\n");
      rtimer_fired = 0;
      rtimer_sayac++;

      if (rtimer_sayac == 30) {
      	LOG_DBG("Rtimer durduruluyor\n");
      	break;
      }
      // Yeniden başlatama
      rtimer_set(&my_rtimer, RTIMER_NOW() + RTIMER_SECOND, 1, rtimer_callback, NULL);
    }

    PROCESS_PAUSE();
  }

  PROCESS_END();
}