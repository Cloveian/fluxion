# same random key / click scripts through two fluxion-app builds, which PNGs differ
# usage: python3 tools/golden.py seed count old-app new-app    (keep a copy of the old binary before refactoring, cp build-pc/fluxion-app /tmp/old)
import hashlib, os, random, subprocess, sys, tempfile
from uikeys import script

random.seed(int(sys.argv[1]))
n = int(sys.argv[2])
apps = [os.path.abspath(a) for a in sys.argv[3:5]]

def render(app, keys):
    with tempfile.TemporaryDirectory(prefix='gold_') as d:
        png = os.path.join(d, 'o.png')
        try:
            subprocess.run([app, '--png', png, '--scale', '1'] + keys, capture_output=True, timeout=60, cwd=d)
            return hashlib.md5(open(png, 'rb').read()).hexdigest() if os.path.exists(png) else 'none'
        except subprocess.TimeoutExpired:
            return 'timeout'

same = 0
for i in range(n):
    keys = script(4, 24, 0.2)
    a, b = render(apps[0], keys), render(apps[1], keys)
    if a == b:
        same += 1
    else:
        print('DIFFERENT', ' '.join(keys))
print(f'{same} same, {n - same} different of {n} (seed {sys.argv[1]})')
