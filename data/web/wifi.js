// WiFi Manager Script
let scanInProgress = false;
let selectedNetwork = null;
let refreshCountdown = 0;
let countdownTimer = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    loadCurrentConnection();
});

// Load current WiFi connection info
async function loadCurrentConnection() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();

        if (data.wifi) {
            document.getElementById('current-ssid').textContent = data.wifi.ssid || 'Não conectado';
            document.getElementById('current-ip').textContent = data.wifi.ip || '--';
            document.getElementById('current-rssi').textContent = data.wifi.rssi ? `${data.wifi.rssi} dBm` : '--';

            const status = data.wifi.connected ? 'Conectado' : 'Desconectado';
            const statusElement = document.getElementById('current-status');
            statusElement.textContent = status;
            statusElement.style.color = data.wifi.connected ? 'var(--success)' : 'var(--danger)';
        }
    } catch (error) {
        console.error('Error loading current connection:', error);
        document.getElementById('current-ssid').textContent = 'Erro ao carregar';
    }
}

// Scan for WiFi networks
async function scanNetworks() {
    if (scanInProgress) {
        console.log('Scan already in progress');
        return;
    }

    scanInProgress = true;
    document.getElementById('loadingSection').style.display = 'block';
    document.getElementById('networksList').innerHTML = '<p style="text-align: center; padding: 40px; color: #6c757d;">Escaneando...</p>';

    // Disable scan button
    const scanBtn = document.querySelector('.toolbar .btn-primary');
    scanBtn.disabled = true;

    try {
        const response = await fetch('/api/wifi/scan');
        const data = await response.json();

        if (data.error) {
            showError(data.error);
            document.getElementById('networksList').innerHTML = `<p style="text-align: center; padding: 40px; color: var(--danger);">${data.error}</p>`;
        } else if (data.networks && data.networks.length > 0) {
            displayNetworks(data.networks);

            // Start countdown for next scan (30 seconds)
            startRefreshCountdown(30);
        } else {
            document.getElementById('networksList').innerHTML = '<p style="text-align: center; padding: 40px; color: #6c757d;">Nenhuma rede encontrada</p>';
        }
    } catch (error) {
        console.error('Error scanning networks:', error);
        showError('Erro ao escanear redes WiFi');
        document.getElementById('networksList').innerHTML = '<p style="text-align: center; padding: 40px; color: var(--danger);">Erro ao escanear redes</p>';
    } finally {
        scanInProgress = false;
        document.getElementById('loadingSection').style.display = 'none';
        scanBtn.disabled = false;
    }
}

// Display networks list
function displayNetworks(networks) {
    const networksList = document.getElementById('networksList');

    // Sort networks by signal strength
    networks.sort((a, b) => b.rssi - a.rssi);

    let html = '<div class="networks-grid">';

    networks.forEach((network, index) => {
        const signalStrength = getSignalStrength(network.rssi);
        const securityIcon = network.encryption !== 0 ? '🔒' : '🔓';

        html += `
            <div class="network-card" onclick="selectNetwork('${escapeHtml(network.ssid)}', ${network.encryption})">
                <div class="network-header">
                    <div class="network-name">
                        <span class="network-icon">${securityIcon}</span>
                        <strong>${escapeHtml(network.ssid)}</strong>
                    </div>
                    <div class="network-signal ${signalStrength.class}">
                        <span class="signal-icon">${signalStrength.icon}</span>
                        <span class="signal-text">${network.rssi} dBm</span>
                    </div>
                </div>
                <div class="network-details">
                    <span class="network-detail">Canal: ${network.channel}</span>
                    <span class="network-detail">${getEncryptionType(network.encryption)}</span>
                </div>
            </div>
        `;
    });

    html += '</div>';
    networksList.innerHTML = html;
}

// Get signal strength classification
function getSignalStrength(rssi) {
    if (rssi >= -50) {
        return { class: 'excellent', icon: '📶', text: 'Excelente' };
    } else if (rssi >= -60) {
        return { class: 'good', icon: '📶', text: 'Bom' };
    } else if (rssi >= -70) {
        return { class: 'fair', icon: '📶', text: 'Razoável' };
    } else {
        return { class: 'weak', icon: '📶', text: 'Fraco' };
    }
}

// Get encryption type name
function getEncryptionType(type) {
    const types = {
        0: 'Aberta',
        2: 'WPA/PSK',
        3: 'WPA2/PSK',
        4: 'WPA/WPA2/PSK',
        5: 'WPA2/Enterprise',
        6: 'WPA3/PSK',
        7: 'WPA2/WPA3/PSK'
    };
    return types[type] || 'Desconhecida';
}

// Select network to connect
function selectNetwork(ssid, encryption) {
    selectedNetwork = { ssid, encryption };

    document.getElementById('selectedSSID').value = ssid;
    document.getElementById('wifiPassword').value = '';

    // Show modal
    const modal = document.getElementById('connectionModal');
    modal.style.display = 'flex';

    // Focus on password field if network is secured
    if (encryption !== 0) {
        setTimeout(() => {
            document.getElementById('wifiPassword').focus();
        }, 100);
    }
}

// Close connection modal
function closeConnectionModal() {
    const modal = document.getElementById('connectionModal');
    modal.style.display = 'none';
    selectedNetwork = null;
}

// Toggle password visibility
function togglePasswordVisibility() {
    const passwordInput = document.getElementById('wifiPassword');
    const toggleBtn = document.querySelector('.btn-toggle-password');

    if (passwordInput.type === 'password') {
        passwordInput.type = 'text';
        toggleBtn.textContent = '🙈';
    } else {
        passwordInput.type = 'password';
        toggleBtn.textContent = '👁️';
    }
}

// Connect to selected network
async function connectToNetwork() {
    if (!selectedNetwork) {
        showError('Nenhuma rede selecionada');
        return;
    }

    const password = document.getElementById('wifiPassword').value;

    // Validate password for secured networks
    if (selectedNetwork.encryption !== 0 && password.length < 8) {
        showError('A senha deve ter pelo menos 8 caracteres');
        return;
    }

    // Confirm action
    const confirmMsg = `Conectar à rede "${selectedNetwork.ssid}"?\n\nO ESP32 será reiniciado e tentará conectar à nova rede.`;
    if (!confirm(confirmMsg)) {
        return;
    }

    try {
        const response = await fetch('/api/wifi/connect', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                ssid: selectedNetwork.ssid,
                password: password
            })
        });

        const data = await response.json();

        if (data.error) {
            showError(data.error);
        } else {
            // Show success message
            closeConnectionModal();
            showSuccess(`Configuração salva! O dispositivo está reiniciando...\n\nAguarde cerca de 10 segundos e recarregue a página.`);

            // Reload page after 12 seconds
            setTimeout(() => {
                window.location.reload();
            }, 12000);
        }
    } catch (error) {
        console.error('Error connecting to network:', error);
        showError('Erro ao salvar configuração WiFi');
    }
}

// Start refresh countdown
function startRefreshCountdown(seconds) {
    refreshCountdown = seconds;
    const refreshBtn = document.getElementById('refreshBtn');
    const countdownSpan = document.getElementById('countdown');

    refreshBtn.disabled = true;

    if (countdownTimer) {
        clearInterval(countdownTimer);
    }

    countdownTimer = setInterval(() => {
        refreshCountdown--;
        countdownSpan.textContent = `Aguarde ${refreshCountdown}s`;

        if (refreshCountdown <= 0) {
            clearInterval(countdownTimer);
            refreshBtn.disabled = false;
            countdownSpan.textContent = '🔄 Atualizar';
        }
    }, 1000);
}

// Show error message
function showError(message) {
    alert('❌ Erro: ' + message);
}

// Show success message
function showSuccess(message) {
    alert('✅ ' + message);
}

// Escape HTML to prevent XSS
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Close modal when clicking outside
window.onclick = function(event) {
    const modal = document.getElementById('connectionModal');
    if (event.target === modal) {
        closeConnectionModal();
    }
}
