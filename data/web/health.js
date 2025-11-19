// ESP32 Health Monitor JavaScript

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
        const response = await fetch('/api/health/status');

        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        updateDisplay(data);
        updateLastUpdateTime();

    } catch (error) {
        console.error('Error fetching health data:', error);
        showError('Erro ao carregar dados de saúde: ' + error.message);
    }
}

// Update all display elements
function updateDisplay(data) {
    // Overall status
    updateOverallStatus(data.status);

    // Uptime
    updateUptime(data.uptime);

    // Memory
    updateMemory(data.memory);

    // WiFi
    updateWiFi(data.wifi);

    // SD Card
    updateSDCard(data.sd_card);

    // CPU & Hardware
    updateHardware(data.cpu, data.flash);
}

// Update overall status
function updateOverallStatus(status) {
    const statusDot = document.getElementById('overall-status');
    const statusText = document.getElementById('overall-status-text');
    const statusCard = document.getElementById('status-card');

    // Remove previous status classes
    statusDot.classList.remove('healthy', 'degraded');
    statusCard.classList.remove('healthy', 'degraded');

    if (status === 'healthy') {
        statusDot.classList.add('healthy');
        statusCard.classList.add('healthy');
        statusText.textContent = 'Sistema Saudável';
    } else {
        statusDot.classList.add('degraded');
        statusCard.classList.add('degraded');
        statusText.textContent = 'Sistema Degradado';
    }
}

// Update uptime display
function updateUptime(uptime) {
    const uptimeElement = document.getElementById('uptime');
    if (uptime && uptime.formatted) {
        uptimeElement.textContent = `Uptime: ${uptime.formatted}`;
    }
}

// Update memory displays
function updateMemory(memory) {
    if (memory.heap) {
        const heap = memory.heap;
        const usagePercent = heap.usage_percent.toFixed(1);

        document.getElementById('heap-usage').textContent = `${usagePercent}%`;
        document.getElementById('heap-used').textContent = formatBytes(heap.used);
        document.getElementById('heap-free').textContent = formatBytes(heap.free);
        document.getElementById('heap-total').textContent = formatBytes(heap.total);

        updateProgressBar('heap-progress', heap.usage_percent);
    }

    if (memory.psram) {
        const psram = memory.psram;
        const usagePercent = psram.usage_percent ? psram.usage_percent.toFixed(1) : 0;

        document.getElementById('psram-usage').textContent = `${usagePercent}%`;
        document.getElementById('psram-used').textContent = formatBytes(psram.used);
        document.getElementById('psram-free').textContent = formatBytes(psram.free);
        document.getElementById('psram-total').textContent = formatBytes(psram.total);

        if (psram.total > 0) {
            updateProgressBar('psram-progress', psram.usage_percent);
        }
    }
}

// Update WiFi information
function updateWiFi(wifi) {
    if (!wifi) return;

    document.getElementById('wifi-ssid').textContent = wifi.ssid || '--';
    document.getElementById('wifi-signal').textContent = wifi.signal_strength || '--';
    document.getElementById('wifi-ip').textContent = wifi.ip || '--';
    document.getElementById('wifi-mac').textContent = wifi.mac || '--';
    document.getElementById('wifi-channel').textContent = wifi.channel || '--';
    document.getElementById('wifi-rssi').textContent = wifi.rssi ? `${wifi.rssi} dBm` : '--';
}

// Update SD Card information
function updateSDCard(sdCard) {
    if (!sdCard || !sdCard.ready) {
        document.getElementById('sd-usage').textContent = 'N/A';
        document.getElementById('sd-used').textContent = '--';
        document.getElementById('sd-free').textContent = '--';
        document.getElementById('sd-total').textContent = '--';
        document.getElementById('sd-type').textContent = '--';
        return;
    }

    const usagePercent = sdCard.usage_percent.toFixed(1);
    document.getElementById('sd-usage').textContent = `${usagePercent}%`;
    document.getElementById('sd-used').textContent = `${sdCard.used_mb} MB`;
    document.getElementById('sd-free').textContent = `${sdCard.free_mb} MB`;
    document.getElementById('sd-total').textContent = `${sdCard.total_mb} MB`;
    document.getElementById('sd-type').textContent = sdCard.type || '--';

    updateProgressBar('sd-progress', sdCard.usage_percent);
}

// Update hardware information
function updateHardware(cpu, flash) {
    if (cpu) {
        document.getElementById('cpu-model').textContent = cpu.chip_model || '--';
        document.getElementById('cpu-revision').textContent = cpu.chip_revision || '--';
        document.getElementById('cpu-freq').textContent = cpu.frequency_mhz ? `${cpu.frequency_mhz} MHz` : '--';
        document.getElementById('cpu-cores').textContent = cpu.cores || '--';
        document.getElementById('sdk-version').textContent = cpu.sdk_version || '--';
    }

    if (flash) {
        document.getElementById('flash-size').textContent = flash.size_mb ? `${flash.size_mb} MB` : '--';
    }
}

// Update application status (removed - no tracking functionality)

// Update progress bar
function updateProgressBar(elementId, percentage) {
    const progressBar = document.getElementById(elementId);
    if (!progressBar) return;

    // Remove previous color classes
    progressBar.classList.remove('warning', 'danger');

    // Set width
    progressBar.style.width = `${Math.min(percentage, 100)}%`;

    // Add color class based on usage
    if (percentage > 80) {
        progressBar.classList.add('danger');
    } else if (percentage > 60) {
        progressBar.classList.add('warning');
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
    document.getElementById('last-update').textContent = timeString;
}

// Show error message
function showError(message) {
    const statusText = document.getElementById('overall-status-text');
    const statusDot = document.getElementById('overall-status');

    statusText.textContent = message;
    statusDot.classList.remove('healthy', 'degraded');
    statusDot.classList.add('degraded');
}

// Manual refresh button
window.refreshHealth = refreshHealth;
