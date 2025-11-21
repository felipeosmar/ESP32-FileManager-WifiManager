let currentPath = '/';
let currentFiles = []; // Store current directory files for duplicate check

async function refreshFiles() {
    try {
        const response = await fetch('/api/files/list?dir=' + encodeURIComponent(currentPath));
        const data = await response.json();

        // Store files list for duplicate checking
        currentFiles = data.files || [];

        const fileList = document.getElementById('fileList');
        fileList.innerHTML = '';

        if (currentPath !== '/') {
            const upItem = createFileItem('..', 0, true, true);
            fileList.appendChild(upItem);
        }

        currentFiles.forEach(file => {
            const item = createFileItem(file.name, file.size, file.isDir);
            fileList.appendChild(item);
        });

        document.getElementById('currentPath').textContent = currentPath;
    } catch (error) {
        alert('Erro ao carregar arquivos: ' + error.message);
    }
}

function createFileItem(name, size, isDir, isUp = false) {
    const item = document.createElement('div');
    item.className = 'file-item';

    const icon = isDir ? '📁' : '📄';
    const displayName = isUp ? 'Voltar...' : name;
    const sizeStr = isDir ? '' : formatSize(size);

    item.innerHTML = `
        <div class="file-icon">${icon}</div>
        <div class="file-info">
            <div class="file-name">${displayName}</div>
            <div class="file-size">${sizeStr}</div>
        </div>
        <div class="file-actions">
            ${!isUp && !isDir ? `<button class="action-btn btn-success" onclick="editFile('${name}', event)">✏️ Editar</button>` : ''}
            ${!isUp && !isDir ? `<button class="action-btn btn-primary" onclick="downloadFile('${name}', event)">⬇️ Download</button>` : ''}
            ${!isUp && !isDir ? `<button class="action-btn btn-primary" onclick="viewFile('${name}', event)">👁️ Ver</button>` : ''}
            ${!isUp ? `<button class="action-btn btn-danger" onclick="deleteFile('${name}', ${isDir}, event)">🗑️ Deletar</button>` : ''}
        </div>
    `;

    if (isDir || isUp) {
        item.onclick = () => navigateTo(isUp ? '..' : name);
    }

    return item;
}

function navigateTo(name) {
    if (name === '..') {
        // Navigate to parent directory
        const parts = currentPath.split('/').filter(p => p);
        parts.pop();
        currentPath = parts.length > 0 ? '/' + parts.join('/') : '/';
    } else {
        // Navigate into subdirectory
        if (currentPath === '/') {
            currentPath = '/' + name;
        } else {
            currentPath = currentPath + '/' + name;
        }
    }

    // Normalize path: ensure it doesn't end with / (except root)
    if (currentPath !== '/' && currentPath.endsWith('/')) {
        currentPath = currentPath.slice(0, -1);
    }

    console.log('Current path:', currentPath);
    refreshFiles();
}

function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

async function downloadFile(name, event) {
    event.stopPropagation();
    const filepath = (currentPath + '/' + name).replace('//', '/');
    window.open('/api/files/download?file=' + encodeURIComponent(filepath), '_blank');
}

async function viewFile(name, event) {
    event.stopPropagation();
    const filepath = (currentPath + '/' + name).replace('//', '/');
    window.open('/api/files/view?file=' + encodeURIComponent(filepath), '_blank');
}

async function deleteFile(name, isDir, event) {
    event.stopPropagation();
    const type = isDir ? 'pasta' : 'arquivo';
    if (!confirm(`Deseja realmente deletar este ${type}?\n${name}`)) return;

    const filepath = (currentPath + '/' + name).replace('//', '/');
    const formData = new FormData();
    formData.append('file', filepath);

    try {
        const response = await fetch('/api/files/delete', {
            method: 'POST',
            body: formData
        });

        if (response.ok) {
            alert('Deletado com sucesso!');
            refreshFiles();
        } else {
            alert('Erro ao deletar');
        }
    } catch (error) {
        alert('Erro: ' + error.message);
    }
}

async function createFolder() {
    const name = prompt('Nome da nova pasta:');
    if (!name) return;

    // Build the full directory path
    let dirpath;
    if (currentPath === '/') {
        dirpath = '/' + name;
    } else {
        dirpath = currentPath + '/' + name;
    }

    console.log('Creating folder at path:', dirpath);

    const formData = new FormData();
    formData.append('dir', dirpath);

    try {
        const response = await fetch('/api/files/mkdir', {
            method: 'POST',
            body: formData
        });

        if (response.ok) {
            alert('Pasta criada com sucesso!');
            refreshFiles();
        } else {
            const error = await response.text();
            console.error('Error creating folder:', error);
            alert('Erro ao criar pasta: ' + error);
        }
    } catch (error) {
        console.error('Exception creating folder:', error);
        alert('Erro: ' + error.message);
    }
}

async function handleFiles(files) {
    for (const file of files) {
        await uploadFile(file);
    }
}

async function uploadFile(file) {
    console.log('Uploading file to path:', currentPath);
    console.log('File name:', file.name);

    // Check if file already exists
    const fileExists = currentFiles.some(f => f.name === file.name && !f.isDir);

    if (fileExists) {
        const confirm = window.confirm(
            `O arquivo "${file.name}" já existe neste diretório.\n\n` +
            `Deseja substituir o arquivo existente?`
        );

        if (!confirm) {
            console.log('Upload cancelled by user');
            return; // User cancelled the upload
        }
    }

    // Use query string to pass directory parameter instead of FormData
    // This ensures the parameter is available before the upload handler starts
    const encodedPath = encodeURIComponent(currentPath);
    const uploadUrl = `/api/files/upload?dir=${encodedPath}`;

    const formData = new FormData();
    formData.append('file', file);

    const progressBar = document.getElementById('progressBar');
    const progressFill = document.getElementById('progressFill');
    progressBar.style.display = 'block';
    progressFill.style.width = '0%';
    progressFill.textContent = '0%';

    try {
        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percent = Math.round((e.loaded / e.total) * 100);
                progressFill.style.width = percent + '%';
                progressFill.textContent = percent + '%';
            }
        });

        xhr.addEventListener('load', () => {
            if (xhr.status === 200) {
                progressFill.style.width = '100%';
                progressFill.textContent = 'Upload completo!';
                setTimeout(() => {
                    progressBar.style.display = 'none';
                    refreshFiles();
                }, 1000);
            } else {
                console.error('Upload failed with status:', xhr.status);
                alert('Erro no upload: ' + xhr.status);
                progressBar.style.display = 'none';
            }
        });

        xhr.addEventListener('error', () => {
            console.error('Upload connection error');
            alert('Erro na conexão');
            progressBar.style.display = 'none';
        });

        console.log('Upload URL:', uploadUrl);
        xhr.open('POST', uploadUrl);
        xhr.send(formData);
    } catch (error) {
        console.error('Upload exception:', error);
        alert('Erro: ' + error.message);
        progressBar.style.display = 'none';
    }
}

// File editing functions
let currentEditFile = '';
let editor = null;

// Initialize Ace Editor
function initEditor() {
    if (!editor) {
        editor = ace.edit("fileEditor");
        editor.setTheme("ace/theme/monokai");
        editor.session.setMode("ace/mode/text");
        editor.setFontSize(14);
    }
}

async function editFile(name, event) {
    event.stopPropagation();
    const filepath = (currentPath + '/' + name).replace('//', '/');
    currentEditFile = filepath;

    try {
        const response = await fetch('/api/files/read?file=' + encodeURIComponent(filepath));
        const data = await response.json();

        if (data.error) {
            alert('Erro ao abrir arquivo: ' + data.error);
            return;
        }

        // Check file size
        if (data.size > 51200) {
            alert('Arquivo muito grande para edição (máx 50KB)');
            return;
        }

        // Open modal and populate editor
        // Open modal and populate editor
        document.getElementById('editorTitle').textContent = '✏️ Editar: ' + name;
        document.getElementById('editorModal').style.display = 'flex';

        // Initialize editor if needed and set content
        initEditor();

        // Detect mode based on extension
        const ext = name.split('.').pop().toLowerCase();
        let mode = "ace/mode/text";
        if (ext === 'js') mode = "ace/mode/javascript";
        else if (ext === 'html') mode = "ace/mode/html";
        else if (ext === 'css') mode = "ace/mode/css";
        else if (ext === 'json') mode = "ace/mode/json";
        else if (ext === 'cpp' || ext === 'h' || ext === 'ino') mode = "ace/mode/c_cpp";
        else if (ext === 'py') mode = "ace/mode/python";

        editor.session.setMode(mode);
        editor.setValue(data.content, -1); // -1 moves cursor to start
        editor.focus();
    } catch (error) {
        alert('Erro ao carregar arquivo: ' + error.message);
    }
}

async function saveFile() {
    const content = editor.getValue();
    const formData = new FormData();
    formData.append('file', currentEditFile);
    formData.append('content', content);

    try {
        const response = await fetch('/api/files/write', {
            method: 'POST',
            body: formData
        });

        const data = await response.json();

        if (data.error) {
            alert('Erro ao salvar: ' + data.error);
            return;
        }

        alert('Arquivo salvo com sucesso!');
        closeEditor();
        refreshFiles();
    } catch (error) {
        alert('Erro ao salvar arquivo: ' + error.message);
    }
}

function closeEditor() {
    document.getElementById('editorModal').style.display = 'none';
    if (editor) {
        editor.setValue('');
    }
    currentEditFile = '';
}

// Close modal when clicking outside
window.onclick = function (event) {
    const modal = document.getElementById('editorModal');
    if (event.target === modal) {
        closeEditor();
    }
}

// Drag and drop
const uploadZone = document.getElementById('uploadZone');

uploadZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    uploadZone.classList.add('dragging');
});

uploadZone.addEventListener('dragleave', () => {
    uploadZone.classList.remove('dragging');
});

uploadZone.addEventListener('drop', (e) => {
    e.preventDefault();
    uploadZone.classList.remove('dragging');
    handleFiles(e.dataTransfer.files);
});

// Initialize
refreshFiles();
