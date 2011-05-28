# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/dahdi/dahdi-2.4.1.ebuild,v 1.3 2011/04/28 21:37:53 tomka Exp $

EAPI=3

inherit base linux-mod eutils

DESCRIPTION="A Linux kernel driver to handle the lowlevel Atari SIO protocol."
HOMEPAGE="http://www.horus.com/~hias/atari/"
SRC_URI="http://www.horus.com/~hias/atari/atarisio/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86"

RESTRICT="test"

DEPEND=""
RDEPEND=""

src_prepare() {
	sed -i -e "s/#ALL_IN_ONE=1/ALL_IN_ONE=1/" Makefile
	sed -i -e "s/#ENABLE_ATP=1/ENABLE_ATP=1/" Makefile
	base_src_prepare
}

src_compile() {
	unset ARCH
	emake KSRC="${KERNEL_DIR}" DESTDIR="${D}" all || die "Compilation failed"
}

src_install() {
	insinto "/etc/udev/rules.d"
	doins atarisio-udev.rules

	insinto "/etc/modprobe.d"
	doins atarisio-modprobe.conf

	insinto "/lib/modules/${KV}/misc"
	doins driver/atarisio.ko

	insinto "/usr/games/bin"
	insopts -m0750
	doins tools/atarisio
	fowners root:games /usr/games/bin/atarisio

	dodoc README README-tools Changelog

	echo
	einfo "Updating module informations"
	update-modules -v

	echo
	elog "A new kernel module (atarisio.ko) has been installed."
	elog "Before loading it you may have to edit"
	elog "/etc/modprobe.d/atarisio-modprobe.conf"
	elog "and do a 'update-modules' to fit it to your needs."
}
