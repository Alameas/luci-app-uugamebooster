// SPDX-License-Identifier: Apache-2.0

'use strict';
'require form';
'require poll';
'require rpc';
'require uci';
'require view';

const callServiceList = rpc.declare({ object: 'service', method: 'list', params: ['name'], expect: { '': {} } });
const callDevices = rpc.declare({ object: 'luci.uugamebooster', method: 'devices', reject: true });
const callDHCPLeases = rpc.declare({ object: 'luci-rpc', method: 'getDHCPLeases', params: ['family'], reject: true });

const deviceTypes = {
	ally: 'ROG Ally',
	android: 'Android',
	apple: 'Apple',
	ios: 'iOS',
	macos: 'macOS',
	oculus: 'Oculus / Meta Quest',
	pico: 'PICO',
	ps4: 'PlayStation 4',
	ps5: 'PlayStation 5',
	ps_portal: 'PlayStation Portal',
	steam_deck: 'Steam Deck',
	switch: 'Nintendo Switch',
	switch2: 'Nintendo Switch 2',
	switch_cn: 'Nintendo Switch (CN)',
	tmgz: 'Tamagotchi Uni',
	vision_pro: 'Apple Vision Pro',
	windows: 'Windows',
	xbox: 'Xbox',
	xboxs: 'Xbox Series S',
	xboxx: 'Xbox Series X'
};

function readStatus() {
	return Promise.all([
		L.resolveDefault(callDHCPLeases(4), null),
		L.resolveDefault(callDevices(), null),
		L.resolveDefault(callServiceList('uugamebooster'), {})
	]);
}

function isRunning(services) {
	try {
		return !!services.uugamebooster.instances.uugamebooster.running;
	} catch (e) {
		return false;
	}
}

function renderStatus(isRunning) {
	var spanTemp = '<span style="color:%s"><strong>%s %s</strong></span>';
	var renderHTML;
	if (isRunning) {
		renderHTML = spanTemp.format('green', _('UU GameAcc'), _('RUNNING'));
	} else {
		renderHTML = spanTemp.format('red', _('UU GameAcc'), _('NOT RUNNING'));
	}
	return renderHTML;
}

function configuredType(mac) {
	var sections = uci.sections('uugamebooster', 'device_type');
	return sections.find(function(section) {
		return (section.mac || '').toUpperCase() === mac;
	});
}

function setDeviceType(mac, type) {
	var section = configuredType(mac);
	if (!type) {
		if (section) {
			uci.remove('uugamebooster', section['.name']);
		}
		return;
	}
	var id = section ? section['.name'] : uci.add('uugamebooster', 'device_type');
	uci.set('uugamebooster', id, 'mac', mac);
	uci.set('uugamebooster', id, 'type', type);
}

function updateStatus(table, data) {
	var online = data[0] && data[0].dhcp_leases;
	var accelerated = data[1] && data[1].addresses;
	var running = isRunning(data[2]);
	var service = document.getElementById('service_status');
	if (service) {
		service.innerHTML = renderStatus(running);
	}
	if (!table) {
		return;
	}

	Array.from(table.querySelectorAll('[data-device-mac]')).forEach(function(row) {
		var mac = row.getAttribute('data-device-mac');
		var device = Array.isArray(online) && online.find(function(item) { return item.macaddr && item.macaddr.toUpperCase() === mac; });
		var status = row.querySelector('.device-status');
		if (!Array.isArray(online)) {
			status.textContent = _('Unknown');
		} else if (!device) {
			status.textContent = _('Offline');
		} else if (!running) {
			status.textContent = _('Not accelerating');
		} else if (!Array.isArray(accelerated)) {
			status.textContent = _('Unknown');
		} else {
			status.textContent = accelerated.includes(device.ipaddr) ? _('Accelerating') : _('Not accelerating');
		}
	});
}

return view.extend({
	load: function() {
		return Promise.all([ uci.load('uugamebooster'), readStatus() ]);
	},

	render: function(data) {
		var m, s, o;
		var hosts = {};
		var leases = data[1][0] && data[1][0].dhcp_leases;
		if (Array.isArray(leases)) {
			leases.forEach(function(item) {
				if (!item.macaddr || !item.ipaddr) {
					return;
				}
				var mac = item.macaddr.toUpperCase();
				hosts[mac] = { name: item.hostname || '', ip: item.ipaddr };
			});
		}
		uci.sections('uugamebooster', 'device_type').forEach(function(section) {
			if (section.mac) {
				var mac = section.mac.toUpperCase();
				hosts[mac] = hosts[mac] || { name: '', ip: '' };
			}
		});

		m = new form.Map('uugamebooster', _('UU Game Accelerator'), _('A Paid Game Acceleration service'));
		s = m.section(form.TypedSection);
		s.anonymous = true;
		s.render = function() {
			return E('div', { class: 'cbi-section' }, E('p', { id: 'service_status' }, _('Collecting data...')));
		};
		s = m.section(form.NamedSection, 'config', 'uugamebooster');
		o = s.option(form.Flag, 'enabled', _('Enable'));
		o.default = o.disabled;
		o.rmempty = false;

		s = m.section(form.TypedSection);
		s.anonymous = true;
		s.render = function() {
			var rows = [ E('tr', { class: 'tr table-titles' }, [
				E('th', { class: 'th' }, _('Device')),
				E('th', { class: 'th' }, _('Device type')),
				E('th', { class: 'th' }, _('Acceleration'))
			]) ];
			Object.keys(hosts).sort().forEach(function(mac) {
				var host = hosts[mac];
				var selected = configuredType(mac);
				var options = [ E('option', { value: '', selected: !selected ? '' : null }, _('(Default)')) ];
				Object.keys(deviceTypes).forEach(function(type) {
					options.push(E('option', { value: type, selected: selected && selected.type === type ? '' : null }, deviceTypes[type]));
				});
				var select = E('select', { class: 'cbi-input-select', change: function(ev) {
					setDeviceType(mac, ev.target.value);
				} }, options);
				rows.push(E('tr', { class: 'tr', 'data-device-mac': mac }, [
					E('td', { class: 'td', 'data-title': _('Device') }, [ host.name || mac, E('br'), host.ip || '-' ]),
					E('td', { class: 'td', 'data-title': _('Device type') }, select),
					E('td', { class: 'td device-status', 'data-title': _('Acceleration') }, _('Collecting data...'))
				]));
			});
			if (rows.length === 1) {
				rows.push(E('tr', { class: 'tr placeholder' }, E('td', { class: 'td', colspan: 3 }, _('No devices.'))));
			}
			return E('div', { class: 'cbi-section' }, [ E('h3', _('Devices')),
				E('table', { class: 'table', id: 'uu_devices' }, rows) ]);
		};
		return m.render().then(function(map) {
			var table = map.querySelector('#uu_devices');
			updateStatus(table, data[1]);
			poll.add(function() { return readStatus().then(function(status) { updateStatus(table, status); }); });
			return map;
		});
	}
});
