SUMMARY = "fish - the friendly interactive shell"
DESCRIPTION = "A smart and user-friendly command line shell for Linux, macOS, \
and the rest of the family."
HOMEPAGE = "https://fishshell.com"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=62bf11ac21699b630f7c619c67090973"

# 3.7.1 is the last C++/CMake release; fish 4.x is a Rust rewrite (heavier
# crate-vendoring recipe). Bump to a 4.x cargo-based recipe if/when wanted.
SRC_URI = "https://github.com/fish-shell/fish-shell/releases/download/${PV}/fish-${PV}.tar.xz"
SRC_URI[sha256sum] = "614c9f5643cd0799df391395fa6bbc3649427bb839722ce3b114d3bbc1a3b250"

DEPENDS = "ncurses pcre2 gettext-native"

inherit cmake pkgconfig

EXTRA_OECMAKE = " \
    -DBUILD_DOCS=OFF \
    -DFISH_USE_SYSTEM_PCRE2=ON \
"

RDEPENDS:${PN} = "ncurses-terminfo-base"

FILES:${PN} += "${datadir}/fish"

# Register fish as a valid login shell so login/chsh accept it (ssh key login
# works without this, but console/su paths check /etc/shells).
pkg_postinst:${PN} () {
    if [ -z "$D" ]; then
        grep -qx "${bindir}/fish" "${sysconfdir}/shells" 2>/dev/null || \
            echo "${bindir}/fish" >> "${sysconfdir}/shells"
    fi
}
