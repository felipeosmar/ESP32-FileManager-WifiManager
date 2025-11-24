/**
 * NTP Configuration Page
 */

// State
let timeUpdateInterval = null;

// Initialize
document.addEventListener('DOMContentLoaded', function () {
    loadConfig();
    refreshTime();

    // Start periodic time updates (every 5 seconds)
    timeUpdateInterval = setInterval(refreshTime, 5000);

    // Form submission
    const form = document.getElementById('ntp-config-form');
    if (form) {
        form.addEventListener('submit', handleSubmit);
    }
});

// Load NTP configuration from server
async function loadConfig() {
    try {
        const response = await fetch('/api/ntp/config');
        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const config = await response.json();

        // Populate form fields
        document.getElementById('ntp-enabled').checked = config.enabled;
        document.getElementById('ntp-server').value = config.server || 'pool.ntp.org';
        document.getElementById('ntp-offset').value = config.offset || -10800;
        document.getElementById('ntp-interval').value = config.interval || 60000;

        console.log('NTP config loaded:', config);
    } catch (error) {
        console.error('Failed to load NTP config:', error);
        showError('Failed to load configuration');
    }
}

// Refresh current time display
async function refreshTime() {
    try {
        const response = await fetch('/api/ntp/time');
        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const data = await response.json();

        const timeElement = document.getElementById('current-time');
        const statusElement = document.getElementById('sync-status');
        const statusCard = document.getElementById('time-status');

        if (timeElement) {
            timeElement.textContent = data.time || '--:--:--';
        }

        if (statusElement && statusCard) {
            if (!data.enabled) {
                statusElement.textContent = 'NTP Disabled';
                statusCard.classList.remove('healthy');
                statusCard.classList.add('degraded');
            } else if (data.synced) {
                statusElement.textContent = '✓ Synchronized';
                statusCard.classList.remove('degraded');
                statusCard.classList.add('healthy');
            } else {
                statusElement.textContent = '⚠ Not Synchronized';
                statusCard.classList.remove('healthy');
                statusCard.classList.add('degraded');
            }
        }
    } catch (error) {
        console.error('Failed to refresh time:', error);
        const statusElement = document.getElementById('sync-status');
        if (statusElement) {
            statusElement.textContent = 'Connection Error';
        }
    }
}

// Handle form submission
async function handleSubmit(event) {
    event.preventDefault();

    const formData = new FormData(event.target);
    const config = {
        enabled: document.getElementById('ntp-enabled').checked,
        server: formData.get('server'),
        offset: parseInt(formData.get('offset')),
        interval: parseInt(formData.get('interval'))
    };

    // Validate
    if (!config.server || config.server.trim() === '') {
        showError('NTP server is required');
        return;
    }

    if (config.interval < 10000 || config.interval > 3600000) {
        showError('Update interval must be between 10000ms and 3600000ms');
        return;
    }

    try {
        const response = await fetch('/api/ntp/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(config)
        });

        if (!response.ok) {
            const errorData = await response.json().catch(() => ({}));
            throw new Error(errorData.error || `HTTP ${response.status}`);
        }

        const result = await response.json();
        showSuccess(result.message || 'Configuration saved successfully');

        // Reload config to reflect changes
        setTimeout(() => {
            loadConfig();
            refreshTime();
        }, 1000);

    } catch (error) {
        console.error('Failed to save NTP config:', error);
        showError('Failed to save configuration: ' + error.message);
    }
}

// Show success message
function showSuccess(message) {
    // Create a temporary success banner
    const banner = document.createElement('div');
    banner.className = 'success-card';
    banner.innerHTML = `
        <span class="success-icon">✓</span>
        <div class="success-content">
            <h3>Success</h3>
            <p>${message}</p>
        </div>
    `;
    banner.style.position = 'fixed';
    banner.style.top = '20px';
    banner.style.right = '20px';
    banner.style.zIndex = '1000';
    banner.style.minWidth = '300px';

    document.body.appendChild(banner);

    setTimeout(() => {
        banner.remove();
    }, 3000);
}

// Show error message
function showError(message) {
    // Create a temporary error banner
    const banner = document.createElement('div');
    banner.className = 'error-banner critical';
    banner.innerHTML = `
        <span class="error-icon">⚠</span>
        <div class="error-message">${message}</div>
        <button class="error-close" onclick="this.parentElement.remove()">×</button>
    `;
    banner.style.position = 'fixed';
    banner.style.top = '20px';
    banner.style.right = '20px';
    banner.style.zIndex = '1000';
    banner.style.minWidth = '300px';

    document.body.appendChild(banner);

    setTimeout(() => {
        banner.remove();
    }, 5000);
}

// Cleanup on page unload
window.addEventListener('beforeunload', function () {
    if (timeUpdateInterval) {
        clearInterval(timeUpdateInterval);
    }
});
