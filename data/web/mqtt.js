// MQTT Configuration Script

let mqttConfig = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    loadMQTTConfig();
    checkMQTTStatus();

    // Auto-refresh status every 5 seconds
    setInterval(checkMQTTStatus, 5000);

    // Form submission
    document.getElementById('mqttConfigForm').addEventListener('submit', saveMQTTConfig);
});

// Load MQTT configuration
async function loadMQTTConfig() {
    try {
        const response = await fetch('/api/mqtt/config');

        if (!response.ok) {
            throw new Error('Falha ao carregar configuração');
        }

        const data = await response.json();
        mqttConfig = data;

        // Populate form
        document.getElementById('mqttEnabled').checked = data.enabled || false;
        document.getElementById('mqttServer').value = data.server || '';
        document.getElementById('mqttPort').value = data.port || 1883;
        document.getElementById('mqttUsername').value = data.username || '';
        document.getElementById('mqttPassword').value = data.password || '';
        document.getElementById('mqttHostname').value = data.hostname || 'ESP32-Device';
        document.getElementById('mqttMainTopic').value = data.main_topic || 'esp32/data';
        document.getElementById('mqttPublishInterval').value = data.publish_interval || 60;
        document.getElementById('mqttClientId').value = data.client_id || '';

        console.log('MQTT configuration loaded:', data);
    } catch (error) {
        console.error('Error loading MQTT config:', error);
        showError('Erro ao carregar configuração: ' + error.message);
    }
}

// Check MQTT connection status
async function checkMQTTStatus() {
    try {
        const response = await fetch('/api/mqtt/status');

        if (!response.ok) {
            throw new Error('Falha ao verificar status');
        }

        const data = await response.json();

        const statusCard = document.getElementById('mqttStatusCard');
        const statusIcon = document.getElementById('mqttStatusIcon');
        const statusValue = document.getElementById('mqttStatus');
        const statusDetail = document.getElementById('mqttStatusDetail');

        if (!data.enabled) {
            statusCard.className = 'status-card';
            statusIcon.textContent = '⏸️';
            statusValue.textContent = 'Desabilitado';
            statusDetail.textContent = 'O MQTT está desativado nas configurações';
        } else if (data.connected) {
            statusCard.className = 'status-card healthy';
            statusIcon.textContent = '✅';
            statusValue.textContent = 'Conectado';
            statusDetail.textContent = `Servidor: ${data.server}:${data.port} | Tópico: ${data.main_topic}`;
        } else {
            statusCard.className = 'status-card degraded';
            statusIcon.textContent = '❌';
            statusValue.textContent = 'Desconectado';
            statusDetail.textContent = data.error || 'Não conectado ao broker MQTT';
        }

        console.log('MQTT status:', data);
    } catch (error) {
        console.error('Error checking MQTT status:', error);
    }
}

// Save MQTT configuration
async function saveMQTTConfig(event) {
    event.preventDefault();

    const formData = {
        enabled: document.getElementById('mqttEnabled').checked,
        server: document.getElementById('mqttServer').value.trim(),
        port: parseInt(document.getElementById('mqttPort').value),
        username: document.getElementById('mqttUsername').value.trim(),
        password: document.getElementById('mqttPassword').value,
        hostname: document.getElementById('mqttHostname').value.trim(),
        main_topic: document.getElementById('mqttMainTopic').value.trim(),
        publish_interval: parseInt(document.getElementById('mqttPublishInterval').value)
    };

    // Validation
    if (formData.enabled && !formData.server) {
        showError('O servidor MQTT é obrigatório quando o MQTT está habilitado');
        return;
    }

    if (formData.port < 1 || formData.port > 65535) {
        showError('A porta deve estar entre 1 e 65535');
        return;
    }

    if (formData.enabled && !formData.main_topic) {
        showError('O tópico principal é obrigatório');
        return;
    }

    if (formData.publish_interval < 10 || formData.publish_interval > 3600) {
        showError('O intervalo de publicação deve estar entre 10 e 3600 segundos');
        return;
    }

    try {
        const response = await fetch('/api/mqtt/config', {
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
            loadMQTTConfig();
            checkMQTTStatus();
        }, 1000);

        console.log('MQTT configuration saved:', result);
    } catch (error) {
        console.error('Error saving MQTT config:', error);
        showError('Erro ao salvar configuração: ' + error.message);
    }
}

// Test MQTT connection
async function testConnection() {
    const server = document.getElementById('mqttServer').value.trim();
    const port = parseInt(document.getElementById('mqttPort').value);

    if (!server) {
        showError('Configure o servidor MQTT antes de testar a conexão');
        return;
    }

    try {
        showSuccess('Testando conexão... Aguarde...');

        const response = await fetch('/api/mqtt/test', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                server: server,
                port: port,
                username: document.getElementById('mqttUsername').value.trim(),
                password: document.getElementById('mqttPassword').value
            })
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || 'Falha no teste de conexão');
        }

        const result = await response.json();

        if (result.success) {
            showSuccess('✅ Conexão bem-sucedida! O servidor MQTT está acessível.');
        } else {
            showError('❌ Falha na conexão: ' + (result.error || 'Erro desconhecido'));
        }

        console.log('Connection test result:', result);
    } catch (error) {
        console.error('Error testing connection:', error);
        showError('Erro ao testar conexão: ' + error.message);
    }
}

// Publish test message
async function publishTestMessage() {
    const topic = document.getElementById('testTopic').value.trim();
    const message = document.getElementById('testMessage').value;
    const retained = document.getElementById('testRetained').checked;

    if (!message) {
        showError('Digite uma mensagem para publicar');
        return;
    }

    try {
        const response = await fetch('/api/mqtt/publish', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                topic: topic || null,
                message: message,
                retained: retained
            })
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || 'Falha ao publicar mensagem');
        }

        const result = await response.json();

        showSuccess(`Mensagem publicada com sucesso no tópico: ${result.topic}`);

        console.log('Message published:', result);
    } catch (error) {
        console.error('Error publishing message:', error);
        showError('Erro ao publicar mensagem: ' + error.message);
    }
}

// Toggle password visibility
function togglePasswordVisibility() {
    const passwordInput = document.getElementById('mqttPassword');
    const button = event.target;

    if (passwordInput.type === 'password') {
        passwordInput.type = 'text';
        button.textContent = '🙈';
    } else {
        passwordInput.type = 'password';
        button.textContent = '👁️';
    }
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
