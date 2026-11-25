# the same random key / click scripts through two builds of fluxion-app, a PNG each, and which ones came out different
# usage: python3 tools/golden.py seed count old-app new-app    (keep a copy of the old binary before refactoring, cp build-pc/fluxion-app /tmp/old)
import hashlib, os, random, subprocess, sys, tempfile

random.seed(int(sys.argv[1]))
n = int(sys.argv[2])
apps = [os.path.abspath(a) for a in sys.argv[3:5]]
toks = ['0','1','2','3','4','5','6','7','8','9','x','y','a','+','-','*','/','^','(',')',',','.','<','>','!',
        'enter','enter','enter','left','right','up','down','clear','del','2nd','alpha','math','apps','prgm','vars','stat','sin','cos','tan','ln','log',
        'x^2','x^-1','neg','sto','xt','on','menu','trig','esc','doc','cat','template','pi','ee','10^x','e^x','return','ctrl','shift',
        'on+a','on+c','on+x','on+v','on+z','alpha+left','alpha+right','alpha+click:20,150',
        'click:20,150','click:100,100','click:35,172','click:10,230','click:160,230','click:300,220','click:5,5','click:160,120','drag:50,200','drag:200,200']
exprs = ['solve(x^2-5x+6,x)', 'expand((x+1)^3)', 'factor(x^2-1)', '(x^2-1)/(x-1)', 'system(x+y=3,x-y=1)', 'sqrt(x^2-4)/x', '2x+1<5']

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
    keys = []
    for _ in range(random.randint(4, 24)):
        if random.random() < 0.2:
            keys += ['clip:' + random.choice(exprs), 'on+v']
        else:
            keys.append(random.choice(toks))
    a, b = render(apps[0], keys), render(apps[1], keys)
    if a == b:
        same += 1
    else:
        print('DIFFERENT', ' '.join(keys))
print(f'{same} same, {n - same} different of {n} (seed {sys.argv[1]})')
