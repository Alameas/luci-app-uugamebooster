import base64
import re
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from tempfile import NamedTemporaryFile

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.serialization import pkcs7
from pyaxmlparser import APK

DOWNLOAD = 'https://adl.netease.com/d/g/uu/c/uu_router'
UA = 'Mozilla/5.0 (Linux; Android 13) AppleWebKit/537.36 Mobile Safari/537.36'


def official_apk_url():
    req = urllib.request.Request(DOWNLOAD, headers={'User-Agent': UA})
    with urllib.request.urlopen(req, timeout=20) as r:
        page = r.read().decode('utf-8')
    match = re.search(r'android_link\s*=.*?["\'](https://[^"\']+\.apk)["\']', page)
    if not match or urllib.parse.urlparse(match.group(1)).hostname != 'uurouter.gdl.netease.com':
        raise ValueError('Official APK link not found; provide --apk instead')
    return match.group(1)


def setup(state, apk=None):
    if apk:
        parse_apk(state, Path(apk))
        return

    print('Checking the official UU download page…', flush=True)
    url = official_apk_url()
    print('Downloading the official APK…', flush=True)
    req = urllib.request.Request(url, headers={'User-Agent': UA})
    with urllib.request.urlopen(req, timeout=90) as r, NamedTemporaryFile(suffix='.apk') as f:
        while chunk := r.read(1024 * 1024):
            f.write(chunk)
        f.flush()
        parse_apk(state, Path(f.name))


def signing_certificate(path):
    with zipfile.ZipFile(path) as z:
        names = [x for x in z.namelist() if re.fullmatch(r'META-INF/[^/]+\.(RSA|DSA|EC)', x, re.I)]
        if len(names) != 1:
            raise ValueError('APK must contain one v1 signature block')
        certs = pkcs7.load_der_pkcs7_certificates(z.read(names[0]))
    if len(certs) != 1:
        raise ValueError('APK signature block must contain one certificate')
    return certs[0].public_bytes(serialization.Encoding.DER)


def parse_apk(state, path):
    apk = APK(str(path))
    if apk.get_package() != 'com.netease.uurouter':
        raise ValueError('APK package is not com.netease.uurouter')
    version = apk.get_androidversion_name()
    code = apk.get_androidversion_code()
    if not version or not code:
        raise ValueError('APK version is missing')
    der = signing_certificate(path)
    state.data.update(certificate=base64.b64encode(der).decode(), app_version=version, app_version_code=str(code))
    state.ensure_device()
    state.save()
    print(f'Initialized UU app {version} (build {code}).')
