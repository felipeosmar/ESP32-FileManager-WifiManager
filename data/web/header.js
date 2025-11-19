// Header inclusion and initialization
function loadHeader(pageTitle) {
    fetch('/header.html')
        .then(response => response.text())
        .then(html => {
            const headerPlaceholder = document.getElementById('header-placeholder');
            if (headerPlaceholder) {
                headerPlaceholder.innerHTML = html;
                // Set page title
                const titleElement = document.getElementById('page-title');
                if (titleElement && pageTitle) {
                    titleElement.textContent = pageTitle;
                }
                // Initialize connection status
                initConnectionStatus();
            }
        })
        .catch(error => console.error('Error loading header:', error));
}

// Initialize connection status indicator
function initConnectionStatus() {
    const statusDot = document.getElementById('connection-status');
    const statusText = document.getElementById('status-text');

    if (statusDot && statusText) {
        // Check connection
        fetch('/api/health/status')
            .then(response => response.json())
            .then(data => {
                statusDot.classList.add('connected');
                statusText.textContent = 'Conectado';
            })
            .catch(() => {
                statusDot.classList.remove('connected');
                statusText.textContent = 'Desconectado';
            });

        // Update every 5 seconds
        setInterval(() => {
            fetch('/api/health/status')
                .then(response => response.json())
                .then(data => {
                    statusDot.classList.add('connected');
                    statusText.textContent = 'Conectado';
                })
                .catch(() => {
                    statusDot.classList.remove('connected');
                    statusText.textContent = 'Desconectado';
                });
        }, 5000);
    }
}
