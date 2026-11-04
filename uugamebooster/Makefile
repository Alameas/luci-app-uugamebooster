# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2021 ImmortalWrt.org

include $(TOPDIR)/rules.mk

PKG_NAME:=uugamebooster
PKG_VERSION:=14.11.2
PKG_RELEASE:=2

PKG_SOURCE=$(PKG_NAME)-$(PKG_VERSION)-$(UU_TYPE).tar.gz
PKG_SOURCE_URL=https://uurouter.gdl.netease.com/uuplugin/$(UU_TYPE)/v$(PKG_VERSION)/uu.tar.gz?

ifeq ($(ARCH),aarch64)
  UU_TYPE:=h3c-nx30pro
  PKG_HASH:=cadd042abb5f3bfdd0e487a7fa61a1b18a2c2cb6d6d14c1e8e3c915bb81cee86
else ifeq ($(ARCH),arm)
  UU_TYPE:=h3c-be18000
  PKG_HASH:=5fa71e75152886d2a1ec14ca93d14ab79cbf674c5adfbf000f5495929446b76d
else ifeq ($(ARCH),mipsel)
  UU_TYPE:=jd-hr06
  PKG_HASH:=902fa79d0477e59fcb1f3cc8daf691a390f0fc12f7acfd2f6e0b884e1a675115
else ifeq ($(ARCH),x86_64)
  UU_TYPE:=openwrt-x86_64
  PKG_HASH:=b770bdd5b78c900aec64c053cef31323d4943289eaf291acca22d990d3fa3680
endif

PKG_LICENSE:=Proprietary

include $(INCLUDE_DIR)/package.mk

STRIP:=true

TAR_CMD=$(HOST_TAR) -C $(1)/ $(TAR_OPTIONS)

define Package/uugamebooster
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=@(aarch64||arm||mipsel||x86_64) +kmod-tun +iptables-nft +kmod-ipt-nat
  DEPENDS+=$(if $(filter openwrt-%,$(UU_TYPE)),,+libstdcpp +libgcc)
  TITLE:=NetEase UU Game Booster
  URL:=https://uu.163.com
endef

define Package/uugamebooster/description
  NetEase's UU Game Booster Accelerates Triple-A Gameplay and Market.
endef

define Build/Compile
endef

define Package/uugamebooster/conffiles
/.uuplugin_uuid
/usr/share/uugamebooster/uu.conf
endef

define Package/uugamebooster/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uuplugin $(1)/usr/bin/uugamebooster
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xuplugin-guardian $(1)/usr/bin/xuplugin-guardian
	$(if $(filter jd-%,$(UU_TYPE)),$(INSTALL_BIN) $(CURDIR)/files/haiapi $(1)/usr/bin/haiapi)
	$(if $(filter h3c-%,$(UU_TYPE)),$(INSTALL_BIN) $(CURDIR)/files/h3c-info $(1)/usr/bin/h3c-info)

	$(INSTALL_DIR) $(1)/usr/share/uugamebooster
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/uu.conf $(1)/usr/share/uugamebooster/uu.conf

	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d
	$(INSTALL_CONF) $(CURDIR)/files/uugamebooster.config $(1)/etc/config/uugamebooster
	$(INSTALL_BIN) $(CURDIR)/files/uugamebooster.init $(1)/etc/init.d/uugamebooster
endef

$(eval $(call BuildPackage,uugamebooster))
