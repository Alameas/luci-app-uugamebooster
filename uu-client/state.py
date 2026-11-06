import json
import os
import secrets
from pathlib import Path
from tempfile import NamedTemporaryFile

ROOT = Path(__file__).resolve().parent
CONFIG = ROOT / '.uu-config.json'
STATE = ROOT / '.uu-state.json'
CONFIG_KEYS = {'certificate', 'device_id', 'app_version', 'app_version_code'}


class State:
    def __init__(self):
        self.data = {}
        for path in (CONFIG, STATE):
            if path.exists():
                self.data.update(json.loads(path.read_text()))

    def ensure_device(self):
        if not self.data.get('device_id'):
            self.data['device_id'] = secrets.token_hex(8)
        return self.data['device_id']

    def save(self):
        config = {key: value for key, value in self.data.items() if key in CONFIG_KEYS}
        state = {key: value for key, value in self.data.items() if key not in CONFIG_KEYS}
        for path, data in ((CONFIG, config), (STATE, state)):
            with NamedTemporaryFile(mode='w', encoding='utf-8', dir=ROOT, prefix='.uu-', delete=False) as f:
                os.fchmod(f.fileno(), 0o600)
                json.dump(data, f, ensure_ascii=False, indent=2)
                f.write('\n')
            os.replace(f.name, path)
