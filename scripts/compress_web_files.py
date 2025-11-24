#!/usr/bin/env python3
"""
Script para comprimir arquivos web (HTML, CSS, JS) com gzip
Executado automaticamente antes do build pelo PlatformIO
"""

import os
import gzip
import shutil
from pathlib import Path

def compress_file(input_file, output_file):
    """Comprime um arquivo usando gzip"""
    with open(input_file, 'rb') as f_in:
        with gzip.open(output_file, 'wb', compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)

    # Mostrar economia
    original_size = os.path.getsize(input_file)
    compressed_size = os.path.getsize(output_file)
    savings = 100 * (1 - compressed_size / original_size)

    print(f"  {input_file.name:30s} {original_size:6d} → {compressed_size:6d} bytes ({savings:5.1f}% saved)")

    return original_size, compressed_size

def compress_web_files(data_dir='data/web'):
    """Comprime todos os arquivos web (.html, .css, .js)"""
    data_path = Path(data_dir)

    if not data_path.exists():
        print(f"⚠️  Directory {data_dir} not found!")
        return

    print("🗜️  Compressing web files with gzip...")
    print(f"{'File':<32s} {'Original':>8s} → {'Compressed':>10s}  {'Savings':>8s}")
    print("-" * 75)

    total_original = 0
    total_compressed = 0
    files_compressed = 0

    # Extensões para comprimir
    extensions = ('.html', '.css', '.js')

    for file_path in sorted(data_path.rglob('*')):
        if file_path.is_file() and file_path.suffix in extensions:
            # Não comprimir arquivos já .gz
            if file_path.suffix == '.gz':
                continue

            gz_path = file_path.with_suffix(file_path.suffix + '.gz')

            # Comprimir apenas se o .gz não existe ou é mais antigo
            if not gz_path.exists() or gz_path.stat().st_mtime < file_path.stat().st_mtime:
                orig, comp = compress_file(file_path, gz_path)
                total_original += orig
                total_compressed += comp
                files_compressed += 1

    if files_compressed > 0:
        total_savings = 100 * (1 - total_compressed / total_original)
        print("-" * 75)
        print(f"{'TOTAL':<32s} {total_original:6d} → {total_compressed:6d} bytes ({total_savings:5.1f}% saved)")
        print(f"✅ Compressed {files_compressed} files")
    else:
        print("✅ All files already compressed (up to date)")

if __name__ == '__main__':
    # Executar do diretório raiz do projeto
    project_root = Path(__file__).parent.parent
    os.chdir(project_root)

    compress_web_files()
