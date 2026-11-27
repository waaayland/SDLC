"""Визуализация моделей SysML v2 в PNG через ядро Jupyter SysML (Pilot Implementation).

Запуск (из каталога srs/models):
    docker build -t sysml-pilot .
    docker run --rm -v "<путь к srs>:/work" -w /work/models sysml-pilot python render.py

Все файлы *.sysml загружаются в одно ядро, затем для каждой строки views.txt
выполняется команда %viz, результат сохраняется в ../images/<имя>.png.
"""
import glob
import os

import cairosvg
import nbformat
from nbclient import NotebookClient

OUT_DIR = os.path.join('..', 'images')

models = sorted(glob.glob('*.sysml'))
views = []
with open('views.txt', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        if line and not line.startswith('#'):
            name, args = line.split('|', 1)
            views.append((name.strip(), args.strip()))

cells = [nbformat.v4.new_code_cell(open(m, encoding='utf-8').read()) for m in models]
cells += [nbformat.v4.new_code_cell('%viz ' + args) for _, args in views]
nb = nbformat.v4.new_notebook(cells=cells)
nb.metadata['kernelspec'] = {'name': 'sysml', 'display_name': 'SysML', 'language': 'sysml'}
NotebookClient(nb, timeout=600, kernel_name='sysml').execute()

errors = 0
for model, cell in zip(models, nb.cells):
    for out in cell.outputs:
        text = out.get('text') or out.get('data', {}).get('text/plain', '')
        print(f'{model}: {text.strip()}')
        errors += 'ERROR' in text

os.makedirs(OUT_DIR, exist_ok=True)
for (name, _), cell in zip(views, nb.cells[len(models):]):
    svg = next((o['data']['image/svg+xml'] for o in cell.outputs
                if 'image/svg+xml' in o.get('data', {})), None)
    if svg is None:
        errors += 1
        print(f'{name}: FAILED', [o.get('text') or o.get('data') for o in cell.outputs])
        continue
    cairosvg.svg2png(bytestring=svg.encode('utf-8'), scale=2, background_color='white',
                     write_to=os.path.join(OUT_DIR, name + '.png'))
    print(f'{name}: ok')

raise SystemExit(1 if errors else 0)
