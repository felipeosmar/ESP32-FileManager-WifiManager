// SHT20 Sensor Configuration Script

let sensorConfig = null;
let sensorData = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    loadSensorConfig();
    checkSensorStatus();

    // Auto-refresh status every 5 seconds
    setInterval(checkSensorStatus, 5000);

    // Form submission
    document.getElementById('sensorConfigForm').addEventListener('submit', saveSensorConfig);
});

// Load sensor configuration
async function loadSensorConfig() {
    try {
        const response = await fetch('/api/sensor/config');

        if (!response.ok) {
            throw new Error('Falha ao carregar configuração');
        }

        const data = await response.json();
        sensorConfig = data;

        // Populate form
        document.getElementById('sensorEnabled').checked = data.enabled !== undefined ? data.enabled : true;
        document.getElementById('readInterval').value = data.read_interval !== undefined ? data.read_interval : 180;
        document.getElementById('fahrenheit').checked = data.fahrenheit || false;

        console.log('Sensor configuration loaded:', data);
    } catch (error) {
        console.error('Error loading sensor config:', error);
        showError('Erro ao carregar configuração: ' + error.message);
    }
}

// Check sensor status
async function checkSensorStatus() {
    try {
        const response = await fetch('/api/sensor/status');

        if (!response.ok) {
            throw new Error('Falha ao verificar status');
        }

        const data = await response.json();
        sensorData = data;

        const statusCard = document.getElementById('sensorStatusCard');
        const statusIcon = document.getElementById('sensorStatusIcon');
        const statusValue = document.getElementById('sensorStatus');
        const statusDetail = document.getElementById('sensorStatusDetail');

        if (!data.enabled) {
            statusCard.className = 'status-card';
            statusIcon.textContent = '⏸️';
            statusValue.textContent = 'Desabilitado';
            statusDetail.textContent = 'O sensor está desativado nas configurações';

            // Clear readings
            document.getElementById('temperatureValue').textContent = '--';
            document.getElementById('humidityValue').textContent = '--';
            document.getElementById('lastUpdate').textContent = 'Nunca';
        } else if (data.available && data.valid) {
            statusCard.className = 'status-card healthy';
            statusIcon.textContent = '✅';
            statusValue.textContent = 'Conectado';
            statusDetail.textContent = 'Sensor SHT20 funcionando normalmente';

            // Update readings
            updateReadings(data);
        } else if (data.available && !data.valid) {
            statusCard.className = 'status-card degraded';
            statusIcon.textContent = '⚠️';
            statusValue.textContent = 'Sem Dados';
            statusDetail.textContent = 'Sensor detectado mas sem leituras válidas';

            // Clear readings
            document.getElementById('temperatureValue').textContent = '--';
            document.getElementById('humidityValue').textContent = '--';
        } else {
            statusCard.className = 'status-card degraded';
            statusIcon.textContent = '❌';
            statusValue.textContent = 'Não Detectado';
            statusDetail.textContent = data.error || 'Sensor não encontrado no endereço I2C 0x40';

            // Clear readings
            document.getElementById('temperatureValue').textContent = '--';
            document.getElementById('humidityValue').textContent = '--';
            document.getElementById('lastUpdate').textContent = 'Nunca';
        }

        console.log('Sensor status:', data);
    } catch (error) {
        console.error('Error checking sensor status:', error);
    }
}

// Update sensor readings display
function updateReadings(data) {
    const tempValue = document.getElementById('temperatureValue');
    const tempUnit = document.getElementById('temperatureUnit');
    const humidityValue = document.getElementById('humidityValue');
    const lastUpdate = document.getElementById('lastUpdate');

    if (data.fahrenheit) {
        tempValue.textContent = data.temperature_f.toFixed(1);
        tempUnit.textContent = '°F';
    } else {
        tempValue.textContent = data.temperature.toFixed(1);
        tempUnit.textContent = '°C';
    }

    humidityValue.textContent = data.humidity.toFixed(1);

    // Format timestamp
    const now = Date.now();
    const timestamp = data.timestamp;
    const elapsed = Math.floor((now - timestamp) / 1000);

    if (elapsed < 60) {
        lastUpdate.textContent = `${elapsed} segundos atrás`;
    } else if (elapsed < 3600) {
        lastUpdate.textContent = `${Math.floor(elapsed / 60)} minutos atrás`;
    } else {
        lastUpdate.textContent = `${Math.floor(elapsed / 3600)} horas atrás`;
    }
}

// Save sensor configuration
async function saveSensorConfig(event) {
    event.preventDefault();

    const formData = {
        enabled: document.getElementById('sensorEnabled').checked,
        read_interval: parseInt(document.getElementById('readInterval').value),
        fahrenheit: document.getElementById('fahrenheit').checked
    };

    // Validation
    if (formData.read_interval < 5 || formData.read_interval > 3600) {
        showError('Intervalo de leitura deve estar entre 5 e 3600 segundos');
        return;
    }

    try {
        const response = await fetch('/api/sensor/config', {
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

        showSuccess('Configuração salva com sucesso!');

        // Reload config and status
        setTimeout(() => {
            loadSensorConfig();
            checkSensorStatus();
        }, 1000);

        console.log('Sensor configuration saved:', result);
    } catch (error) {
        console.error('Error saving sensor config:', error);
        showError('Erro ao salvar configuração: ' + error.message);
    }
}

// Force immediate sensor reading
async function forceRead() {
    if (!sensorData || !sensorData.enabled) {
        showError('Sensor está desabilitado');
        return;
    }

    if (!sensorData.available) {
        showError('Sensor não está disponível');
        return;
    }

    showSuccess('Atualizando leituras...');

    // Trigger immediate status check
    setTimeout(() => {
        checkSensorStatus();
    }, 500);
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
