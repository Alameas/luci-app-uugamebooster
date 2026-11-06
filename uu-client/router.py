import json
import urllib.parse
import urllib.request

import netifaces

from account import BASE, check

LOCAL_HTTP = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def gateways():
    return [(address, interface) for address, interface, _ in
            netifaces.gateways().get(netifaces.AF_INET, [])]


def router_info(host, timeout=3):
    with LOCAL_HTTP.open(f'http://{host}:16363/router', timeout=timeout) as response:
        lines = response.read(8192).decode('utf-8').splitlines()

    info = {key.strip(): value.strip() for line in lines if ':' in line
            for key, value in [line.split(':', 1)]}
    if not info.get('sn') or not info.get('type'):
        raise ValueError('Not a UU router')
    return {'ip': host, **info}


def discover():
    routers = []
    for address, interface in gateways():
        print(f'Probing {address} (interface: {interface})', flush=True)
        try:
            routers.append(router_info(address, timeout=0.8))
        except (OSError, ValueError):
            continue
    return routers


class Router:
    def __init__(self, account):
        self.account = account

    def request(self, path, payload, headers=None):
        h = {'SystemType': 'android', 'Client-Origin': '3001', 'Content-Type': 'application/json', **(headers or {})}
        req = urllib.request.Request(BASE + '/api/' + path, data=json.dumps(payload).encode(), headers=h)
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r)

    def list_bound(self):
        if not self.account.session:
            raise ValueError('Run login first')
        query = urllib.parse.urlencode({'uu_sess_id': self.account.session})
        url = f'{BASE}/api/routers/?{query}'
        req = urllib.request.Request(url, headers={'SystemType': 'android', 'Client-Origin': '3001'})
        with urllib.request.urlopen(req, timeout=20) as response:
            result = json.load(response)
        check(result, 'List routers')
        self.account.state.data['bound_routers_response'] = result
        self.account.state.save()
        return result.get('routers', [])

    def bind(self, info):
        session = self.account.session
        if not session:
            raise ValueError('Run login first')
        result = self.request('sess', {'type': info['type'], 'sn': info['sn']})
        check(result, 'Router session')
        sess = result['sess']
        result = self.request('login-uu-sess', {'uu_sess_id': session, 'router_model': info.get('model', '')}, {'sess': sess})
        check(result, 'Bind')
        self.account.state.data['last_router_response'] = {'operation': 'bind', 'response': result}
        return sess

    def unbind(self, info):
        sess = self.bind(info)
        result = self.request('unbind-previous', {'type': info['type'], 'sn': info['sn']},
                              {'Ntes-UU': self.account.session, 'sess': sess})
        check(result, 'Unbind')
        self.account.state.data['last_router_response'] = {'operation': 'unbind', 'response': result}
