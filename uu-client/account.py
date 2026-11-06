import base64
import getpass
import hashlib
import json
import string
import time
import urllib.parse
import urllib.request

from cryptography.hazmat.primitives import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

BASE = 'https://router.uu.163.com'


def check(result, action):
    if result.get('status') != 'ok':
        raise ValueError(f"{action} failed: {result.get('status', 'unknown')}")


def shift(text):
    groups = (string.ascii_lowercase, string.ascii_uppercase, string.digits)
    table = str.maketrans(''.join(groups), ''.join(group[2:] + group[:2] for group in groups))
    return text.translate(table)


class Account:
    def __init__(self, state):
        self.state = state
        data = state.data
        if not all(data.get(k) for k in ('certificate', 'device_id', 'app_version', 'app_version_code')):
            raise ValueError('Run setup first to download the APK and initialize the client')
        self.device = data['device_id']
        self.key = hashlib.md5(base64.b64decode(data['certificate']).hex().encode()).hexdigest()
        raw = self.device.encode()
        self.aes = raw[:16] if len(raw) >= 16 else hashlib.md5(raw).digest()
        self.session = data.get('session')

    def crypt(self, data, decrypt=False):
        cipher = Cipher(algorithms.AES(self.aes), modes.ECB())
        if decrypt:
            dec = cipher.decryptor()
            unpad = padding.PKCS7(128).unpadder()
            return unpad.update(dec.update(data) + dec.finalize()) + unpad.finalize()
        pad = padding.PKCS7(128).padder()
        enc = cipher.encryptor()
        return enc.update(pad.update(data) + pad.finalize()) + enc.finalize()

    def api(self, path, payload=None):
        body = '' if payload is None else json.dumps(payload, ensure_ascii=False, separators=(',', ':'))
        seed = str(int(time.time()))
        source = 'seed=' + seed + '&key=' + self.key
        if body:
            source += '&post_content=' + urllib.parse.quote(body, safe='/~')
        headers = {'SystemType': 'android', 'SystemVersion': '37',
                   'AppVersionCode': self.state.data['app_version'], 'AppVersion': self.state.data['app_version_code'],
                   'BundleId': 'com.netease.uurouter', 'Channel': 'official', 'Locale': 'zh_CN',
                   'Content-Type': 'text/plain; charset=utf-8', 'EAD': shift(self.device), 'Seed': seed,
                   'Sign': hashlib.md5(source.encode()).hexdigest().upper()}
        if self.session:
            headers['Ntes-UU'] = self.session
        data = base64.b64encode(self.crypt(body.encode())) if body else b''
        req = urllib.request.Request(BASE + '/app/' + path, data=data, headers=headers)
        with urllib.request.urlopen(req, timeout=20) as response:
            raw = response.read()
            new_session = response.headers.get('Ntes-UU')
            if response.headers.get('Encryption', 'false').lower() != 'false':
                raw = self.crypt(base64.b64decode(raw), decrypt=True)
            result = json.loads(raw)
            if new_session:
                self.session = new_session
        return result

    def login(self, phone, country):
        self.session = None
        check(self.api('auth'), 'Device authentication')
        check(self.api('code', {'country_code': country, 'mobile': phone, 'type': 0, 'for': 0, 'times': 0}), 'Send code')
        code = getpass.getpass('SMS code: ').strip()
        result = self.api('account/login', {'country_code': country, 'mobile': phone, 'code': code})
        check(result, 'Login')
        if not self.session:
            raise ValueError('Login did not return a session')
        self.state.data['session'] = self.session
        self.state.data['user_info'] = result.get('user_info', {})
        self.state.data['login_response'] = result
        self.state.save()
        print('Login successful.')
