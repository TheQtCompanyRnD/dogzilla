#!/bin/sh
# Expand the root partition to fill the whole storage device, then grow the
# ext4 filesystem online. Runs once on first boot (guarded by a stamp file).
set -e

STAMP=/var/lib/rpi-resize-rootfs.done
[ -e "$STAMP" ] && exit 0

root_part="$(findmnt -n -o SOURCE /)"
if [ -z "$root_part" ] || [ ! -b "$root_part" ]; then
    echo "resize-rootfs: could not determine root block device (got '$root_part')" >&2
    exit 1
fi

# Split the root partition device into its disk + partition number, handling
# both mmcblk0p2 / nvme0n1p2 style (pN suffix) and sda2 style names.
case "$root_part" in
    *[0-9]p[0-9]*)
        disk="${root_part%p[0-9]*}"
        partnum="${root_part##*p}"
        ;;
    *)
        disk="$(printf '%s' "$root_part" | sed -E 's/[0-9]+$//')"
        partnum="$(printf '%s' "$root_part" | sed -E 's/.*[^0-9]//')"
        ;;
esac

echo "resize-rootfs: growing ${disk} partition ${partnum} and filesystem on ${root_part}"

# Grow the partition to the end of the disk. parted informs the kernel of the
# new size via BLKPG; partprobe is a belt-and-suspenders re-read.
parted -s "$disk" resizepart "$partnum" 100%
partprobe "$disk" 2>/dev/null || true

# Online-grow the (mounted) root filesystem to the new partition size.
resize2fs "$root_part"

mkdir -p "$(dirname "$STAMP")"
touch "$STAMP"
echo "resize-rootfs: done"
