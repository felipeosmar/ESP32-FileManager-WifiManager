// Standard Footer Component
// Loads a reusable footer for all pages

function loadFooter() {
    const footerHTML = `
        <footer>
            <p>ESP32 File Manager v1.0</p>
        </footer>
    `;

    const footerPlaceholder = document.getElementById('footer-placeholder');
    if (footerPlaceholder) {
        footerPlaceholder.innerHTML = footerHTML;
    }
}

// Load footer when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', loadFooter);
} else {
    loadFooter();
}
