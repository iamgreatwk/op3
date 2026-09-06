################################################################################
#
# op3-recovery
#
################################################################################

OP3_RECOVERY_VERSION = 1
OP3_RECOVERY_SITE = $(TOPDIR)/package/op3-recovery/source
OP3_RECOVERY_SITE_METHOD = local

OP3_RECOVERY_SOURCES = \
	recovery/recovery_mainline.c \
	recovery/recovery_drm.c \
	libtsm/src/tsm/tsm-render.c \
	libtsm/src/tsm/tsm-screen.c \
	libtsm/src/tsm/tsm-selection.c \
	libtsm/src/tsm/tsm-unicode.c \
	libtsm/src/tsm/tsm-vte.c \
	libtsm/src/tsm/tsm-vte-charsets.c \
	libtsm/src/shared/shl-htable.c \
	libtsm/src/shared/shl-ring.c \
	libtsm/external/wcwidth/wcwidth.c

define OP3_RECOVERY_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -static -O2 -pipe \
		-Wno-unused-result \
		-I$(@D)/libtsm/src/tsm \
		-I$(@D)/libtsm/src/shared \
		-I$(@D)/libtsm/external \
		-o $(@D)/recovery_mainline \
		$(addprefix $(@D)/,$(OP3_RECOVERY_SOURCES))
endef

define OP3_RECOVERY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/recovery_mainline \
		$(TARGET_DIR)/sbin/recovery_mainline
	$(INSTALL) -D -m 0755 $(@D)/runners/browser-session.sh \
		$(TARGET_DIR)/usr/bin/op3-browser-session
	$(INSTALL) -D -m 0755 $(@D)/runners/browser-session.sh \
		$(TARGET_DIR)/usr/bin/browser
	$(INSTALL) -D -m 0755 $(@D)/runners/cog-run.sh \
		$(TARGET_DIR)/opt/op3-recovery/cog-run.sh
	$(INSTALL) -D -m 0755 $(@D)/runners/chromium-run.sh \
		$(TARGET_DIR)/opt/op3-recovery/chromium-run.sh
endef

$(eval $(generic-package))
