# a layer may only include from the layers below it. prints every #include that points up or sideways and exits 1
# usage: python3 tools/layers.py   (make test runs it)
import os, re, sys

# folder under src/ -> what its files may include, a folder or one file. the bottom is first
ALLOWED = {
    'engine': ['engine'],
    'engine/pc': ['engine', 'engine/pc'],   # the command line and test runner
    'platform': ['platform'],
    'render': ['engine', 'render', 'platform/screen.h'],
    'editor': ['engine', 'render', 'editor'],
    'input': ['engine', 'editor', 'input', 'platform/keys.h', 'platform/keys_pc.h', 'platform/events_pc.h'],
    'ui': ['engine', 'render', 'editor', 'input', 'platform', 'ui'],
    '(root)': ['engine', 'render', 'editor', 'input', 'platform', 'ui', '(root)'],   # main.c, main_pc.c
}

def where(rel):
    if rel.startswith('engine/pc/'):
        return 'engine/pc'
    return rel.split('/')[0] if '/' in rel else '(root)'

def resolve(here, inc):
    # next to the file, from src/, or from src/engine (the engine's own -I)
    for base in (here, 'src', 'src/engine'):
        path = os.path.normpath(os.path.join(base, inc))
        if os.path.exists(path):
            return os.path.relpath(path, 'src')
    return None

bad = 0
for dirpath, _, names in sorted(os.walk('src')):
    for name in sorted(names):
        if not name.endswith(('.c', '.h')):
            continue
        path = os.path.join(dirpath, name)
        rel = os.path.relpath(path, 'src')
        mine = where(rel)
        for n, line in enumerate(open(path, errors='ignore'), 1):
            m = re.match(r'\s*#\s*include\s+"([^"]+)"', line)
            target = resolve(dirpath, m.group(1)) if m else None
            if target is None or where(target) == mine:
                continue
            if target not in ALLOWED[mine] and where(target) not in ALLOWED[mine]:
                print(f'{path}:{n}: {mine} includes {target}')
                bad += 1
if bad:
    print(bad, 'include(s) point the wrong way')
    sys.exit(1)
