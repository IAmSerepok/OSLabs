from flask import Flask, render_template, jsonify, request, redirect, url_for
import requests
import json
from datetime import datetime, timedelta
import plotly.graph_objects as go
import plotly.utils
import threading
import time

app = Flask(__name__)

# Конфигурация сервера
SERVER_URL = "http://localhost:8080"
UPDATE_INTERVAL = 5

class TemperatureClient:
    def __init__(self, server_url):
        self.server_url = server_url
        self.current_temp = 0.0
        self.history = []
        self.max_history = 100
        self.lock = threading.Lock()
        
    def fetch_current_temperature(self):
        try:
            response = requests.get(f"{self.server_url}/api/current", timeout=2)
            if response.status_code == 200:
                data = response.json()
                return data.get('temperature', 0.0)
        except Exception as e:
            print(f"Error fetching temperature: {e}")
        return None
    
    def fetch_statistics(self, start_time, end_time):
        try:
            params = {
                'start': start_time,
                'end': end_time
            }
            response = requests.get(f"{self.server_url}/api/statistics", params=params, timeout=5)
            if response.status_code == 200:
                return response.json()
        except Exception as e:
            print(f"Error fetching statistics: {e}")
        return None
    
    def fetch_raw_data(self, start_time, end_time, limit=100):
        try:
            params = {
                'start': start_time,
                'end': end_time,
                'limit': limit
            }
            response = requests.get(f"{self.server_url}/api/raw", params=params, timeout=5)
            if response.status_code == 200:
                return response.json()
        except Exception as e:
            print(f"Error fetching raw data: {e}")
        return None
    
    def fetch_hourly_data(self, start_date, end_date):
        try:
            params = {
                'start': start_date,
                'end': end_date
            }
            response = requests.get(f"{self.server_url}/api/hourly", params=params, timeout=5)
            if response.status_code == 200:
                return response.json()
        except Exception as e:
            print(f"Error fetching hourly data: {e}")
        return None
    
    def fetch_daily_data(self, start_date, end_date):
        try:
            params = {
                'start': start_date,
                'end': end_date
            }
            response = requests.get(f"{self.server_url}/api/daily", params=params, timeout=5)
            if response.status_code == 200:
                return response.json()
        except Exception as e:
            print(f"Error fetching daily data: {e}")
        return None
    
    def update_history(self):
        temp = self.fetch_current_temperature()
        if temp is not None:
            with self.lock:
                timestamp = datetime.now().strftime("%H:%M:%S")
                self.history.append({
                    'timestamp': timestamp,
                    'temperature': temp
                })
                if len(self.history) > self.max_history:
                    self.history.pop(0)
                self.current_temp = temp

client = TemperatureClient(SERVER_URL)

def background_updater():
    while True:
        client.update_history()
        time.sleep(UPDATE_INTERVAL)

# Запускаем фоновый поток
updater_thread = threading.Thread(target=background_updater, daemon=True)
updater_thread.start()

@app.route('/')
def index():
    return redirect(url_for('dashboard'))

@app.route('/dashboard')
def dashboard():
    with client.lock:
        current_temp = client.current_temp
        history = client.history.copy()
    
    # Получаем последние 24 часа данных для графика
    end_time = datetime.now()
    start_time = end_time - timedelta(hours=24)
    
    # Получаем сырые данные для таблицы
    start_str = start_time.strftime("%Y-%m-%d %H:%M:%S")
    end_str = end_time.strftime("%Y-%m-%d %H:%M:%S")
    raw_data = client.fetch_raw_data(start_str, end_str, limit=20)
    
    # Получаем статистику
    stats = client.fetch_statistics(start_str, end_str)
    
    # Создаем график истории
    if history:
        times = [h['timestamp'] for h in history]
        temps = [h['temperature'] for h in history]
        
        fig_history = go.Figure()
        fig_history.add_trace(go.Scatter(
            x=times, y=temps,
            mode='lines+markers',
            name='Temperature',
            line=dict(color='#667eea', width=2),
            marker=dict(size=6),
            fill='tozeroy',
            fillcolor='rgba(102, 126, 234, 0.1)'
        ))
        
        fig_history.update_layout(
            title='Temperature History (Last 100 readings)',
            xaxis_title='Time',
            yaxis_title='Temperature (°C)',
            template='plotly_white',
            height=400,
            showlegend=False
        )
        
        history_graph = json.dumps(fig_history, cls=plotly.utils.PlotlyJSONEncoder)
    else:
        history_graph = None
    
    return render_template('dashboard.html',
                         current_temp=current_temp,
                         stats=stats,
                         history_graph=history_graph,
                         raw_data=raw_data,
                         history=history[-10:],
                         start_time=start_str,
                         end_time=end_str)

@app.route('/statistics')
def statistics():
    # Получаем параметры из запроса
    start_param = request.args.get('start')
    end_param = request.args.get('end')
    
    if not start_param or not end_param:
        end_time = datetime.now()
        start_time = end_time - timedelta(hours=24)
        start_param = start_time.strftime("%Y-%m-%d %H:%M:%S")
        end_param = end_time.strftime("%Y-%m-%d %H:%M:%S")
    
    # Получаем статистику
    stats = client.fetch_statistics(start_param, end_param)
    
    raw_data = client.fetch_raw_data(start_param, end_param, limit=20)
    
    # Получаем часовые данные для графика
    start_date = start_param[:10]
    end_date = end_param[:10]
    hourly_data = client.fetch_hourly_data(start_date, end_date)
    
    # Создаем график часовых средних
    if hourly_data:
        hours = []
        temps = []
        for item in hourly_data:
            if isinstance(item, dict):
                timestamp = item.get('timestamp', '')
                temp = item.get('temperature', 0)
                # Форматируем время для отображения
                if ' ' in timestamp:
                    time_part = timestamp.split(' ')[1][:5]
                    hours.append(time_part)
                else:
                    hours.append(timestamp[:5])
                temps.append(temp)
        
        fig_hourly = go.Figure()
        fig_hourly.add_trace(go.Bar(
            x=hours, y=temps,
            name='Hourly Average',
            marker_color='#764ba2'
        ))
        
        fig_hourly.update_layout(
            title=f'Hourly Temperature Averages ({start_date} to {end_date})',
            xaxis_title='Time',
            yaxis_title='Temperature (°C)',
            template='plotly_white',
            height=400,
            showlegend=False
        )
        
        hourly_graph = json.dumps(fig_hourly, cls=plotly.utils.PlotlyJSONEncoder)
    else:
        hourly_graph = None
    
    return render_template('statistics.html',
                         stats=stats,
                         raw_data=raw_data,
                         hourly_graph=hourly_graph,
                         start_time=start_param,
                         end_time=end_param)

@app.route('/api/current')
def api_current():
    with client.lock:
        return jsonify({
            'temperature': client.current_temp,
            'timestamp': datetime.now().isoformat()
        })

@app.route('/api/history')
def api_history():
    with client.lock:
        return jsonify(client.history)

@app.route('/api/statistics')
def api_statistics():
    try:
        start = request.args.get('start')
        end = request.args.get('end')
        
        if not start or not end:
            return jsonify({'error': 'Missing start or end parameter'}), 400
        
        stats = client.fetch_statistics(start, end)
        if stats:
            return jsonify(stats)
        else:
            return jsonify({'error': 'Failed to fetch statistics'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/raw')
def api_raw():
    try:
        start = request.args.get('start')
        end = request.args.get('end')
        limit = request.args.get('limit', '100')
        
        if not start or not end:
            return jsonify({'error': 'Missing start or end parameter'}), 400
        
        try:
            limit_int = int(limit)
        except:
            limit_int = 100
        
        raw_data = client.fetch_raw_data(start, end, limit_int)
        if raw_data:
            return jsonify(raw_data)
        else:
            return jsonify({'error': 'Failed to fetch raw data'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/hourly')
def api_hourly():
    try:
        start = request.args.get('start')
        end = request.args.get('end')
        
        if not start or not end:
            return jsonify({'error': 'Missing start or end parameter'}), 400
        
        hourly_data = client.fetch_hourly_data(start, end)
        if hourly_data:
            return jsonify(hourly_data)
        else:
            return jsonify({'error': 'Failed to fetch hourly data'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)