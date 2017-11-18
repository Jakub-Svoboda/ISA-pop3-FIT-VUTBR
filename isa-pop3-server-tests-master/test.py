#!/usr/bin/env python3

import codecs
import hashlib
import re
import time
import os
import signal
import shutil
import socket
import subprocess
import sys

from random import randint

POPSER_PATH = '../popser'
DEFAULT_SLEEP = 0.025
RETRY_SLEEP = 0.1
BEFORE_RECV_SLEEP = 0.05
RETRIES = 10
CHECK_NONCE_UNIQUENESS = True

port = randint(10000, 50000)
last_query = ''
last_id = ''
flags = set()
fail = []
conn = {}
msg_ids = []
inited_maildirs = set()
apop_nonces = {}

if sys.stdout.encoding is None or sys.stdout.encoding == 'ANSI_X3.4-1968':
    utf8_writer = codecs.getwriter('UTF-8')
    if sys.version_info.major < 3:
        sys.stdout = utf8_writer(sys.stdout, errors='replace')
    else:
        sys.stdout = utf8_writer(sys.stdout.buffer, errors='replace')


def get_port():
    global port
    port += 1
    return str(port)

def open_socket(retries=RETRIES):
    global port, fail
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', port))
        s.settimeout(0.1)
        return s
    except ConnectionRefusedError:
        if retries == 0:
            fail.append('Chyba navázání spojení se serverem (port %d)' % port)
            return None
        time.sleep(RETRY_SLEEP)
        return open_socket(retries-1)

def my_send(sock, s_msg):
    global fail
    if sock is None:
        return False
    totalsent = 0
    msg = s_msg.encode('utf-8')
    try:
        while totalsent < len(s_msg):
            sent = sock.send(msg[totalsent:])
            if sent == 0:
                return False
            totalsent = totalsent + sent
    except socket.timeout:
        fail.append('Odesílání zprávy trvalo příliš dlouho (timeout)')
        return False
    return True

def my_receive(sock):
    global fail
    if sock is None:
        return '\n'
    time.sleep(BEFORE_RECV_SLEEP)
    #chunks = []
    #bytes_recd = 0
    #while bytes_recd < msg_len:
    try:
        chunk = sock.recv(4096)#min(msg_len - bytes_recd, 2048))
    except socket.timeout:
        fail.append('Příjem zprávy trval příliš dlouho (timeout)')
        return '\n'

    return chunk.decode("utf-8")
    if chunk == b'':
        return '\n'
    #chunks.append(chunk)
    #bytes_recd = bytes_recd + len(chunk)
    #return b''.join(chunks)

def get_connection(id):
    global conn
    if id not in conn:
        conn[id] = open_socket()
    return conn[id]

def translate_callback(r):
    global last_id
    last_id = translate_id(r.group(2))
    return r.group(1) + last_id

def do_send_msg(conn_id, val):
    global fail, last_query
    val = re.sub(r'^([A-Za-z]+ )([0-9]+)', translate_callback, val)
    if 'apop' in flags and val[:4].upper() == 'APOP':
        val = re.sub(r'(APOP .+? )(.+)', lambda x: apop_hash(conn_id, x), val)
    #print("Sending:", val)
    if not my_send(get_connection(conn_id), val + "\r\n"):
        fail.append('Chyba při odesílání zprávy:\n  ' + val[:70])
        return False
    last_query = val
    return True

def do_recv_msg(conn_id, val):
    global fail, last_id
    s = my_receive(get_connection(conn_id))

    print("Received:", s,end="")
    if re.match('[^\r]\n', s):
        fail.append('Server ve zprávě vrátil \\n, před kterým nebylo' +
                    ' \\r\n  ' + val[:70])
        return False
    s = s.replace('\r', '')
    if len(s) == 0 or s[-1] != '\n':
        print(s,len(s))
        fail.append('Server vrátil zprávu, která nekončí \\n\n  ' + val[:70])
        return False
    s = s[:-1]
    if last_query.upper() == 'LIST':
        return check_list(s, val)
        
    if 'apop' in flags and val == '<nonce>':
        return check_apop_nonce(conn_id, s)
    
    val = val.replace(r'<id>', last_id)
    val_r = re.escape(val)
    val_r = val_r.replace(r'\ \<any\>', '.*')
    if not re.match(val_r, s):
        str = ('Server vrátil jinou odpověď na dotaz "{}"\n  ' +
               'Chci: {}\n  Dostal jsem: {}').format(last_query,
               val.replace('\n', '\n' + 8 * ' '),
               s.replace('\n', '\n' + 15 * ' '))
        fail.append(str)
        return False
    return True

def rmdir(dir):
    try:
        shutil.rmtree(dir)
    except FileNotFoundError:
        pass

def do_action(action):
    if action[0] == 'init_maildir':
        dir = action[1]
        if '/' in dir:
            return False
        rmdir(dir + '/new')
        rmdir(dir + '/cur')
        shutil.copytree(dir + '/bac', dir + '/new')
        os.mkdir(dir + '/cur')
        return True
    if action[0] == 'reset_server':
        subprocess.call([POPSER_PATH, '-r'])
        return True
    return False

def translate_id(id):
    global msg_ids, last_id
    if int(id) >= len(msg_ids):
        return id
    return msg_ids[int(id)][0]

def check_list(rec, exp):
    global fail, msg_ids
    out = True
    lines_rec = rec.split('\n')
    lines_exp = exp.split('\n')
    
    if lines_rec[0][:3] != '+OK' or lines_rec[-1] != '.':
        out = False
    if len(lines_rec) != len(lines_exp):
        out = False
    
    msg_ids = [None]
    for line in lines_exp[1:-1]:
        id, size = line.split()
        msg_ids.append([None, size])
    
    for line in lines_rec[1:-1]:
        id, size = line.split()
        for i in range(1, len(msg_ids)):
            if msg_ids[i] == [None, size]:
                msg_ids[i][0] = id
                break
        else:
            out = False
    if not out:
        str = ('Server vrátil jinou odpověď na dotaz "{}"\n  ' +
               'Chci: {}\n  Dostal jsem: {}').format(last_query,
               exp.replace('\n', '\n' + 8 * ' '),
               rec.replace('\n', '\n' + 15 * ' '))
        fail.append(str)
    return out

def check_apop_nonce(conn_id, msg):
    global fail, apop_nonces
    nonce_regex_res = re.search("<.+>", msg)
    if msg[:4] != '+OK ' or nonce_regex_res is None:
        fail.append('Nesprávný formát úvodní zprávy:\n  ' + msg)
        return False
    nonce = nonce_regex_res.group(0)
    if CHECK_NONCE_UNIQUENESS and (nonce in apop_nonces.values()):
        fail.append('Nonce není unikátní:\n  ' + nonce)
        return False
    apop_nonces[conn_id] = nonce
    return True
    
def apop_hash(conn_id, msg):
    str = (apop_nonces[conn_id] + msg.group(2)).encode('utf-8')
    return msg.group(1) + hashlib.md5(str).hexdigest()

def read_file(data):
    m_key = None
    m_val = None
    for row in data.split('\n'):
        key, val = re.split(' {1,4}', row, 1)
        if key == '':
            m_val += '\n' + val
        else:
            if m_key is not None:
                yield m_key, m_val
            m_key = key
            m_val = val
    yield m_key, m_val
        

def do_test(test):
    global last_query, flags, fail, conn
    
    last_query = ''
    flags = set()
    fail = []
    conn = {}
    apop_nonces = {}
    
    l = test.split('\n', 2)
    if len(l) == 2:
        l.append('? ')
    name, cmd, data = l
    
    args = [POPSER_PATH]
    for a in cmd.split()[1:]:
        if a == '<port>':
            a = get_port()
        if args[-1] == '-d':
            if a not in inited_maildirs:
                do_action(['init_maildir', a])
                if len(inited_maildirs) > 0:
                    do_action(['reset_server'])
                inited_maildirs.add(a)
        args.append(a)
    srv = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(DEFAULT_SLEEP)
    
    for key, val in read_file(data):
        if key == 'F':
            # Nastavení testovacího skriptu
            flags.add(val)
        elif key == '':
            print(val)
        elif key[0] == 'C':
            # Poslat paket
            if not do_send_msg(key[1:], val):
                break
        elif key[0] == 'S':
            # Přečíst paket
            if not do_recv_msg(key[1:], val):
                break
        elif key[0] == 'A':
            # Provede akci
            if not do_action(val.split()):
                fail.append('Selhala akce: ' + val)
                break
    
    for _, c in conn.items():
        if c is not None:
            c.close()
    
    if 'wait' in flags:
        retcode = srv.wait()
        if bool(retcode) != ('fail' in flags):
            want = 'negativní' if ('fail' in flags) else '0'
            got = retcode
            fail.append('Neočekávaný návrarový kód (chci %s, dostal jsem %s)' %
                        (want, got))
    else:
        srv.send_signal(signal.SIGINT)
    
    out = srv.stdout.read()
    err = srv.stderr.read()
    printed_stdout = out != b''
    printed_stderr = err != b''
    
    if (printed_stdout and 'stdout' not in flags):
        fail.append('Sever vytiskl něco na stdout, i když neměl')
    if (not printed_stdout and 'stdout' in flags):
        fail.append('Sever nevytiskl nic na stdout, ale měl')
    if (printed_stderr and 'stderr' not in flags):
        fail.append('Sever vytiskl něco na stderr, i když neměl\n  ' +
                    err.decode("utf-8")[:70].strip())
    if (not printed_stderr and 'stderr' in flags):
        fail.append('Sever nevytiskl nic na stderr, ale měl')
    
    ok = len(fail) == 0
    
    if ok:
        print(Color.GRAY + name.ljust(74) + Color.GREEN + "[ OK ]" + Color.ENDC)
    else:
        print(Color.BOLD + name.ljust(74) + Color.RED + "[FAIL]" + Color.ENDC)
        for e in fail:
            print(Color.YELLOW + "* " + e + Color.ENDC)

class Color:
    OKBLUE = '\033[94m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    PURPLE = '\033[95m'
    CYAN = '\033[96m'
    DARKCYAN = '\033[36m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    LIGHTGRAY = '\033[37m'
    GRAY = '\033[90m'
    RED = '\033[91m'

with open('tests.txt', 'r', encoding='utf-8') as f:
    for test in re.split('\n+-{3,}\n+', f.read().strip()):
        if test.startswith('*'):
            break
        do_test(test)
    do_action(['reset_server'])
