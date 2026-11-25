# random key and click scripts through the app's --png mode, looking for crashes and sanitizer reports
# usage: python3 tools/uifuzz.py seed count [path to fluxion-app, an asan build finds the most]
import random, subprocess, sys, os, tempfile
random.seed(int(sys.argv[1]))
n = int(sys.argv[2])
app = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else 'build-pc/fluxion-app')
toks = ['0','1','2','3','4','5','6','7','8','9','x','y','a','e','+','-','*','/','^','(',')',',','=','.','<','>','!',
        'enter','enter','enter','left','right','up','down','clear','del','2nd','alpha','math','apps','prgm','vars','stat','sin','cos','tan','ln','log',
        'x^2','x^-1','neg','sto','xt','on','menu','trig','esc','doc','cat','template','pi','ee','10^x','e^x','return','ctrl','shift',
        'on+a','on+c','on+x','on+v','on+z','alpha+left','alpha+right','alpha+click:20,150',
        'click:20,150','click:100,100','click:35,172','click:10,230','click:160,230','click:300,220','click:5,5','click:160,120','drag:50,200','drag:200,200']
exprs = ['solve(x^2-5x+6,x)','integral(x*sin(x),x)','d/dx(x^3*sin(x))','limit(sin(x)/x,x,0)','expand((x+1)^3)','factor(x^2-1)','x^3-3x+1=0','sqrt(x^2-4)/x','(x^2-1)/(x-1)',
         'system(x+y=3,x-y=1)','sum(k,k,1,10)','tangent(x^2,x,1)','critical(x^3-3x,x)','increasing(x^3-3x,x)','implicit(x^2+y^2=25,x,y)','taylor(sin(x),0,5,x)','2x+1<5']
bad = 0
for i in range(n):
    keys = []
    for _ in range(random.randint(5, 30)):
        if random.random() < 0.25:
            keys += ['clip:' + random.choice(exprs), 'on+v']
        else:
            keys.append(random.choice(toks))
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
