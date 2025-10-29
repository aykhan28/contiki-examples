#!/usr/bin/env python3
from flask import Flask, request, jsonify, render_template_string
import sqlite3
import json
from datetime import datetime
import os

app = Flask(__name__)

# Veritabanı ayarları
DB_PATH = 'sensor_data.db'
ERROR_DB_PATH = 'error_logs.db'

def init_database():
    """Veritabanını başlat"""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Önce tabloyu sil (eğer varsa)
    cursor.execute('DROP TABLE IF EXISTS sensor_data')
    
    # Yeniden oluştur
    cursor.execute('''
        CREATE TABLE sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id INTEGER NOT NULL,
            light REAL,
            temperature REAL,
            humidity_air REAL,
            humidity_ground INTEGER,
            rx_drift INTEGER,
            tx_drift INTEGER,
            timestamp TEXT NOT NULL,
            received_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    
    conn.commit()
    conn.close()
    print(f"Veritabanı yeniden oluşturuldu: {DB_PATH}")

def init_error_database():
    """Hata veritabanını başlat"""
    conn = sqlite3.connect(ERROR_DB_PATH)
    cursor = conn.cursor()
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS error_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            error_type TEXT NOT NULL,
            error_message TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            received_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    
    conn.commit()
    conn.close()
    print(f"Hata veritabanı hazır: {ERROR_DB_PATH}")

def get_db_connection():
    """Veritabanı bağlantısı al"""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def get_error_db_connection():
    """Hata veritabanı bağlantısı al"""
    conn = sqlite3.connect(ERROR_DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

# HTML Template
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sensör Veri Monitörü</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .tabs {
            display: flex;
            justify-content: center;
            margin-bottom: 20px;
            gap: 10px;
        }
        
        .tab-btn {
            background: rgba(255, 255, 255, 0.2);
            border: 2px solid rgba(255, 255, 255, 0.3);
            color: white;
            padding: 12px 24px;
            border-radius: 25px;
            cursor: pointer;
            font-size: 16px;
            transition: all 0.3s ease;
            backdrop-filter: blur(10px);
        }
        
        .tab-btn.active {
            background: rgba(255, 255, 255, 0.4);
            font-weight: bold;
        }
        
        .tab-btn:hover {
            background: rgba(255, 255, 255, 0.3);
            transform: translateY(-2px);
        }
        
        .tab-content {
            display: none;
        }
        
        .tab-content.active {
            display: block;
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .stat-card {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 15px;
            padding: 20px;
            text-align: center;
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
        }
        
        .stat-number {
            font-size: 2em;
            font-weight: bold;
            color: #667eea;
            margin-bottom: 5px;
        }
        
        .stat-label {
            color: #666;
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .controls {
            text-align: center;
            margin-bottom: 20px;
        }
        
        .btn {
            background: rgba(255, 255, 255, 0.2);
            border: 2px solid rgba(255, 255, 255, 0.3);
            color: white;
            padding: 12px 24px;
            border-radius: 25px;
            cursor: pointer;
            font-size: 16px;
            margin: 0 10px;
            transition: all 0.3s ease;
            backdrop-filter: blur(10px);
        }
        
        .btn:hover {
            background: rgba(255, 255, 255, 0.3);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        }
        
        .data-table {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 15px;
            overflow: hidden;
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
            backdrop-filter: blur(10px);
            overflow-x: auto;
        }
        
        table {
            width: 100%;
            border-collapse: collapse;
            min-width: 900px;
        }
        
        th {
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
            padding: 15px;
            text-align: left;
            font-weight: 600;
            white-space: nowrap;
        }
        
        td {
            padding: 12px 15px;
            border-bottom: 1px solid #eee;
            white-space: nowrap;
        }
        
        tbody tr:hover {
            background-color: #f8f9ff;
        }
        
        .node-badge {
            background: #667eea;
            color: white;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 0.8em;
            font-weight: bold;
        }
        
        .sensor-value {
            font-weight: 600;
            color: #333;
        }
        
        .temperature { color: #e74c3c; }
        .humidity { color: #3498db; }
        .light { color: #f39c12; }
        .drift { color: #9b59b6; }
        
        .timestamp {
            color: #666;
            font-size: 0.9em;
        }
        
        .error-badge {
            background: #e74c3c;
            color: white;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 0.8em;
            font-weight: bold;
        }
        
        .refresh-info {
            text-align: center;
            color: rgba(255, 255, 255, 0.8);
            margin-top: 20px;
            font-size: 0.9em;
        }
        
        .loading {
            text-align: center;
            padding: 40px;
            color: #667eea;
            font-size: 1.2em;
        }
        
        @media (max-width: 768px) {
            .stats-grid {
                grid-template-columns: 1fr;
            }
            
            .header h1 {
                font-size: 2em;
            }
            
            table {
                font-size: 0.9em;
            }
            
            th, td {
                padding: 10px 8px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌡️ Sensör Veri Monitörü</h1>
            <p>Gerçek zamanlı sensör verilerini ve hata loglarını izleyin</p>
        </div>
        
        <div class="tabs">
            <button class="tab-btn active" onclick="showTab('sensor-tab')">📊 Sensör Verileri</button>
            <button class="tab-btn" onclick="showTab('error-tab')">⚠️ Hata Logları</button>
        </div>
        
        <!-- Sensör Verileri Tab -->
        <div id="sensor-tab" class="tab-content active">
            <div class="stats-grid">
                <div class="stat-card">
                    <div class="stat-number" id="totalRecords">{{ stats.total_records }}</div>
                    <div class="stat-label">Toplam Kayıt</div>
                </div>
                <div class="stat-card">
                    <div class="stat-number" id="activeNodes">{{ stats.active_nodes }}</div>
                    <div class="stat-label">Aktif Node</div>
                </div>
                <div class="stat-card">
                    <div class="stat-number" id="avgTemp">{{ "%.1f°C"|format(stats.avg_temp or 0) }}</div>
                    <div class="stat-label">Ortalama Sıcaklık</div>
                </div>
                <div class="stat-card">
                    <div class="stat-number" id="avgHumidity">{{ "%.1f%%"|format(stats.avg_humidity or 0) }}</div>
                    <div class="stat-label">Ortalama Hava Nemi</div>
                </div>
            </div>
            
            <div class="controls">
                <button class="btn" onclick="refreshData()">🔄 Yenile</button>
                <button class="btn" onclick="clearData()">🗑️ Verileri Temizle</button>
                <button class="btn" onclick="toggleAutoRefresh()">⏱️ Otomatik Yenileme</button>
            </div>
            
            <div class="data-table">
                <table>
                    <thead>
                        <tr>
                            <th>Node ID</th>
                            <th>🌡️ Sıcaklık</th>
                            <th>💧 Hava Nemi</th>
                            <th>🌱 Toprak Nemi</th>
                            <th>💡 Işık</th>
                            <th>📡 RX Drift</th>
                            <th>📡 TX Drift</th>
                            <th>🕒 Zaman</th>
                            <th>📅 Alınma Zamanı</th>
                        </tr>
                    </thead>
                    <tbody id="dataTable">
                        {% for row in data %}
                        <tr>
                            <td><span class="node-badge">{{ row.node_id }}</span></td>
                            <td><span class="sensor-value temperature">{{ "%.2f°C"|format(row.temperature) }}</span></td>
                            <td><span class="sensor-value humidity">{{ "%.2f%%"|format(row.humidity_air) }}</span></td>
                            <td><span class="sensor-value humidity">{{ row.humidity_ground }}%</span></td>
                            <td><span class="sensor-value light">{{ "%.2f lux"|format(row.light) }}</span></td>
                            <td><span class="sensor-value drift">{{ row.rx_drift }}</span></td>
                            <td><span class="sensor-value drift">{{ row.tx_drift }}</span></td>
                            <td><span class="timestamp">{{ row.timestamp[:19] }}</span></td>
                            <td><span class="timestamp">{{ row.received_at[:19] }}</span></td>
                        </tr>
                        {% endfor %}
                    </tbody>
                </table>
            </div>
        </div>
        
        <!-- Hata Logları Tab -->
        <div id="error-tab" class="tab-content">
            <div class="stats-grid">
                <div class="stat-card">
                    <div class="stat-number" id="totalErrors">{{ error_stats.total_errors }}</div>
                    <div class="stat-label">Toplam Hata</div>
                </div>
                <div class="stat-card">
                    <div class="stat-number" id="lastErrorTime">{{ error_stats.last_error_time or "Yok" }}</div>
                    <div class="stat-label">Son Hata Zamanı</div>
                </div>
                <div class="stat-card">
                    <div class="stat-number" id="errorTypes">{{ error_stats.error_types }}</div>
                    <div class="stat-label">Farklı Hata Tipi</div>
                </div>
            </div>
            
            <div class="controls">
                <button class="btn" onclick="refreshErrorData()">🔄 Hata Loglarını Yenile</button>
                <button class="btn" onclick="clearErrorData()">🗑️ Hata Loglarını Temizle</button>
            </div>
            
            <div class="data-table">
                <table>
                    <thead>
                        <tr>
                            <th>Hata Tipi</th>
                            <th>Hata Mesajı</th>
                            <th>🕒 Zaman</th>
                            <th>📅 Alınma Zamanı</th>
                        </tr>
                    </thead>
                    <tbody id="errorTable">
                        {% for error in error_data %}
                        <tr>
                            <td><span class="error-badge">{{ error.error_type }}</span></td>
                            <td><span class="sensor-value">{{ error.error_message }}</span></td>
                            <td><span class="timestamp">{{ error.timestamp[:19] }}</span></td>
                            <td><span class="timestamp">{{ error.received_at[:19] }}</span></td>
                        </tr>
                        {% endfor %}
                    </tbody>
                </table>
            </div>
        </div>
        
        <div class="refresh-info">
            <p>Son güncelleme: <span id="lastUpdate">{{ current_time }}</span></p>
            <p id="autoRefreshStatus">Otomatik yenileme: Kapalı</p>
        </div>
    </div>

    <script>
        let autoRefreshInterval = null;
        let autoRefreshEnabled = false;
        let currentTab = 'sensor-tab';
        
        function showTab(tabId) {
            // Tüm tab içeriklerini gizle
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            
            // Tüm tab butonlarını pasif yap
            document.querySelectorAll('.tab-btn').forEach(btn => {
                btn.classList.remove('active');
            });
            
            // Seçilen tab'ı göster
            document.getElementById(tabId).classList.add('active');
            
            // Seçilen tab butonunu aktif yap
            event.target.classList.add('active');
            
            currentTab = tabId;
            
            // Tab değiştiğinde verileri yenile
            if (tabId === 'sensor-tab') {
                refreshData();
            } else {
                refreshErrorData();
            }
        }
        
        function updateLastUpdateTime() {
            document.getElementById('lastUpdate').textContent = new Date().toLocaleString('tr-TR');
        }
        
        async function refreshData() {
            try {
                const response = await fetch('/api/data');
                const result = await response.json();
                
                // İstatistikleri güncelle
                document.getElementById('totalRecords').textContent = result.stats.total_records;
                document.getElementById('activeNodes').textContent = result.stats.active_nodes;
                document.getElementById('avgTemp').textContent = (result.stats.avg_temp || 0).toFixed(1) + '°C';
                document.getElementById('avgHumidity').textContent = (result.stats.avg_humidity || 0).toFixed(1) + '%';
                
                // Tabloyu güncelle
                const tbody = document.getElementById('dataTable');
                tbody.innerHTML = '';
                
                result.data.forEach(row => {
                    const tr = document.createElement('tr');
                    tr.innerHTML = `
                        <td><span class="node-badge">${row.node_id}</span></td>
                        <td><span class="sensor-value temperature">${row.temperature.toFixed(2)}°C</span></td>
                        <td><span class="sensor-value humidity">${row.humidity_air.toFixed(2)}%</span></td>
                        <td><span class="sensor-value humidity">${row.humidity_ground}%</span></td>
                        <td><span class="sensor-value light">${row.light.toFixed(2)} lux</span></td>
                        <td><span class="sensor-value drift">${row.rx_drift}</span></td>
                        <td><span class="sensor-value drift">${row.tx_drift}</span></td>
                        <td><span class="timestamp">${row.timestamp.substring(0, 19)}</span></td>
                        <td><span class="timestamp">${row.received_at.substring(0, 19)}</span></td>
                    `;
                    tbody.appendChild(tr);
                });
                
                updateLastUpdateTime();
            } catch (error) {
                console.error('Veri yenilenirken hata:', error);
            }
        }
        
        async function refreshErrorData() {
            try {
                const response = await fetch('/api/errors');
                const result = await response.json();
                
                // İstatistikleri güncelle
                document.getElementById('totalErrors').textContent = result.stats.total_errors;
                document.getElementById('lastErrorTime').textContent = result.stats.last_error_time || 'Yok';
                document.getElementById('errorTypes').textContent = result.stats.error_types;
                
                // Tabloyu güncelle
                const tbody = document.getElementById('errorTable');
                tbody.innerHTML = '';
                
                result.data.forEach(error => {
                    const tr = document.createElement('tr');
                    tr.innerHTML = `
                        <td><span class="error-badge">${error.error_type}</span></td>
                        <td><span class="sensor-value">${error.error_message}</span></td>
                        <td><span class="timestamp">${error.timestamp.substring(0, 19)}</span></td>
                        <td><span class="timestamp">${error.received_at.substring(0, 19)}</span></td>
                    `;
                    tbody.appendChild(tr);
                });
                
                updateLastUpdateTime();
            } catch (error) {
                console.error('Hata verileri yenilenirken hata:', error);
            }
        }
        
        async function clearData() {
            if (confirm('Tüm sensör verilerini silmek istediğinizden emin misiniz?')) {
                try {
                    const response = await fetch('/api/clear', { method: 'POST' });
                    if (response.ok) {
                        alert('Veriler başarıyla temizlendi!');
                        refreshData();
                    }
                } catch (error) {
                    console.error('Veri silinirken hata:', error);
                    alert('Veriler silinirken hata oluştu!');
                }
            }
        }
        
        async function clearErrorData() {
            if (confirm('Tüm hata loglarını silmek istediğinizden emin misiniz?')) {
                try {
                    const response = await fetch('/api/clear_errors', { method: 'POST' });
                    if (response.ok) {
                        alert('Hata logları başarıyla temizlendi!');
                        refreshErrorData();
                    }
                } catch (error) {
                    console.error('Hata logları silinirken hata:', error);
                    alert('Hata logları silinirken hata oluştu!');
                }
            }
        }
        
        function toggleAutoRefresh() {
            autoRefreshEnabled = !autoRefreshEnabled;
            const statusElement = document.getElementById('autoRefreshStatus');
            
            if (autoRefreshEnabled) {
                autoRefreshInterval = setInterval(() => {
                    if (currentTab === 'sensor-tab') {
                        refreshData();
                    } else {
                        refreshErrorData();
                    }
                }, 5000); // 5 saniyede bir
                statusElement.textContent = 'Otomatik yenileme: Açık (5s)';
            } else {
                clearInterval(autoRefreshInterval);
                statusElement.textContent = 'Otomatik yenileme: Kapalı';
            }
        }
        
        // Sayfa yüklendiğinde son güncelleme zamanını ayarla
        updateLastUpdateTime();
    </script>
</body>
</html>
'''

@app.route('/')
def index():
    """Ana sayfa"""
    try:
        conn = get_db_connection()
        error_conn = get_error_db_connection()
        
        # Son 50 sensör kaydı
        data = conn.execute('''
            SELECT * FROM sensor_data 
            ORDER BY received_at DESC 
            LIMIT 50
        ''').fetchall()
        
        # Sensör istatistikleri
        stats = conn.execute('''
            SELECT 
                COUNT(*) as total_records,
                COUNT(DISTINCT node_id) as active_nodes,
                AVG(temperature) as avg_temp,
                AVG(humidity_air) as avg_humidity
            FROM sensor_data
        ''').fetchone()
        
        # Son 50 hata kaydı
        error_data = error_conn.execute('''
            SELECT * FROM error_logs 
            ORDER BY received_at DESC 
            LIMIT 50
        ''').fetchall()
        
        # Hata istatistikleri
        error_stats = error_conn.execute('''
            SELECT 
                COUNT(*) as total_errors,
                COUNT(DISTINCT error_type) as error_types,
                MAX(timestamp) as last_error_time
            FROM error_logs
        ''').fetchone()
        
        conn.close()
        error_conn.close()
        
        return render_template_string(HTML_TEMPLATE, 
                                    data=data, 
                                    stats=stats,
                                    error_data=error_data,
                                    error_stats=error_stats,
                                    current_time=datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    except Exception as e:
        print(f"Ana sayfa hatası: {e}")
        return f"Hata: {e}", 500

@app.route('/api/data')
def api_data():
    """JSON formatında sensör veri API'si"""
    try:
        conn = get_db_connection()
        
        # Son 50 kayıt
        data = conn.execute('''
            SELECT * FROM sensor_data 
            ORDER BY received_at DESC 
            LIMIT 50
        ''').fetchall()
        
        # İstatistikler
        stats = conn.execute('''
            SELECT 
                COUNT(*) as total_records,
                COUNT(DISTINCT node_id) as active_nodes,
                AVG(temperature) as avg_temp,
                AVG(humidity_air) as avg_humidity
            FROM sensor_data
        ''').fetchone()
        
        conn.close()
        
        # Row objelerini dictionary'ye çevir
        data_list = []
        for row in data:
            data_list.append({
                'id': row['id'],
                'node_id': row['node_id'],
                'light': row['light'],
                'temperature': row['temperature'],
                'humidity_air': row['humidity_air'],
                'humidity_ground': row['humidity_ground'],
                'rx_drift': row['rx_drift'],
                'tx_drift': row['tx_drift'],
                'timestamp': row['timestamp'],
                'received_at': row['received_at']
            })
        
        stats_dict = {
            'total_records': stats['total_records'],
            'active_nodes': stats['active_nodes'],
            'avg_temp': stats['avg_temp'],
            'avg_humidity': stats['avg_humidity']
        }
        
        return jsonify({
            'data': data_list,
            'stats': stats_dict
        })
        
    except Exception as e:
        print(f"API veri hatası: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/errors')
def api_errors():
    """JSON formatında hata logları API'si"""
    try:
        conn = get_error_db_connection()
        
        # Son 50 hata kaydı
        data = conn.execute('''
            SELECT * FROM error_logs 
            ORDER BY received_at DESC 
            LIMIT 50
        ''').fetchall()
        
        # Hata istatistikleri
        stats = conn.execute('''
            SELECT 
                COUNT(*) as total_errors,
                COUNT(DISTINCT error_type) as error_types,
                MAX(timestamp) as last_error_time
            FROM error_logs
        ''').fetchone()
        
        conn.close()
        
        # Row objelerini dictionary'ye çevir
        data_list = []
        for row in data:
            data_list.append({
                'id': row['id'],
                'error_type': row['error_type'],
                'error_message': row['error_message'],
                'timestamp': row['timestamp'],
                'received_at': row['received_at']
            })
        
        stats_dict = {
            'total_errors': stats['total_errors'],
            'error_types': stats['error_types'],
            'last_error_time': stats['last_error_time'][:19] if stats['last_error_time'] else None
        }
        
        return jsonify({
            'data': data_list,
            'stats': stats_dict
        })
        
    except Exception as e:
        print(f"API hata hatası: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/data', methods=['POST'])
def receive_data():
    """Sensör verilerini al"""
    try:
        # JSON verisini al
        json_data = request.get_json()
        
        if not json_data:
            print("Hata: JSON veri bulunamadı")
            return jsonify({'error': 'JSON veri bulunamadı'}), 400
            
        data_str = json_data.get('data', '')
        timestamp = json_data.get('timestamp', datetime.now().isoformat())
        
        if not data_str:
            print("Hata: Veri alanı boş")
            return jsonify({'error': 'Veri alanı boş'}), 400
        
        print(f"Gelen veri: {data_str}")
        
        # Sensör verisini parse et
        try:
            sensor_data = json.loads(data_str)
        except json.JSONDecodeError as e:
            print(f"JSON parse hatası: {e}")
            return jsonify({'error': 'Geçersiz JSON format'}), 400
        
        # Gerekli alanları kontrol et
        required_fields = ['node_id', 'light', 'temperature', 'humidity_air', 'humidity_ground', 'rx_drift', 'tx_drift']
        for field in required_fields:
            if field not in sensor_data:
                print(f"Eksik alan: {field}")
                return jsonify({'error': f'Eksik alan: {field}'}), 400
        
        # Veritabanına kaydet
        conn = get_db_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO sensor_data (node_id, light, temperature, humidity_air, humidity_ground, rx_drift, tx_drift, timestamp)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            sensor_data['node_id'],
            sensor_data['light'],
            sensor_data['temperature'],
            sensor_data['humidity_air'],
            sensor_data['humidity_ground'],
            sensor_data['rx_drift'],
            sensor_data['tx_drift'],
            timestamp
        ))
        
        conn.commit()
        conn.close()
        
        print(f"✅ Veri kaydedildi - Node: {sensor_data['node_id']}, "
              f"Sıcaklık: {sensor_data['temperature']}°C, "
              f"Hava Nemi: {sensor_data['humidity_air']}%, "
              f"Toprak Nemi: {sensor_data['humidity_ground']}%, "
              f"Işık: {sensor_data['light']} lux")
        
        return jsonify({'status': 'success', 'message': 'Veri başarıyla kaydedildi'}), 200
        
    except Exception as e:
        print(f"❌ Veri alma hatası: {e}")
        return jsonify({'error': f'Sunucu hatası: {str(e)}'}), 500

@app.route('/error', methods=['POST'])
def receive_error():
    """Hata mesajlarını al"""
    try:
        # JSON verisini al
        json_data = request.get_json()
        
        if not json_data:
            print("Hata: JSON veri bulunamadı")
            return jsonify({'error': 'JSON veri bulunamadı'}), 400
        
        error_type = json_data.get('error_type', '')
        error_message = json_data.get('error_message', '')
        timestamp = json_data.get('timestamp', datetime.now().isoformat())
        
        if not error_type or not error_message:
            print("Hata: error_type veya error_message boş")
            return jsonify({'error': 'error_type ve error_message gereklidir'}), 400
        
        print(f"Gelen hata: {error_type} - {error_message}")
        
        # Hata veritabanına kaydet
        conn = get_error_db_connection()
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO error_logs (error_type, error_message, timestamp)
            VALUES (?, ?, ?)
        ''', (error_type, error_message, timestamp))
        
        conn.commit()
        conn.close()
        
        print(f"⚠️ Hata kaydedildi - Tip: {error_type}, Mesaj: {error_message}")
        
        return jsonify({'status': 'success', 'message': 'Hata başarıyla kaydedildi'}), 200
        
    except Exception as e:
        print(f"❌ Hata alma hatası: {e}")
        return jsonify({'error': f'Sunucu hatası: {str(e)}'}), 500

@app.route('/api/clear', methods=['POST'])
def clear_data():
    """Tüm sensör verilerini temizle"""
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('DELETE FROM sensor_data')
        deleted_count = cursor.rowcount
        conn.commit()
        conn.close()
        
        print(f"{deleted_count} sensör kaydı silindi")
        return jsonify({'status': 'success', 'deleted_count': deleted_count}), 200
        
    except Exception as e:
        print(f"Veri temizleme hatası: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/clear_errors', methods=['POST'])
def clear_errors():
    """Tüm hata loglarını temizle"""
    try:
        conn = get_error_db_connection()
        cursor = conn.cursor()
        cursor.execute('DELETE FROM error_logs')
        deleted_count = cursor.rowcount
        conn.commit()
        conn.close()
        
        print(f"{deleted_count} hata kaydı silindi")
        return jsonify({'status': 'success', 'deleted_count': deleted_count}), 200
        
    except Exception as e:
        print(f"Hata logları temizleme hatası: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/ping')
def ping():
    """Bağlantı kontrolü için ping endpoint'i"""
    return jsonify({'status': 'ok', 'timestamp': datetime.now().isoformat()}), 200

if __name__ == '__main__':
    # Veritabanlarını başlat
    init_database()
    init_error_database()
    
    print("Flask Sensör Sunucusu Başlatılıyor...")
    print("Arayüz: http://localhost:5000")
    print("HTTP modunda çalışıyor")
    print("Hata logları için /error endpoint'i aktif")
    
    # HTTP için
    app.run(host='0.0.0.0', port=5000, debug=True)