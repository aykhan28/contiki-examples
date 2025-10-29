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

# SQLite veritabanı ayarları
DB_PATH = 'offline_data.db'

class SensorDataSender:
    def __init__(self):
        self.init_database()
        self.init_serial()

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
            print(f"Seri port açıldı: {SERIAL_PORT}")
        except serial.SerialException as e:
            print(f"Seri port açılamadı: {e}")
            exit(1)
            
    def get_timestamp(self):
        """ISO formatında zaman damgası döndür"""
        return datetime.now().isoformat()
        
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
        print(f"Hedef URL: {TARGET_URL}\n")
        
        # Program başladığında offline verileri göndermeyi dene
        if self.check_connection():
            print("Sunucu bağlantısı mevcut, offline veriler kontrol ediliyor...")
            self.send_offline_data()
        else:
            print("Sunucu bağlantısı yok, offline modda başlanıyor...")
            
        buffer = ""  # Veri biriktirme buffer'ı
            
        try:
            while True:
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

                # Kısa bir bekleme
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\nProgram durduruldu.")
        finally:
            self.cleanup()
            
    def cleanup(self):
        """Temizlik işlemleri"""
        if hasattr(self, 'ser'):
            self.ser.close()
            print("Seri port kapatıldı")
        if hasattr(self, 'conn'):
            self.conn.close()
            print("Veritabanı bağlantısı kapatıldı")

if __name__ == "__main__":
    sender = SensorDataSender()
    sender.run()