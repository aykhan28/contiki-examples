#!/usr/bin/env python3

import serial
import requests
import json
import sqlite3
import time
import re
from datetime import datetime
import os

# Seri port ayarları
SERIAL_PORT = '/dev/ttyACM0'     # Veya '/dev/serial0', '/dev/ttyAMA0'
BAUD_RATE = 115200

# HTTP hedef ayarları
TARGET_URL = 'http://10.142.1.191:5000/data'  # HTTP URL
ERROR_URL = 'http://10.142.1.191:5000/error'  # Hata bildirimi için endpoint

# SQLite veritabanı ayarları
DB_PATH = 'offline_data.db'

# Hata kontrol ayarları
DATA_TIMEOUT = 600  # 10 dakika (saniye cinsinden)
ERROR_REPORT_INTERVAL = 600  # 10 dakika (saniye cinsinden) - Hata mesajı gönderme aralığı

class SensorDataSender:
    def __init__(self):
        self.init_database()
        self.serial_connected = False
        self.last_data_time = None
        self.last_serial_error_time = 0
        self.last_data_timeout_error_time = 0

    def init_database(self):
        """SQLite veritabanını başlat"""
        self.conn = sqlite3.connect(DB_PATH)
        cursor = self.conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS offline_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                data TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        self.conn.commit()
        print(f"Veritabanı hazır: {DB_PATH}")
        
    def init_serial(self):
        """Seri portu başlat"""
        try:
            self.ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            self.serial_connected = True
            print(f"Seri port açıldı: {SERIAL_PORT}")
            return True
        except serial.SerialException as e:
            self.serial_connected = False
            print(f"Seri port açılamadı: {e}")
            return False
            
    def get_timestamp(self):
        """ISO formatında zaman damgası döndür"""
        return datetime.now().isoformat()
        
    def send_error_to_server(self, error_type, error_message):
        """Hata mesajını sunucuya gönder"""
        payload = {
            'error_type': error_type,
            'error_message': error_message,
            'timestamp': self.get_timestamp()
        }
        
        try:
            response = requests.post(
                ERROR_URL,
                json=payload,
                timeout=3
            )
            
            if response.status_code == 200:
                print(f"Hata mesajı sunucuya gönderildi: {error_type}")
                return True
            else:
                print(f"Hata mesajı gönderilemedi: {response.status_code}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"Hata mesajı gönderme başarısız: {e}")
            return False
    
    def check_and_report_errors(self):
        """
        Hataları kontrol et ve 10 dakikada bir sunucuya bildir
        """
        current_time = time.time()
        
        # Seri port hata kontrolü
        if not self.serial_connected:
            # Son hata mesajından 10 dakika geçtiyse tekrar gönder
            if current_time - self.last_serial_error_time >= ERROR_REPORT_INTERVAL:
                print(f"HATA: Seri port bağlı değil - Sunucuya bildiriliyor")
                self.send_error_to_server(
                    'serial_port_error',
                    'Seri port bağlantısı kurulamadı veya kesildi'
                )
                self.last_serial_error_time = current_time
        
        # Veri timeout kontrolü (sadece seri port bağlıyken)
        if self.serial_connected and self.last_data_time is not None:
            time_since_last_data = current_time - self.last_data_time
            
            # 10 dakikadan fazla veri gelmemişse
            if time_since_last_data > DATA_TIMEOUT:
                # Son hata mesajından 10 dakika geçtiyse tekrar gönder
                if current_time - self.last_data_timeout_error_time >= ERROR_REPORT_INTERVAL:
                    print(f"HATA: {int(time_since_last_data)} saniyedir veri gelmiyor - Sunucuya bildiriliyor")
                    self.send_error_to_server(
                        'data_timeout',
                        f'{int(time_since_last_data)} saniyedir veri gelmedi'
                    )
                    self.last_data_timeout_error_time = current_time
        
    def parse_sensor_data(self, data_str):
        """Sensör verisini parse et ve JSON formatına çevir"""
        try:
            # Regex pattern ile veriyi parse et
            # Node 17277: Light=14.69, Temp=29.71, Humid_air=54.27, Humid_ground=100%, RX_drift=6, TX_drift=0
            pattern = r'Node\s+(\d+):\s+Light=([0-9.]+),\s+Temp=([0-9.]+),\s+Humid_air=([0-9.]+),\s+Humid_ground=([0-9]+)%,\s+RX_drift=([0-9-]+),\s+TX_drift=([0-9-]+)'
            
            match = re.match(pattern, data_str.strip())
            
            if match:
                node_id = int(match.group(1))
                light = float(match.group(2))
                temp = float(match.group(3))
                humid_air = float(match.group(4))
                humid_ground = int(match.group(5))
                rx_drift = int(match.group(6))
                tx_drift = int(match.group(7))
                
                # JSON formatında veri oluştur
                parsed_data = {
                    "node_id": node_id,
                    "light": light,
                    "temperature": temp,
                    "humidity_air": humid_air,
                    "humidity_ground": humid_ground,
                    "rx_drift": rx_drift,
                    "tx_drift": tx_drift,
                    "timestamp": self.get_timestamp()
                }
                
                print(f"Veri parse edildi - Node ID: {node_id}")
                return json.dumps(parsed_data)
            else:
                print(f"Veri formatı eşleşmedi: {data_str}")
                return None
                
        except Exception as e:
            print(f"Veri parse hatası: {e} - Veri: {data_str}")
            return None
        
    def save_to_database(self, data, timestamp):
        """Veriyi yerel veritabanına kaydet"""
        try:
            cursor = self.conn.cursor()
            cursor.execute(
                "INSERT INTO offline_data (data, timestamp) VALUES (?, ?)",
                (data, timestamp)
            )
            self.conn.commit()
            print(f"Veri yerel veritabanına kaydedildi")
        except sqlite3.Error as e:
            print(f"Veritabanı kayıt hatası: {e}")
            
    def send_to_server(self, data, timestamp):
        """Veriyi sunucuya gönder"""
        # data zaten JSON string, önce parse et
        try:
            parsed_data = json.loads(data)
        except json.JSONDecodeError:
            print(f"JSON parse hatası: {data}")
            return False
        
        # Sunucunun beklediği format
        payload = {
            'data': data,  # JSON string olarak gönder
            'timestamp': timestamp
        }
        
        try:
            response = requests.post(
                TARGET_URL,
                json=payload,
                timeout=3
            )
            
            if response.status_code == 200:
                print(f"Veri başarıyla gönderildi: {response.status_code}")
                return True
            else:
                print(f"Sunucu hatası: {response.status_code} - {response.text}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"HTTP isteği başarısız: {e}")
            return False
            
    def send_offline_data(self):
        """Yerel veritabanındaki verileri sunucuya gönder"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT id, data, timestamp FROM offline_data ORDER BY id")
        offline_records = cursor.fetchall()
        
        if not offline_records:
            return
            
        print(f"Yerel veritabanında {len(offline_records)} kayıt bulundu, gönderiliyor...")
        
        successful_ids = []
        
        for record_id, data, timestamp in offline_records:
            if self.send_to_server(data, timestamp):
                successful_ids.append(record_id)
            else:
                # İlk başarısız gönderimde dur
                break
                
        # Başarıyla gönderilen kayıtları sil
        if successful_ids:
            placeholders = ','.join('?' * len(successful_ids))
            cursor.execute(f"DELETE FROM offline_data WHERE id IN ({placeholders})", successful_ids)
            self.conn.commit()
            print(f"Yerel veritabanından {len(successful_ids)} kayıt silindi")
            
    def check_connection(self):
        """Sunucu bağlantısını kontrol et"""
        try:
            response = requests.get(
                TARGET_URL.replace('/data', '/ping'),  # Ping endpoint'i varsa
                timeout=2
            )
            return True
        except:
            # Ping endpoint'i yoksa normal endpoint'i dene
            try:
                response = requests.post(
                    TARGET_URL,
                    json={'test': 'connection'},
                    timeout=2
                )
                return True
            except:
                return False
                
    def run(self):
        """Ana döngü"""
        print("Sensör veri aktarımı başlatıldı...")
        print("Tüm gelen veriler sunucuya gönderilecek.")
        print(f"Hedef URL: {TARGET_URL}")
        print(f"Hata URL: {ERROR_URL}")
        print(f"Hata raporlama aralığı: {ERROR_REPORT_INTERVAL} saniye (10 dakika)\n")
        
        # Program başladığında offline verileri göndermeyi dene
        if self.check_connection():
            print("Sunucu bağlantısı mevcut, offline veriler kontrol ediliyor...")
            self.send_offline_data()
        else:
            print("Sunucu bağlantısı yok, offline modda başlanıyor...")
        
        # Seri portu başlat
        if not self.init_serial():
            print("Seri port bağlanamadı, ilk hata mesajı sunucuya gönderiliyor...")
            self.send_error_to_server(
                'serial_port_error',
                'Seri port bağlantısı kurulamadı'
            )
            self.last_serial_error_time = time.time()
            
        buffer = ""  # Veri biriktirme buffer'ı
        last_error_check_time = time.time()
        serial_retry_time = time.time()
        SERIAL_RETRY_INTERVAL = 30  # 30 saniyede bir seri port bağlantısını dene
            
        try:
            while True:
                current_time = time.time()
                
                # Seri port bağlı değilse periyodik olarak bağlanmayı dene
                if not self.serial_connected:
                    if current_time - serial_retry_time > SERIAL_RETRY_INTERVAL:
                        print("Seri port bağlantısı deneniyor...")
                        if self.init_serial():
                            print("Seri port bağlantısı başarılı!")
                            self.last_data_time = time.time()  # Bağlantı kurulduğunda zamanı sıfırla
                        serial_retry_time = current_time
                    
                    # Hata kontrolü ve raporlama (10 dakikada bir)
                    if current_time - last_error_check_time >= 60:  # Her 1 dakikada kontrol et
                        self.check_and_report_errors()
                        last_error_check_time = current_time
                    
                    time.sleep(1)
                    continue
                
                # Seri port bağlıysa veri oku
                try:
                    if self.ser.in_waiting > 0:
                        # Gelen veriyi buffer'a ekle
                        new_data = self.ser.read(self.ser.in_waiting).decode(errors='ignore')
                        buffer += new_data
                        
                        # Satır sonlarına göre veriyi böl
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()
                            
                            if line:
                                # Veriyi parse et
                                parsed_json = self.parse_sensor_data(line)
                                
                                if parsed_json:
                                    timestamp = self.get_timestamp()
                                    print(f"İşlenen veri: {line}")
                                    
                                    # Veri gelme zamanını güncelle
                                    self.last_data_time = time.time()
                                    
                                    # Önce sunucuya göndermeyi dene
                                    if self.send_to_server(parsed_json, timestamp):
                                        # Başarılı gönderim sonrası offline verileri kontrol et
                                        self.send_offline_data()
                                    else:
                                        # Başarısız gönderim, yerel veritabanına kaydet
                                        self.save_to_database(parsed_json, timestamp)
                                else:
                                    # Parse edilemeyen veriler için bilgi ver
                                    print(f"Parse edilemeyen veri atlandı: {line}")
                
                except serial.SerialException as e:
                    print(f"Seri port okuma hatası: {e}")
                    self.serial_connected = False
                    if hasattr(self, 'ser'):
                        try:
                            self.ser.close()
                        except:
                            pass
                    
                    # Hata zamanını güncelle
                    self.last_serial_error_time = time.time()
                    self.send_error_to_server(
                        'serial_port_error',
                        f'Seri port bağlantısı kesildi: {str(e)}'
                    )
                    
                    serial_retry_time = time.time()
                    continue
                
                # Her 1 dakikada bir hata kontrolü yap (10 dakikada bir gönderim yapılacak)
                if current_time - last_error_check_time >= 60:
                    self.check_and_report_errors()
                    last_error_check_time = current_time

                # Kısa bir bekleme
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\nProgram durduruldu.")
        finally:
            self.cleanup()
            
    def cleanup(self):
        """Temizlik işlemleri"""
        if hasattr(self, 'ser') and self.serial_connected:
            try:
                self.ser.close()
                print("Seri port kapatıldı")
            except:
                pass
        if hasattr(self, 'conn'):
            self.conn.close()
            print("Veritabanı bağlantısı kapatıldı")

if __name__ == "__main__":
    sender = SensorDataSender()
    sender.run()
