# random key and click scripts through the app's --png mode, looking for crashes and sanitizer reports
# usage: python3 tools/uifuzz.py seed count [path to fluxion-app or fluxion-desktop, an asan build finds the most]
import random, subprocess, sys, os, tempfile
from uikeys import script
random.seed(int(sys.argv[1]))
n = int(sys.argv[2])
app = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else 'build-pc/fluxion-app')
bad = 0
for i in range(n):
    keys = script(5, 30, 0.25, 'desktop' in os.path.basename(app))
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='print_stacktrace=1')
    with tempfile.TemporaryDirectory(prefix='uif_') as d:
        try:
            p = subprocess.run([app, '--png', os.path.join(d, 'o.png'), '--scale', '1'] + keys, capture_output=True, text=True, timeout=60, env=env, cwd=d)
        except subprocess.TimeoutExpired:
            print('TIMEOUT', ' '.join(keys)); bad += 1; continue
    out = p.stdout + p.stderr
    if p.returncode != 0 or 'runtime error' in out or 'AddressSanitizer' in out:
        bad += 1
        print('FAIL rc', p.returncode, ' '.join(keys)); print(out[:1500])
print('done', n, 'bad', bad)
