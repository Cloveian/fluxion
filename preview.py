#!/usr/bin/env python3
# local docs preview. makes a venv if there isn't one, installs requirements,
# builds once so broken links show up, then serves. nav is by hand in mkdocs.yml

import os, socket, subprocess, sys, threading, time, webbrowser
from pathlib import Path

root = Path(__file__).parent
venv = root / ".venv"
py = str(venv / "bin" / "python")
mkdocs = str(venv / "bin" / "mkdocs")
os.chdir(root)

if not venv.exists():
    print("making the venv...", flush=True)
    subprocess.run([sys.executable, "-m", "venv", str(venv)], check=True)
subprocess.run([py, "-m", "pip", "install", "-q", "-r", "requirements.txt"], check=True)

# build once and print whatever mkdocs complains about (broken links etc). not fatal, still serve.
r = subprocess.run([mkdocs, "build", "--site-dir", str(root / "site")], capture_output=True, text=True)
for ln in (r.stderr + r.stdout).splitlines():
    if "WARNING" in ln or "anchor" in ln:
        print(ln)

# first free port in 8001-8020
port = None
for p in range(8001, 8021):
    s = socket.socket()
    try:
        s.bind(("127.0.0.1", p))
        s.close()
        port = p
        break
    except OSError:
        s.close()
if port is None:
    sys.exit("no free port in 8001-8020")

url = "http://127.0.0.1:%d" % port
print("preview on " + url, flush=True)

# open the browser after the server's had a sec to come up
def opener():
    time.sleep(2)
    try:
        webbrowser.open(url)
    except Exception:
        pass
threading.Thread(target=opener, daemon=True).start()

subprocess.run([mkdocs, "serve", "--dev-addr", "127.0.0.1:%d" % port])
