// Bunlar çizgi topolojisinde cooja'da 21 düğüm uzaktaki düğüme veri iletebilmemizi sağlar
#define COOJA_RADIO_CONF_BUFSIZE 1000
#define PACKETBUF_CONF_SIZE 1000

// Kanal sayısını azaltmak denemeleri hızlandırması için (gerçekte tavsiye edilmez)
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_1_1