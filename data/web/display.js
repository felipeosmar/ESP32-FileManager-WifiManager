// Display OLED Configuration Script

let displayConfig = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    loadDisplayConfig();
    checkDisplayStatus();

    // Auto-refresh status every 5 seconds
    setInterval(checkDisplayStatus, 5000);

    // Form submission
    document.getElementById('displayConfigForm').addEventListener('submit', saveDisplayConfig);

    // Display mode change handler
    document.getElementById('displayMode').addEventListener('change', function() {
        const customSection = document.getElementById('customTextSection');
        if (this.value === '5') { // Custom text mode
            customSection.style.display = 'block';
        } else {
            customSection.style.display = 'none';
        }
    });
});

// Load display configuration
async function loadDisplayConfig() {
    try {
        const response = await fetch('/api/display/config');

        if (!response.ok) {
            throw new Error('Falha ao carregar configuração');
        }

        const data = await response.json();
        displayConfig = data;

        // Populate form
        document.getElementById('displayEnabled').checked = data.enabled || false;
        document.getElementById('displayAddress').value = data.address || 60;
        document.getElementById('sdaPin').value = data.sda_pin || 21;
        document.getElementById('sclPin').value = data.scl_pin || 22;
        document.getElementById('rstPin').value = data.rst_pin !== undefined ? data.rst_pin : -1;
        document.getElementById('autoUpdate').checked = data.auto_update !== false;
        document.getElementById('brightness').value = data.brightness || 128;
        document.getElementById('flipDisplay').checked = data.flip_display || false;

        updateBrightnessValue(data.brightness || 128);

        console.log('Display configuration loaded:', data);
    } catch (error) {
        console.error('Error loading display config:', error);
        showError('Erro ao carregar configuração: ' + error.message);
    }
}

// Check display status
async function checkDisplayStatus() {
    try {
        const response = await fetch('/api/display/status');

        if (!response.ok) {
            throw new Error('Falha ao verificar status');
        }

        const data = await response.json();

        const statusCard = document.getElementById('displayStatusCard');
        const statusIcon = document.getElementById('displayStatusIcon');
        const statusValue = document.getElementById('displayStatus');
        const statusDetail = document.getElementById('displayStatusDetail');

        if (!data.enabled) {
            statusCard.className = 'status-card';
            statusIcon.textContent = '⏸️';
            statusValue.textContent = 'Desabilitado';
            statusDetail.textContent = 'O display está desativado nas configurações';
        } else if (data.available) {
            statusCard.className = 'status-card healthy';
            statusIcon.textContent = '✅';
            statusValue.textContent = 'Conectado';
            const modeNames = ['Desligado', 'Logo', 'Sistema', 'Rede', 'MQTT', 'Texto'];
            statusDetail.textContent = `Endereço: 0x${data.address.toString(16).toUpperCase()} | Modo: ${modeNames[data.mode] || 'Desconhecido'}`;
        } else {
            statusCard.className = 'status-card degraded';
            statusIcon.textContent = '❌';
            statusValue.textContent = 'Não Detectado';
            statusDetail.textContent = data.error || 'Display não encontrado no endereço I2C configurado';
        }

        console.log('Display status:', data);
    } catch (error) {
        console.error('Error checking display status:', error);
    }
}

// Save display configuration
async function saveDisplayConfig(event) {
    event.preventDefault();

    const formData = {
        enabled: document.getElementById('displayEnabled').checked,
        address: parseInt(document.getElementById('displayAddress').value),
        sda_pin: parseInt(document.getElementById('sdaPin').value),
        scl_pin: parseInt(document.getElementById('sclPin').value),
        rst_pin: parseInt(document.getElementById('rstPin').value),
        auto_update: document.getElementById('autoUpdate').checked,
        brightness: parseInt(document.getElementById('brightness').value),
        flip_display: document.getElementById('flipDisplay').checked
    };

    // Validation
    if (formData.sda_pin < 0 || formData.sda_pin > 39) {
        showError('Pino SDA deve estar entre 0 e 39');
        return;
    }

    if (formData.scl_pin < 0 || formData.scl_pin > 39) {
        showError('Pino SCL deve estar entre 0 e 39');
        return;
    }

    if (formData.rst_pin < -1 || formData.rst_pin > 39) {
        showError('Pino RST deve estar entre -1 e 39');
        return;
    }

    if (formData.sda_pin === formData.scl_pin) {
        showError('Pinos SDA e SCL devem ser diferentes');
        return;
    }

    if (formData.rst_pin >= 0 && (formData.rst_pin === formData.sda_pin || formData.rst_pin === formData.scl_pin)) {
        showError('Pino RST não pode ser igual ao SDA ou SCL');
        return;
    }

    try {
        const response = await fetch('/api/display/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(formData)
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || 'Falha ao salvar configuração');
        }

        const result = await response.json();

        showSuccess('Configuração salva com sucesso! Reinicie o ESP32 para aplicar as mudanças.');

        // Reload config and status
        setTimeout(() => {
            loadDisplayConfig();
            checkDisplayStatus();
        }, 1000);

        console.log('Display configuration saved:', result);
    } catch (error) {
        console.error('Error saving display config:', error);
        showError('Erro ao salvar configuração: ' + error.message);
    }
}

// Apply display mode
async function applyDisplayMode() {
    const mode = parseInt(document.getElementById('displayMode').value);

    let payload = { mode: mode };

    // If custom text mode, include the text lines
    if (mode === 5) {
        payload.line1 = document.getElementById('customLine1').value || '';
        payload.line2 = document.getElementById('customLine2').value || '';
        payload.line3 = document.getElementById('customLine3').value || '';
        payload.line4 = document.getElementById('customLine4').value || '';
    }

    try {
        const response = await fetch('/api/display/mode', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || 'Falha ao aplicar modo');
        }

        const result = await response.json();

        const modeNames = ['desligado', 'logo', 'sistema', 'rede', 'MQTT', 'texto personalizado'];
        showSuccess(`Modo "${modeNames[mode]}" aplicado com sucesso!`);

        // Update status
        setTimeout(checkDisplayStatus, 500);

        console.log('Display mode applied:', result);
    } catch (error) {
        console.error('Error applying display mode:', error);
        showError('Erro ao aplicar modo: ' + error.message);
    }
}

// Update brightness value display
function updateBrightnessValue(value) {
    document.getElementById('brightnessValue').textContent = value;
}

// Show error message
function showError(message) {
    const errorSection = document.getElementById('errorSection');
    const errorBanner = document.getElementById('errorBanner');
    const errorMessage = document.getElementById('errorMessage');

    document.getElementById('successSection').style.display = 'none';

    errorMessage.textContent = message;
    errorBanner.className = 'error-banner critical';
    errorSection.style.display = 'block';

    // Auto-hide after 8 seconds
    setTimeout(() => {
        closeError();
    }, 8000);
}

// Show success message
function showSuccess(message) {
    const successSection = document.getElementById('successSection');
    const successMessage = document.getElementById('successMessage');

    document.getElementById('errorSection').style.display = 'none';

    successMessage.textContent = message;
    successSection.style.display = 'block';

    // Auto-hide after 5 seconds
    setTimeout(() => {
        successSection.style.display = 'none';
    }, 5000);
}

// Close error banner
function closeError() {
    document.getElementById('errorSection').style.display = 'none';
}
