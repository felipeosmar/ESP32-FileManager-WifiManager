// ESP32 Status Monitor JavaScript

let autoRefreshEnabled = true;
let refreshInterval = null;

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    setupAutoRefresh();
    refreshHealth();
});

// Setup auto refresh toggle
function setupAutoRefresh() {
    const autoRefreshToggle = document.getElementById('auto-refresh');

    autoRefreshToggle.addEventListener('change', (e) => {
        autoRefreshEnabled = e.target.checked;

        if (autoRefreshEnabled) {
            startAutoRefresh();
        } else {
            stopAutoRefresh();
        }
    });

    // Start auto refresh by default
    startAutoRefresh();
}

function startAutoRefresh() {
    // Clear any existing interval
    if (refreshInterval) {
        clearInterval(refreshInterval);
    }

    // Refresh every 5 seconds
    refreshInterval = setInterval(refreshHealth, 5000);
}

function stopAutoRefresh() {
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
}

// Fetch and display health data
async function refreshHealth() {
    try {
        const response = await fetch('/api/status');

        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        updateDisplay(data);
        updateLastUpdateTime();

    } catch (error) {
        console.error('Error fetching status data:', error);
        showError('Erro ao carregar dados de status: ' + error.message);
    }
}

// Update all display elements
function updateDisplay(data) {
    // Overall status
    updateOverallStatus(data.status);

    // Uptime
    updateUptime(data.uptime);

    // System Info
    updateSystemInfo(data.system, data.cpu);

    // Memory
    updateMemory(data.memory);

    // WiFi
    updateWiFi(data.wifi);

    // Storage (SPIFFS & Flash)
    updateStorage(data.spiffs, data.flash);
}

// Update overall status
function updateOverallStatus(status) {
    const statusDot = document.getElementById('overall-status');
    const statusText = document.getElementById('overall-status-text');
    const statusCard = document.getElementById('status-card');

    // Remove previous status classes
    if (statusDot) statusDot.classList.remove('healthy', 'degraded');
    if (statusCard) statusCard.classList.remove('healthy', 'degraded');

    if (status === 'healthy') {
        if (statusDot) statusDot.classList.add('healthy');
        if (statusCard) statusCard.classList.add('healthy');
        if (statusText) statusText.textContent = 'Sistema Saudável';
    } else {
        if (statusDot) statusDot.classList.add('degraded');
        if (statusCard) statusCard.classList.add('degraded');
        if (statusText) statusText.textContent = 'Sistema Degradado';
    }
}

// Update uptime display
function updateUptime(uptime) {
    const uptimeElement = document.getElementById('uptime');
    if (uptime && uptime.formatted) {
        uptimeElement.textContent = `Uptime: ${uptime.formatted}`;
    }
}

// Update system info
function updateSystemInfo(system, cpu) {
    if (system) {
        setText('system-reset', system.reset_reason);
        setText('system-compile', `${system.compile_date} ${system.compile_time}`);
    }

    if (cpu) {
        setText('cpu-model', cpu.chip_model);
        setText('cpu-revision', cpu.chip_revision);
        setText('cpu-freq', cpu.frequency_mhz ? `${cpu.frequency_mhz} MHz` : '--');
        setText('sdk-version', cpu.sdk_version);
    }
}

// Update memory displays
function updateMemory(memory) {
    if (memory.heap) {
        const heap = memory.heap;
        const usagePercent = heap.usage_percent.toFixed(1);

        setText('heap-usage', `${usagePercent}%`);
        setText('heap-used', formatBytes(heap.used));
        setText('heap-free', formatBytes(heap.free));
        setText('heap-total', formatBytes(heap.total));
    }

    if (memory.psram) {
        const psram = memory.psram;
        const usagePercent = psram.usage_percent ? psram.usage_percent.toFixed(1) : 0;

        setText('psram-usage', `${usagePercent}%`);
        setText('psram-used', formatBytes(psram.used));
        setText('psram-free', formatBytes(psram.free));
        setText('psram-total', formatBytes(psram.total));
    }

    if (memory.sketch) {
        const sketch = memory.sketch;
        const usagePercent = sketch.usage_percent.toFixed(1);

        setText('sketch-usage', `${usagePercent}%`);
        setText('sketch-used', formatBytes(sketch.used));
        setText('sketch-free', formatBytes(sketch.free));
        setText('sketch-total', formatBytes(sketch.total));
    }
}

// Update WiFi information
function updateWiFi(wifi) {
    if (!wifi) return;

    setText('wifi-ssid', wifi.ssid);
    setText('wifi-signal', wifi.signal_strength);
    setText('wifi-ip', wifi.ip);
    setText('wifi-gateway', wifi.gateway);
    setText('wifi-subnet', wifi.subnet);
    setText('wifi-dns', wifi.dns);
    setText('wifi-mac', wifi.mac);
    setText('wifi-bssid', wifi.bssid);
    setText('wifi-channel', wifi.channel);
    setText('wifi-rssi', wifi.rssi ? `${wifi.rssi} dBm` : '--');
}

// Update Storage information
function updateStorage(spiffs, flash) {
    if (spiffs && spiffs.ready) {
        const usagePercent = spiffs.usage_percent.toFixed(1);
        setText('spiffs-usage', `${usagePercent}%`);
        setText('spiffs-used', formatBytes(spiffs.used_bytes));
        setText('spiffs-free', formatBytes(spiffs.free_bytes));
        setText('spiffs-total', formatBytes(spiffs.total_bytes));
    }

    if (flash) {
        setText('flash-size', flash.size_mb ? `${flash.size_mb} MB` : '--');
        setText('flash-speed', flash.speed_mhz ? `${flash.speed_mhz} MHz` : '--');
    }
}

// Helper to set text content safely
function setText(id, text) {
    const el = document.getElementById(id);
    if (el) {
        el.textContent = text || '--';
    }
}

// Format bytes to human readable
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    if (!bytes) return '--';

    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));

    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Update last update time
function updateLastUpdateTime() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('pt-BR');
    setText('last-update', timeString);
}

// Show error message
function showError(message) {
    const statusText = document.getElementById('overall-status-text');
    const statusDot = document.getElementById('overall-status');

    if (statusText) statusText.textContent = message;
    if (statusDot) {
        statusDot.classList.remove('healthy', 'degraded');
        statusDot.classList.add('degraded');
    }
}

// Manual refresh button
window.refreshHealth = refreshHealth;
