![img](http://zfsonlinux.org/images/zfs-linux.png)

ZFS on Linux is an advanced file system and volume manager which was originally
developed for Solaris and is now maintained by the OpenZFS community.

[![codecov](https://codecov.io/gh/zfsonlinux/zfs/branch/master/graph/badge.svg)](https://codecov.io/gh/zfsonlinux/zfs)

# Official Resources
  * [Site](http://zfsonlinux.org)
  * [Wiki](https://github.com/zfsonlinux/zfs/wiki)
  * [Mailing lists](https://github.com/zfsonlinux/zfs/wiki/Mailing-Lists)
  * [OpenZFS site](http://open-zfs.org/)

# Installation
Full documentation for installing ZoL on your favorite Linux distribution can
be found at [our site](http://zfsonlinux.org/).

# Contribute & Develop
We have a separate document with [contribution guidelines](./.github/CONTRIBUTING.md).

# Changes by Turbo
This based on ZoL/zfs-0.7-release as of a8c2b7ebc64d23ffe15e2b168aa0bea7a4897133
(Fri Feb 22 09:47:55 2019 -0800- "Tag zfs-0.7.13") with the following merges
(in this order):

* turbo/sharenfs_rewrite
  https://github.com/zfsonlinux/zfs/pull/2790
  Rewrite of nfs.c to keep options per host separated.

* turbo/iscsi
  https://github.com/zfsonlinux/zfs/pull/1099
  iSCSI sharing for ZoL (shareiscsi).

* turbo/smbfs_registry-shares
  https://github.com/zfsonlinux/zfs/pull/1476
  Rewrite of the Libshare/SMBFS sharing property (sharesmb)

* turbo/no_cachefile
  https://github.com/zfsonlinux/zfs/pull/3526
  Change default cachefile property to 'none'
