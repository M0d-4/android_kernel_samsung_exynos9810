### AnyKernel3 Ramdisk Mod Script
## Galaxy S9+ (star2lte / SM-G965x) - Exynos 9810
## Based on the AnyKernel3 template by osm0sis @ xda-developers
##
## The core AnyKernel3 files (tools/, META-INF/) are pulled from upstream by
## .github/workflows/build-star2lte.yml; only this script is kept in the repo.

### AnyKernel setup
# global properties
properties() { '
kernel.string=star2lte kernel
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=star2lte
device.name2=star2ltexx
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties


### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
# Galaxy S9/S9+ boot partition (UFS)
BLOCK=/dev/block/platform/11120000.ufs/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# boot install
# Only the kernel Image is replaced. The ramdisk is left untouched, so skip
# unpack/repack of it: split_boot -> flash_boot swaps in the new Image and keeps
# the device's existing ramdisk, device tree, cmdline, offsets and the
# SEANDROIDENFORCE trailer.
split_boot;
flash_boot;
## end boot install
