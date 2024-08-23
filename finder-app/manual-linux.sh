#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# Number of CPUs available for parallel jobs
NPROC=$(nproc)

if [ $# -lt 1 ]
then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    # Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # Kernel build steps
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

mkdir -p ${OUTDIR}/rootfs/{bin,sbin,etc,proc,sys,usr/{bin,sbin},lib,lib64,home,dev}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean || true
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} menuconfig || {
        echo "BusyBox configuration failed."
        exit 1
    }
    
    # Enable bash in BusyBox configuration
    # In the menuconfig interface, navigate to "Shells --->"
    # Enable "bash-like shell" or "ash" by pressing "Y"
    
else
    cd busybox
fi

# Make and install busybox
make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

echo "Library dependencies"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# Add library dependencies to rootfs
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

# Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# Ensure the conf directory and its files exist before copying
if [ -f ${FINDER_APP_DIR}/conf/username.txt ] && [ -f ${FINDER_APP_DIR}/conf/assignment.txt ]; then
    echo "Copying the finder related scripts and executables to the /home directory on the target rootfs"
    # Create the conf directory in rootfs/home
    mkdir -p ${OUTDIR}/rootfs/home/conf

    # Copy the finder scripts and the conf files
    cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
    cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
    cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf/
    cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf/
    cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
    cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
else
    echo "Required configuration files are missing in ${FINDER_APP_DIR}/conf. Exiting."
    exit 1
fi

# Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov > ${OUTDIR}/initramfs.cpio || {
    echo "Error creating initramfs.cpio"
    exit 1
}
cd ${OUTDIR}
gzip -f initramfs.cpio || {
    echo "Error compressing initramfs.cpio"
    exit 1
}

echo "Kernel build, root filesystem, and initramfs creation completed successfully."
