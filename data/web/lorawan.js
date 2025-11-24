// LoRaWAN Configuration Manager

let lorawanConfig = {};

// Load configuration on page load
document.addEventListener('DOMContentLoaded', () => {
    loadConfig();
    checkLoRaWANStatus();
});

// Load LoRaWAN configuration from server
async function loadConfig() {
    try {
        const response = await fetch('/api/lorawan/config');
        if (!response.ok) {
            throw new Error('Falha ao carregar configuração');
        }

        lorawanConfig = await response.json();
        populateForm(lorawanConfig);
        toggleActivationFields();
    } catch (error) {
        console.error('Erro ao carregar configuração:', error);
        showError('Erro ao carregar configuração LoRaWAN');
    }
}

// Populate form with configuration data
function populateForm(config) {
    // Basic settings
    document.getElementById('lorawan-enabled').checked = config.enabled || false;
    document.getElementById('activation-mode').value = config.activation_mode || 'OTAA';
    document.getElementById('region').value = config.region || 'US915';
    document.getElementById('device-class').value = config.device_class || 'A';

    // OTAA parameters
    document.getElementById('dev-eui').value = config.dev_eui || '';
    document.getElementById('app-eui').value = config.app_eui || '';
    document.getElementById('app-key').value = config.app_key || '';

    // ABP parameters
    document.getElementById('dev-addr').value = config.dev_addr || '';
    document.getElementById('nwk-s-key').value = config.nwk_s_key || '';
    document.getElementById('app-s-key').value = config.app_s_key || '';

    // Advanced settings
    document.getElementById('adr-enabled').checked = config.adr_enabled !== false;
    document.getElementById('confirmed-uplinks').checked = config.confirmed_uplinks || false;
    document.getElementById('data-rate').value = config.data_rate || 0;
    document.getElementById('tx-power').value = config.tx_power || 14;
    document.getElementById('uplink-interval').value = config.uplink_interval || 300;

    // Pin configuration
    if (config.pins) {
        document.getElementById('pin-miso').value = config.pins.miso || 19;
        document.getElementById('pin-mosi').value = config.pins.mosi || 27;
        document.getElementById('pin-sck').value = config.pins.sck || 5;
        document.getElementById('pin-nss').value = config.pins.nss || 18;
        document.getElementById('pin-rst').value = config.pins.rst || 14;
        document.getElementById('pin-dio0').value = config.pins.dio0 || 26;
        document.getElementById('pin-dio1').value = config.pins.dio1 || 33;
        document.getElementById('pin-dio2').value = config.pins.dio2 || 32;
    }
}

// Toggle activation fields based on selected mode
function toggleActivationFields() {
    const mode = document.getElementById('activation-mode').value;
    const otaaSection = document.getElementById('otaa-section');
    const abpSection = document.getElementById('abp-section');

    if (mode === 'OTAA') {
        otaaSection.style.display = 'block';
        abpSection.style.display = 'none';

        // Make OTAA fields required
        document.getElementById('dev-eui').required = true;
        document.getElementById('app-eui').required = true;
        document.getElementById('app-key').required = true;

        // Make ABP fields optional
        document.getElementById('dev-addr').required = false;
        document.getElementById('nwk-s-key').required = false;
        document.getElementById('app-s-key').required = false;
    } else {
        otaaSection.style.display = 'none';
        abpSection.style.display = 'block';

        // Make OTAA fields optional
        document.getElementById('dev-eui').required = false;
        document.getElementById('app-eui').required = false;
        document.getElementById('app-key').required = false;

        // Make ABP fields required
        document.getElementById('dev-addr').required = true;
        document.getElementById('nwk-s-key').required = true;
        document.getElementById('app-s-key').required = true;
    }
}

// Toggle password visibility functions
function toggleAppKeyVisibility() {
    const input = document.getElementById('app-key');
    input.type = input.type === 'password' ? 'text' : 'password';
}

function toggleNwkKeyVisibility() {
    const input = document.getElementById('nwk-s-key');
    input.type = input.type === 'password' ? 'text' : 'password';
}

function toggleAppSKeyVisibility() {
    const input = document.getElementById('app-s-key');
    input.type = input.type === 'password' ? 'text' : 'password';
}

// Validate hex string
function validateHex(value, length) {
    const hexPattern = new RegExp(`^[0-9A-Fa-f]{${length}}$`);
    return hexPattern.test(value);
}

// Save LoRaWAN configuration
async function saveLoRaWANConfig() {
    try {
        // Collect form data
        const config = {
            enabled: document.getElementById('lorawan-enabled').checked,
            activation_mode: document.getElementById('activation-mode').value,
            region: document.getElementById('region').value,
            device_class: document.getElementById('device-class').value,
            dev_eui: document.getElementById('dev-eui').value,
            app_eui: document.getElementById('app-eui').value,
            app_key: document.getElementById('app-key').value,
            dev_addr: document.getElementById('dev-addr').value,
            nwk_s_key: document.getElementById('nwk-s-key').value,
            app_s_key: document.getElementById('app-s-key').value,
            adr_enabled: document.getElementById('adr-enabled').checked,
            confirmed_uplinks: document.getElementById('confirmed-uplinks').checked,
            data_rate: parseInt(document.getElementById('data-rate').value),
            tx_power: parseInt(document.getElementById('tx-power').value),
            uplink_interval: parseInt(document.getElementById('uplink-interval').value),
            pins: {
                miso: parseInt(document.getElementById('pin-miso').value),
                mosi: parseInt(document.getElementById('pin-mosi').value),
                sck: parseInt(document.getElementById('pin-sck').value),
                nss: parseInt(document.getElementById('pin-nss').value),
                rst: parseInt(document.getElementById('pin-rst').value),
                dio0: parseInt(document.getElementById('pin-dio0').value),
                dio1: parseInt(document.getElementById('pin-dio1').value),
                dio2: parseInt(document.getElementById('pin-dio2').value)
            }
        };

        // Validate hex values
        if (config.activation_mode === 'OTAA') {
            if (!validateHex(config.dev_eui, 16)) {
                showError('DevEUI deve ter 16 caracteres hexadecimais');
                return;
            }
            if (!validateHex(config.app_eui, 16)) {
                showError('AppEUI deve ter 16 caracteres hexadecimais');
                return;
            }
            if (!validateHex(config.app_key, 32)) {
                showError('AppKey deve ter 32 caracteres hexadecimais');
                return;
            }
        } else {
            if (!validateHex(config.dev_addr, 8)) {
                showError('DevAddr deve ter 8 caracteres hexadecimais');
                return;
            }
            if (!validateHex(config.nwk_s_key, 32)) {
                showError('NwkSKey deve ter 32 caracteres hexadecimais');
                return;
            }
            if (!validateHex(config.app_s_key, 32)) {
                showError('AppSKey deve ter 32 caracteres hexadecimais');
                return;
            }
        }

        // Send configuration to server
        const response = await fetch('/api/lorawan/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(config)
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.message || 'Falha ao salvar configuração');
        }

        const result = await response.json();
        showSuccess(result.message || 'Configuração salva com sucesso!');

        // Reload configuration
        setTimeout(() => {
            loadConfig();
        }, 1000);

    } catch (error) {
        console.error('Erro ao salvar configuração:', error);
        showError(error.message || 'Erro ao salvar configuração LoRaWAN');
    }
}

// Check LoRaWAN status
async function checkLoRaWANStatus() {
    try {
        const response = await fetch('/api/lorawan/status');
        if (!response.ok) {
            throw new Error('Falha ao verificar status');
        }

        const status = await response.json();
        updateStatusDisplay(status);
    } catch (error) {
        console.error('Erro ao verificar status:', error);
        updateStatusDisplay({
            enabled: false,
            joined: false,
            message: 'Erro ao verificar status'
        });
    }
}

// Update status display
function updateStatusDisplay(status) {
    const statusCard = document.getElementById('lorawan-status-card');
    const statusIcon = document.getElementById('lorawan-status-icon');
    const statusText = document.getElementById('lorawan-status');
    const statusDetail = document.getElementById('lorawan-status-detail');

    if (!status.enabled) {
        statusCard.className = 'status-card degraded';
        statusIcon.textContent = '📡';
        statusText.textContent = 'Desabilitado';
        statusDetail.textContent = 'LoRaWAN está desabilitado';
    } else if (status.joined) {
        statusCard.className = 'status-card healthy';
        statusIcon.textContent = '✅';
        statusText.textContent = 'Conectado';
        statusDetail.textContent = status.message || 'Join bem-sucedido com a rede';
    } else if (status.joining) {
        statusCard.className = 'status-card';
        statusIcon.textContent = '🔄';
        statusText.textContent = 'Conectando...';
        statusDetail.textContent = 'Tentando join com a rede';
    } else {
        statusCard.className = 'status-card degraded';
        statusIcon.textContent = '⚠️';
        statusText.textContent = 'Desconectado';
        statusDetail.textContent = status.message || 'Aguardando conexão';
    }

    // Display additional info if available
    if (status.uplink_count !== undefined) {
        statusDetail.textContent += ` | Uplinks: ${status.uplink_count}`;
    }
    if (status.downlink_count !== undefined) {
        statusDetail.textContent += ` | Downlinks: ${status.downlink_count}`;
    }
}

// Test join procedure
async function testJoin() {
    try {
        showSuccess('Iniciando procedimento de join...');

        const response = await fetch('/api/lorawan/join', {
            method: 'POST'
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.message || 'Falha ao iniciar join');
        }

        const result = await response.json();
        showSuccess(result.message || 'Join iniciado. Aguarde a confirmação...');

        // Check status after 5 seconds
        setTimeout(() => {
            checkLoRaWANStatus();
        }, 5000);

    } catch (error) {
        console.error('Erro ao testar join:', error);
        showError(error.message || 'Erro ao iniciar join');
    }
}

// Send test uplink
async function sendTestUplink() {
    try {
        const testPayload = {
            type: 'test',
            timestamp: Date.now(),
            random: Math.floor(Math.random() * 1000)
        };

        const response = await fetch('/api/lorawan/uplink', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(testPayload)
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.message || 'Falha ao enviar uplink');
        }

        const result = await response.json();
        showSuccess(result.message || 'Uplink de teste enviado com sucesso!');

        // Update status
        setTimeout(() => {
            checkLoRaWANStatus();
        }, 2000);

    } catch (error) {
        console.error('Erro ao enviar uplink:', error);
        showError(error.message || 'Erro ao enviar uplink de teste');
    }
}

// Show error message
function showError(message) {
    const errorSection = document.getElementById('error-section');
    const errorBanner = document.getElementById('error-banner');
    const errorMessage = document.getElementById('error-message');
    const successSection = document.getElementById('success-section');

    errorMessage.textContent = message;
    errorBanner.className = 'error-banner critical';
    errorSection.style.display = 'block';
    successSection.style.display = 'none';

    // Auto-hide after 5 seconds
    setTimeout(() => {
        closeError();
    }, 5000);
}

// Show success message
function showSuccess(message) {
    const successSection = document.getElementById('success-section');
    const successMessage = document.getElementById('success-message');
    const errorSection = document.getElementById('error-section');

    successMessage.textContent = message;
    successSection.style.display = 'block';
    errorSection.style.display = 'none';

    // Auto-hide after 5 seconds
    setTimeout(() => {
        closeSuccess();
    }, 5000);
}

// Close error message
function closeError() {
    const errorSection = document.getElementById('error-section');
    errorSection.style.display = 'none';
}

// Close success message
function closeSuccess() {
    const successSection = document.getElementById('success-section');
    successSection.style.display = 'none';
}

// Auto-refresh status every 10 seconds if enabled
setInterval(() => {
    if (document.getElementById('lorawan-enabled').checked) {
        checkLoRaWANStatus();
    }
}, 10000);
