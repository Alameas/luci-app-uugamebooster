#!/usr/bin/env python3
import argparse
import getpass
import json
import sys
import zipfile

from account import Account
from apk import setup
from router import Router, discover as discover_gateways, gateways, router_info
from state import State


def parse_args():
    parser = argparse.ArgumentParser(description='UU console client')
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('setup', help='Download the official APK and extract its version and certificate')
    command.add_argument('--apk', help='Use a local official APK')
    command = commands.add_parser('login', help='Log in with an SMS code')
    command.add_argument('--country', default='86', help='Country code (default: 86)')
    command.add_argument('--phone', help='Phone number; omit to enter it privately')
    commands.add_parser('discover', help='Discover routers at local gateways')
    commands.add_parser('list', help='List routers bound to the current account')
    for name in ('bind', 'unbind'):
        command = commands.add_parser(name, help=f'{name.capitalize()} a discovered router')
        command.add_argument('ip', nargs='?', help='Router IP address')
    return parser.parse_args()


def discover(state):
    routers = discover_gateways()
    state.data['routers'] = routers
    state.save()
    return routers


def select_router(state, command, ip):
    if ip:
        interface = next((name for address, name in gateways() if address == ip), 'manual')
        print(f'Probing {ip} (interface: {interface})', flush=True)
        return router_info(ip)
    routers = discover(state)
    if not routers:
        raise ValueError('No UU router found')
    if len(routers) > 1:
        addresses = ', '.join(router['ip'] for router in routers)
        raise ValueError(f'Multiple routers found: {addresses}; use: python uu.py {command} <ip>')
    return routers[0]


def main():
    args = parse_args()
    state = State()
    if args.command == 'setup':
        setup(state, args.apk)
    elif args.command == 'login':
        phone = args.phone or getpass.getpass('Phone number: ').strip()
        if not phone.isdigit():
            raise ValueError('Phone number must contain only digits')
        Account(state).login(phone, args.country)
    elif args.command == 'discover':
        print(json.dumps(discover(state), ensure_ascii=False, indent=2))
    elif args.command == 'list':
        print(json.dumps(Router(Account(state)).list_bound(), ensure_ascii=False, indent=2))
    else:
        info = select_router(state, args.command, args.ip)
        getattr(Router(Account(state)), args.command)(info)
        state.data['routers'] = [info]
        state.save()
        print('Router bound.' if args.command == 'bind' else 'Router unbound.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
        print(f'Error: {exc}', file=sys.stderr)
        sys.exit(1)
