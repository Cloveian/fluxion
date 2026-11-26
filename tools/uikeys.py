# the key and click tokens uifuzz.py and golden.py draw their random scripts from
import random

toks = ['0','1','2','3','4','5','6','7','8','9','x','y','a','e','+','-','*','/','^','(',')',',','=','.','<','>','!',
        'enter','enter','enter','left','right','up','down','clear','del','2nd','alpha','math','apps','prgm','vars','stat','sin','cos','tan','ln','log',
        'x^2','x^-1','neg','sto','xt','on','menu','trig','esc','doc','cat','template','pi','ee','10^x','e^x','return','ctrl','shift',
        'on+a','on+c','on+x','on+v','on+z','alpha+left','alpha+right','alpha+click:20,150',
        'hover:20,150','hover:160,230','hover:60,60','click:20,150','click:100,100','click:35,172','click:10,230','click:160,230','click:300,220','click:5,5','click:160,120','drag:50,200','drag:200,200']
exprs = ['solve(x^2-5x+6,x)','integral(x*sin(x),x)','d/dx(x^3*sin(x))','limit(sin(x)/x,x,0)','expand((x+1)^3)','factor(x^2-1)','x^3-3x+1=0','sqrt(x^2-4)/x','(x^2-1)/(x-1)',
         'system(x+y=3,x-y=1)','sum(k,k,1,10)','tangent(x^2,x,1)','critical(x^3-3x,x)','increasing(x^3-3x,x)','implicit(x^2+y^2=25,x,y)','taylor(sin(x),0,5,x)','2x+1<5',
         'solve(x^5-5x+1=0,x)','solve(cos(x)=x,x)','solve(tan(x)=x,x)']

# fluxion-desktop spells them key:name, type:text, mouse and wheel
desktop_toks = ['type:x', 'type:2', 'type:+', 'type:-', 'type:*', 'type:/', 'type:^', 'type:(', 'type:)', 'type:,', 'type:=', 'type:<', 'type:sin(',
                'key:enter', 'key:enter', 'key:bs', 'key:del', 'key:left', 'key:right', 'key:up', 'key:down', 'key:home', 'key:end', 'key:tab', 'key:esc',
                'key:shift+left', 'key:shift+right', 'key:ctrl+left', 'key:ctrl+right', 'key:ctrl+enter', 'key:shift+enter', 'key:ctrl+bs',
                'key:f1', 'key:f2', 'key:f3', 'key:f4', 'key:f5', 'key:f6', 'key:f7', 'key:ctrl+k', 'key:ctrl+a', 'key:ctrl+c', 'key:ctrl+x', 'key:ctrl+z',
                'key:ctrl+v', 'wheel:1', 'wheel:-1', 'hover:20,150', 'hover:160,230', 'click:20,150', 'click:100,100', 'click:35,172', 'click:10,230',
                'click:160,230', 'click:300,220', 'click:5,5', 'shift+click:20,150', 'drag:50,200', 'drag:200,200']

# lo to hi tokens, a pasted command (clip: then ctrl+v) with this chance in place of one
def script(lo, hi, paste, desktop=False):
    pool, ctrl_v = (desktop_toks, 'key:ctrl+v') if desktop else (toks, 'on+v')
    keys = []
    for _ in range(random.randint(lo, hi)):
        if random.random() < paste:
            keys += ['clip:' + random.choice(exprs), ctrl_v]
        else:
            keys.append(random.choice(pool))
    return keys
