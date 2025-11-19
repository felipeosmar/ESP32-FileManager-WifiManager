// Firmware Upload Script
let uploadInProgress = false;
let selectedFile = null;

// Initialize event listeners
document.addEventListener('DOMContentLoaded', function() {
    const uploadZone = document.getElementById('uploadZone');
    const firmwareInput = document.getElementById('firmwareInput');

    // Click to select file
    uploadZone.addEventListener('click', function() {
        if (!uploadInProgress) {
            firmwareInput.click();
        }
    });

    // File input change
    firmwareInput.addEventListener('change', function(e) {
        if (e.target.files.length > 0) {
            handleFileSelection(e.target.files[0]);
        }
    });

    // Drag and drop events
    uploadZone.addEventListener('dragover', function(e) {
        e.preventDefault();
        if (!uploadInProgress) {
            uploadZone.classList.add('dragging');
        }
    });

    uploadZone.addEventListener('dragleave', function() {
        uploadZone.classList.remove('dragging');
    });

    uploadZone.addEventListener('drop', function(e) {
        e.preventDefault();
        uploadZone.classList.remove('dragging');

        if (!uploadInProgress && e.dataTransfer.files.length > 0) {
            handleFileSelection(e.dataTransfer.files[0]);
        }
    });
});

// Handle file selection
function handleFileSelection(file) {
    console.log('File selected:', file.name);

    // Client-side validation: check file extension
    if (!file.name.toLowerCase().endsWith('.bin')) {
        showError('Arquivo inválido: apenas arquivos .bin são aceitos');
        return;
    }

    // Check file size (should be < 2MB for typical ESP32)
    if (file.size > 2 * 1024 * 1024) {
        showError('Arquivo muito grande: tamanho máximo é 2MB');
        return;
    }

    if (file.size === 0) {
        showError('Arquivo vazio: selecione um firmware válido');
        return;
    }

    selectedFile = file;

    // Show confirmation
    const confirmMsg = `Confirmar atualização do firmware?\n\n` +
                      `Arquivo: ${file.name}\n` +
                      `Tamanho: ${formatSize(file.size)}\n\n` +
                      `O dispositivo será reiniciado após a atualização.`;

    if (confirm(confirmMsg)) {
        uploadFirmware(file);
    }
}

// Upload firmware
async function uploadFirmware(file) {
    uploadInProgress = true;
    let uploadCompleted = false; // Track if upload reached 100%

    // Update UI
    document.getElementById('uploadZone').classList.add('uploading');
    document.getElementById('progressSection').style.display = 'block';
    document.getElementById('errorSection').style.display = 'none';
    document.getElementById('successSection').style.display = 'none';

    // Update status indicator
    const statusDot = document.getElementById('upload-status');
    const statusText = document.getElementById('status-text');
    statusDot.classList.add('uploading');
    statusText.textContent = 'Enviando...';

    // Prevent navigation during upload
    window.addEventListener('beforeunload', preventNavigation);

    const formData = new FormData();
    formData.append('file', file);

    const xhr = new XMLHttpRequest();

    // Progress tracking
    xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            updateProgress(percent);

            // Mark as completed when upload reaches 100%
            if (percent >= 100) {
                uploadCompleted = true;
                console.log('Upload data transmission completed (100%)');
            }
        }
    });

    // Upload complete
    xhr.addEventListener('load', function() {
        console.log('Upload complete - Status:', xhr.status);
        console.log('Response text:', xhr.responseText);

        if (xhr.status === 200) {
            // Parse response
            try {
                const response = JSON.parse(xhr.responseText);
                console.log('Parsed response:', response);

                if (response.error) {
                    handleUploadError(response.error);
                } else {
                    handleUploadSuccess();
                }
            } catch (e) {
                console.error('Failed to parse response:', e);
                console.error('Response was:', xhr.responseText);
                handleUploadError('Erro ao processar resposta do servidor');
            }
        } else {
            let errorMsg = 'Erro no upload';
            try {
                const response = JSON.parse(xhr.responseText);
                if (response.error) {
                    errorMsg = response.error;
                }
            } catch (e) {
                errorMsg = `Erro no upload: ${xhr.status}`;
            }
            console.error('Upload failed with status:', xhr.status, 'Message:', errorMsg);
            handleUploadError(errorMsg);
        }
    });

    // Upload error
    xhr.addEventListener('error', function(e) {
        console.error('XHR error event:', e);
        console.error('XHR state:', xhr.readyState);
        console.error('XHR status:', xhr.status);
        console.log('Upload completed flag:', uploadCompleted);

        // If upload was completed (100%), treat connection error as success
        // This happens because ESP32 reboots immediately after sending response
        if (uploadCompleted) {
            console.log('Upload was completed before connection closed - treating as success');
            handleUploadSuccess();
        } else {
            handleUploadError('Erro de conexão durante o upload. Verifique a conexão com o dispositivo.');
        }
    });

    // Upload timeout
    xhr.addEventListener('timeout', function() {
        console.error('XHR timeout - upload took too long');
        handleUploadError('Timeout: o upload demorou muito tempo');
    });

    // Upload abort
    xhr.addEventListener('abort', function() {
        console.error('XHR aborted');
        handleUploadError('Upload cancelado');
    });

    // Send request
    console.log('Sending firmware to /api/firmware/upload');
    console.log('File size:', file.size, 'bytes');
    xhr.open('POST', '/api/firmware/upload');
    xhr.timeout = 300000; // Increased to 5 minutes timeout for large files
    xhr.send(formData);
}

// Update progress bar
function updateProgress(percent, statusText = null) {
    const progressFill = document.getElementById('progressFill');
    const progressText = document.getElementById('progressText');
    const statusMessage = document.getElementById('statusMessage');

    // Update progress bar and percentage
    progressFill.style.width = percent + '%';
    progressText.textContent = percent + '%';

    // Update status message if provided
    if (statusText) {
        statusMessage.textContent = statusText;
    } else if (percent < 100) {
        statusMessage.textContent = 'Enviando firmware...';
    } else {
        statusMessage.textContent = 'Upload completo';
    }
}

// Handle upload success
function handleUploadSuccess() {
    console.log('Firmware upload successful - ESP32 is flashing and rebooting');

    // Show status messages without simulating fake progress
    const statusMessage = document.getElementById('statusMessage');
    const progressText = document.getElementById('progressText');

    // Hide percentage, show status text only
    progressText.style.display = 'none';

    // Show processing status
    statusMessage.textContent = '⚡ Gravando firmware na flash...';

    setTimeout(() => {
        statusMessage.textContent = '🔄 Dispositivo reiniciando...';

        setTimeout(() => {
            // Hide progress bar, show success message
            document.getElementById('progressSection').style.display = 'none';
            document.getElementById('successSection').style.display = 'block';
            document.getElementById('successMessage').textContent =
                'Firmware gravado com sucesso! O dispositivo está reiniciando...';

            // Update status indicator
            const statusDot = document.getElementById('upload-status');
            const statusText = document.getElementById('status-text');
            statusDot.classList.remove('uploading');
            statusDot.classList.add('connected');
            statusText.textContent = 'Processando';

            // Start reconnect polling
            setTimeout(() => {
                startReconnectPolling();
            }, 2000);
        }, 1500);
    }, 1000);
}

// Handle upload error
function handleUploadError(errorMsg) {
    console.error('Upload error:', errorMsg);

    uploadInProgress = false;

    // Remove navigation blocker
    window.removeEventListener('beforeunload', preventNavigation);

    // Update UI
    document.getElementById('uploadZone').classList.remove('uploading');
    document.getElementById('progressSection').style.display = 'none';

    // Update status indicator
    const statusDot = document.getElementById('upload-status');
    const statusText = document.getElementById('status-text');
    statusDot.classList.remove('uploading');
    statusText.textContent = 'Erro';

    // Show error
    showError(errorMsg);

    // Reset status after 5 seconds
    setTimeout(() => {
        statusDot.classList.remove('uploading');
        statusText.textContent = 'Pronto';
    }, 5000);
}

// Start reconnect polling after reboot
function startReconnectPolling() {
    console.log('Starting reconnect polling...');

    const reconnectInfo = document.getElementById('reconnectInfo');
    reconnectInfo.innerHTML = '⏳ Aguardando reconexão do dispositivo...<br><small>(O ESP32 leva cerca de 5-10 segundos para reiniciar)</small>';

    let attempts = 0;
    const maxAttempts = 30; // Increased from 20 to 30 attempts
    const pollInterval = 2000; // Reduced from 3s to 2s for faster detection

    const pollTimer = setInterval(async () => {
        attempts++;
        console.log(`Reconnect attempt ${attempts}/${maxAttempts}`);

        reconnectInfo.innerHTML =
            `⏳ Tentativa ${attempts}/${maxAttempts}<br>` +
            `<small>Aguardando o dispositivo voltar online...</small>`;

        try {
            // Use a shorter timeout for the fetch request
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 1500);

            const response = await fetch('/api/health/status', {
                method: 'GET',
                cache: 'no-cache',
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            if (response.ok) {
                // Device is back online!
                clearInterval(pollTimer);
                console.log('Device reconnected successfully!');

                reconnectInfo.innerHTML =
                    '✅ <strong>Dispositivo online!</strong><br>' +
                    '<small>Atualização concluída com sucesso. Redirecionando...</small>';

                // Remove navigation blocker
                window.removeEventListener('beforeunload', preventNavigation);

                // Update final status
                const statusDot = document.getElementById('upload-status');
                const statusText = document.getElementById('status-text');
                statusDot.classList.remove('uploading');
                statusDot.classList.add('connected');
                statusText.textContent = 'Completo';

                // Reload page after 3 seconds to allow user to see success message
                setTimeout(() => {
                    console.log('Redirecting to home page...');
                    window.location.href = '/';
                }, 3000);
            }
        } catch (error) {
            // Device not yet available, continue polling
            console.log('Device not available yet:', error.message);
        }

        if (attempts >= maxAttempts) {
            // Timeout
            clearInterval(pollTimer);
            console.error('Reconnect timeout - device did not come back online');

            reconnectInfo.innerHTML =
                '⚠️ <strong>Tempo esgotado aguardando reconexão</strong><br>' +
                '<small>O dispositivo pode ter reiniciado com sucesso, mas não está respondendo.</small><br>' +
                '<button onclick="window.location.reload()" style="margin-top: 10px; padding: 8px 16px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 4px;">Recarregar Página</button>';

            // Remove navigation blocker
            window.removeEventListener('beforeunload', preventNavigation);

            // Update status
            const statusDot = document.getElementById('upload-status');
            const statusText = document.getElementById('status-text');
            statusDot.classList.remove('uploading');
            statusText.textContent = 'Timeout';
        }
    }, pollInterval);
}

// Show error message
function showError(message) {
    const errorSection = document.getElementById('errorSection');
    const errorBanner = document.getElementById('errorBanner');
    const errorMessage = document.getElementById('errorMessage');

    errorMessage.textContent = message;
    errorSection.style.display = 'block';

    // Determine error severity
    if (message.toLowerCase().includes('inválido') ||
        message.toLowerCase().includes('magic byte')) {
        errorBanner.className = 'error-banner critical';
    } else {
        errorBanner.className = 'error-banner warning';
    }

    // Auto-hide after 10 seconds
    setTimeout(() => {
        closeError();
    }, 10000);
}

// Close error banner
function closeError() {
    document.getElementById('errorSection').style.display = 'none';
}

// Prevent navigation during upload
function preventNavigation(e) {
    e.preventDefault();
    e.returnValue = 'Atualização de firmware em andamento. Sair desta página irá interromper a atualização.';
    return e.returnValue;
}

// Format file size
function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}
