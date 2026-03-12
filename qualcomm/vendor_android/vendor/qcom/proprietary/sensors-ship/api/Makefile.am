ACLOCAL_AMFLAGS = -I m4

dirs = proto \
          rpc/direct_channel_stub   \
          rpc/device_mode_stub \
          rpc/dynamic_loader_stub \
          rpc/remote_proc_state_stub\
          rpc/registry_skel

SUBDIRS = $(dirs)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensors-api.pc

EXTRA_DIST = $(pkgconfig_DATA)
