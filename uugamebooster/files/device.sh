#!/bin/sh

. /lib/functions.sh
. /lib/functions/system.sh

uu_device_mac() {
	local mac

	mac="$(uci -q get uugamebooster.config.mac)"
	if [ -z "$mac" ]; then
		mac="$(get_mac_label)"
		if [ -z "$mac" ]; then
			mac="$(macaddr_random)"
		fi

		[ -n "$mac" ] || return 1
		uci -q set "uugamebooster.config.mac=$mac" && uci -q commit uugamebooster || return 1
	fi

	printf '%s\n' "$mac"
}

uu_device_model() {
	local model

	model="$(ubus call system board 2>/dev/null | jsonfilter -e '@.model' 2>/dev/null)"
	[ -n "$model" ] || model="$(cat /tmp/sysinfo/model 2>/dev/null)"
	[ -n "$model" ] || model=OpenWrt

	printf '%s\n' "$model"
}
