/**
 * ESP32 File Manager - Web Interface JavaScript
 */

// State
let isConnected = false;
let healthData = null;

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    console.log('ESP32 File Manager Interface Loaded');

    // Update connection status periodically
    checkConnection();
    setInterval(checkConnection, 2000);

    // Update system info
    updateSystemInfo();
    setInterval(updateSystemInfo, 5000);
});

async function checkConnection() {
    try {
        const response = await fetch('/api/health/status', {
            method: 'GET',
            cache: 'no-cache'
        });
        if (response.ok) {
            healthData = await response.json();
            if (!isConnected) {
                isConnected = true;
                updateConnectionStatus(true);
            }
        } else {
            if (isConnected) {
                isConnected = false;
                updateConnectionStatus(false);
            }
        }
    } catch (error) {
        if (isConnected) {
            isConnected = false;
            updateConnectionStatus(false);
        }
    }
}

function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connection-status');
    const statusText = document.getElementById('status-text');

    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Conectado';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Desconectado';
    }
}

async function updateSystemInfo() {
    if (!healthData) return;

    // Update uptime
    const uptimeElement = document.getElementById('uptime');
    if (uptimeElement && healthData.uptime) {
        uptimeElement.textContent = healthData.uptime.formatted || 'N/A';
    }

    // Update WiFi status
    const wifiElement = document.getElementById('wifi-status');
    if (wifiElement && healthData.wifi) {
        const wifiStatus = healthData.wifi.connected ?
            `${healthData.wifi.ssid} (${healthData.wifi.signal_strength})` :
            'Desconectado';
        wifiElement.textContent = wifiStatus;
    }
}

console.log('ESP32 File Manager Ready');
