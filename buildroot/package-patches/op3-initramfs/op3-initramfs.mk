################################################################################
#
# op3-initramfs
#
################################################################################

OP3_INITRAMFS_VERSION = 1
OP3_INITRAMFS_SITE = $(TOPDIR)/package/op3-initramfs/source
OP3_INITRAMFS_SITE_METHOD = local

define OP3_INITRAMFS_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -static -Os -Wall -Wextra \
		-Werror -o $(@D)/feed_entropy $(@D)/feed_entropy.c
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -static -Os -Wall -Wextra \
		-Werror -o $(@D)/nc $(@D)/netcat.c
endef

define OP3_INITRAMFS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/init $(TARGET_DIR)/init
	$(INSTALL) -D -m 0644 $(@D)/inittab $(TARGET_DIR)/etc/inittab
	$(INSTALL) -D -m 0755 $(@D)/init_mainline.sh \
		$(TARGET_DIR)/sbin/init_mainline.sh
	$(INSTALL) -D -m 0755 $(@D)/run_recovery.sh \
		$(TARGET_DIR)/sbin/run_recovery.sh
	$(INSTALL) -D -m 0755 $(@D)/init_audio_mainline.sh \
		$(TARGET_DIR)/usr/bin/init_audio_mainline.sh
	$(INSTALL) -D -m 0755 $(@D)/wifi_auto.sh \
		$(TARGET_DIR)/usr/bin/wifi_auto.sh
	$(INSTALL) -D -m 0755 $(@D)/route.sh \
		$(TARGET_DIR)/opt/op3-audio/route.sh
	$(INSTALL) -D -m 0755 $(@D)/feed_entropy \
		$(TARGET_DIR)/usr/bin/feed_entropy
	$(INSTALL) -D -m 0755 $(@D)/nc \
		$(TARGET_DIR)/usr/bin/nc
endef

$(eval $(generic-package))
