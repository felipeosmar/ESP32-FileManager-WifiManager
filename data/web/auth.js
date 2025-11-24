// Authentication page JavaScript

let authData = null;

// Load authentication status on page load
document.addEventListener('DOMContentLoaded', function() {
    loadAuthStatus();

    // Add password strength checker
    const newPasswordInput = document.getElementById('new-password');
    if (newPasswordInput) {
        newPasswordInput.addEventListener('input', checkPasswordStrength);
    }
});

async function loadAuthStatus() {
    try {
        const response = await fetch('/api/auth/status');
        if (!response.ok) {
            throw new Error('Falha ao carregar status de autenticação');
        }

        authData = await response.json();

        // Update UI
        document.getElementById('current-username').textContent = authData.username;
        document.getElementById('username').value = authData.username;

        if (authData.first_login) {
            document.getElementById('auth-status').textContent = '⚠️ Primeiro acesso';
            document.getElementById('auth-status').style.color = '#ff9800';
            document.getElementById('first-login-alert').style.display = 'block';
        } else {
            document.getElementById('auth-status').textContent = '✅ Configurado';
            document.getElementById('auth-status').style.color = '#4caf50';
            document.getElementById('first-login-alert').style.display = 'none';
        }
    } catch (error) {
        console.error('Erro ao carregar status:', error);
        showMessage('Erro ao carregar informações de autenticação', 'error');
    }
}

function checkPasswordStrength() {
    const password = document.getElementById('new-password').value;
    const strengthBar = document.getElementById('strength-bar');
    const strengthText = document.getElementById('strength-text');

    if (password.length === 0) {
        strengthBar.style.width = '0%';
        strengthBar.className = 'strength-level';
        strengthText.textContent = 'Digite uma senha';
        return;
    }

    let strength = 0;
    let feedback = [];

    // Length check
    if (password.length >= 8) {
        strength += 25;
    } else {
        feedback.push('mínimo 8 caracteres');
    }

    // Uppercase check
    if (/[A-Z]/.test(password)) {
        strength += 25;
    } else {
        feedback.push('letra maiúscula');
    }

    // Lowercase check
    if (/[a-z]/.test(password)) {
        strength += 25;
    } else {
        feedback.push('letra minúscula');
    }

    // Number check
    if (/[0-9]/.test(password)) {
        strength += 25;
    } else {
        feedback.push('número');
    }

    // Update strength bar
    strengthBar.style.width = strength + '%';

    if (strength < 50) {
        strengthBar.className = 'strength-level weak';
        strengthText.textContent = 'Fraca - Falta: ' + feedback.join(', ');
    } else if (strength < 75) {
        strengthBar.className = 'strength-level medium';
        strengthText.textContent = 'Média - Falta: ' + feedback.join(', ');
    } else if (strength < 100) {
        strengthBar.className = 'strength-level good';
        strengthText.textContent = 'Boa - Falta: ' + feedback.join(', ');
    } else {
        strengthBar.className = 'strength-level strong';
        strengthText.textContent = 'Forte ✓';
    }
}

async function changePassword(event) {
    event.preventDefault();

    const currentPassword = document.getElementById('current-password').value;
    const newPassword = document.getElementById('new-password').value;
    const confirmPassword = document.getElementById('confirm-password').value;
    const username = document.getElementById('username').value;

    // Validate passwords match
    if (newPassword !== confirmPassword) {
        showMessage('As senhas não coincidem', 'error');
        return;
    }

    // Validate password strength
    if (newPassword.length < 8) {
        showMessage('A senha deve ter no mínimo 8 caracteres', 'error');
        return;
    }

    if (!/[A-Z]/.test(newPassword)) {
        showMessage('A senha deve conter pelo menos 1 letra maiúscula', 'error');
        return;
    }

    if (!/[a-z]/.test(newPassword)) {
        showMessage('A senha deve conter pelo menos 1 letra minúscula', 'error');
        return;
    }

    if (!/[0-9]/.test(newPassword)) {
        showMessage('A senha deve conter pelo menos 1 número', 'error');
        return;
    }

    // Prepare data
    const data = {
        current_password: currentPassword,
        new_password: newPassword,
        username: username
    };

    try {
        const response = await fetch('/api/auth/change-password', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });

        const result = await response.json();

        if (response.ok) {
            showMessage('Senha alterada com sucesso! Você precisará fazer login novamente com a nova senha.', 'success');
            setTimeout(() => {
                // Clear form
                resetForm();
                // Reload status
                loadAuthStatus();
            }, 2000);
        } else {
            showMessage(result.error || 'Erro ao alterar senha', 'error');
        }
    } catch (error) {
        console.error('Erro:', error);
        showMessage('Erro de comunicação com o servidor', 'error');
    }
}

function resetForm() {
    document.getElementById('change-password-form').reset();
    document.getElementById('strength-bar').style.width = '0%';
    document.getElementById('strength-bar').className = 'strength-level';
    document.getElementById('strength-text').textContent = 'Digite uma senha';
    if (authData) {
        document.getElementById('username').value = authData.username;
    }
}

function showMessage(message, type) {
    // Create a simple alert-style message
    const alertDiv = document.createElement('div');
    alertDiv.className = 'alert alert-' + type;
    alertDiv.textContent = message;
    alertDiv.style.position = 'fixed';
    alertDiv.style.top = '20px';
    alertDiv.style.right = '20px';
    alertDiv.style.zIndex = '9999';
    alertDiv.style.maxWidth = '400px';
    alertDiv.style.animation = 'slideIn 0.3s ease-out';

    document.body.appendChild(alertDiv);

    // Remove after 5 seconds
    setTimeout(() => {
        alertDiv.style.animation = 'slideOut 0.3s ease-out';
        setTimeout(() => {
            document.body.removeChild(alertDiv);
        }, 300);
    }, 5000);
}
