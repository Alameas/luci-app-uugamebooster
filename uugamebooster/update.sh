#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2021 ImmortalWrt.org

set -x

CURDIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)" || exit 1
trap 'rm -rf "$TMP"' EXIT

URL="$(curl -fsSL 'https://router.uu.163.com/api/plugin?type=openwrt-x86_64' | jq -er '.url')" || exit 1
VERSION="$(echo "$URL" | sed -n 's|.*/v\([0-9][0-9.]*\)/uu\.tar\.gz.*|\1|p')"
[ -n "$VERSION" ] || exit 1

for ARCH in "aarch64" "arm" "mipsel" "x86_64"; do
	UU_TYPE="$(awk -v arch="$ARCH" '
		$0 ~ "^((else )?ifeq) \\(\\$\\(ARCH\\)," arch "\\)" { found = 1; next }
		found && /^ *(else|endif)/ { exit }
		found && /^ *UU_TYPE:=/ { sub(/^ *UU_TYPE:=/, ""); print; exit }
	' "$CURDIR/Makefile")"
	[ -n "$UU_TYPE" ] || exit 1
	curl -fsSL "https://uurouter.gdl.netease.com/uuplugin/$UU_TYPE/v$VERSION/uu.tar.gz?" -o "$TMP/uu.tar.gz" || exit 1
	HASH="$(openssl dgst -sha256 "$TMP/uu.tar.gz" | awk '{print $NF}')"
	[ -n "$HASH" ] || exit 1
	printf '%s %s\n' "$ARCH" "$HASH" >> "$TMP/hashes"
done

awk -v version="$VERSION" '
	NR == FNR { hash[$1] = $2; next }
	/^((else )?ifeq) \(\$\(ARCH\),/ {
		arch = $0
		sub(/^.*\(\$\(ARCH\),/, "", arch)
		sub(/\).*/, "", arch)
	}
	/^endif/ { arch = "" }
	!arch && /^PKG_VERSION:=/ { sub(/PKG_VERSION:=.*/, "PKG_VERSION:=" version) }
	arch in hash && /^ *PKG_HASH:=/ { sub(/PKG_HASH:=.*/, "PKG_HASH:=" hash[arch]) }
	{ print }
' "$TMP/hashes" "$CURDIR/Makefile" > "$TMP/Makefile" || exit 1
cat "$TMP/Makefile" > "$CURDIR/Makefile"
