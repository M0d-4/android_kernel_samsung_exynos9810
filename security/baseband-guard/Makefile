# Baseband-guard (https://github.com/vc-teahouse/Baseband-guard @ a54e0dc6)
# Vendored for this 4.9 tree: no git/network access at build time, no
# DEFINE_LSM/CONFIG_LSM (pre-5.1 LSM model: hooks are registered from
# security_initcall()).
#
# security/selinux/ is descended into before this directory (see
# security/Makefile), so flask.h/av_permissions.h already exist in
# $(objtree)/security/selinux when we compile.

bbg-objs += baseband_guard.o
bbg-objs += tracing/tracing.o
bbg-objs += blkdev_helper.o

ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include -I$(srctree)/block
ccflags-y += -I$(objtree)/security/selinux -include $(srctree)/include/uapi/asm-generic/errno.h

obj-$(CONFIG_BBG) += bbg.o

ccflags-y += -DBBG_VERSION=\"a54e0dc6\"
ccflags-y += -DBBG_REPO=\"https://github.com/vc-teahouse/Baseband-guard\"
