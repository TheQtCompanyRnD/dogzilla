# qtgamepad is NOT part of meta-qt6 -- it was dropped as a standard Qt 6
# module. This recipe ports it against the Qt 6.x sources so it can be built
# alongside the rest of Qt 6.12. The module is unmaintained upstream, so the
# branch/SRCREV below may need adjusting (try "dev" if the version branch is
# absent), and it may need patches to build against current qtbase.

LICENSE = "The-Qt-Company-Commercial | GPL-3.0-only & (LGPL-3.0-only | GPL-2.0-only | GPL-3.0-only) & GFDL-1.3-no-invariants-only & BSD-3-Clause"
LIC_FILES_CHKSUM = " \
    file://LICENSES/BSD-3-Clause.txt;md5=cb40fa7520502d8c7a3aea47cae1316c \
    file://LICENSES/GFDL-1.3-no-invariants-only.txt;md5=a22d0be1ce2284b67950a4d1673dd1b0 \
    file://LICENSES/GPL-2.0-only.txt;md5=b234ee4d69f5fce4486a80fdaf4a4263 \
    file://LICENSES/GPL-3.0-only.txt;md5=d32239bcb673463ab874e80d47fae504 \
    file://LICENSES/LGPL-3.0-only.txt;md5=e6a600fd5e1d9cbde2d983680233ad02 \
    file://LICENSES/LicenseRef-Qt-Commercial.txt;md5=40a1036f91cefc0e3fabad241fb5f187 \
"

inherit qt6-cmake

include recipes-qt/qt6/qt6-git.inc
include recipes-qt/qt6/qt6.inc

DEPENDS += "qtbase qtdeclarative"

# qtgamepad lives in its own repo and has no dedicated SRCREV in
# meta-qt6/qt6-git.inc, so pin it here. Upstream stopped branching after 6.3;
# "dev" is the only branch that still tracks recent Qt, so build against it
# (pinned to its current head for reproducibility). Bump the SRCREV if it
# fails to build against the qtbase you are using.
QT_MODULE = "qtgamepad"
QT_MODULE_BRANCH = "dev"
SRCREV = "6de4c9d2ad753eac40b9f83bebcc01084fae16a9"

# The native Linux backend is evdev (built by default); SDL2 is optional and
# off by default for a headless robot.
PACKAGECONFIG ??= ""
PACKAGECONFIG[sdl2] = "-DFEATURE_sdl2=ON,-DFEATURE_sdl2=OFF,libsdl2"

BBCLASSEXTEND = "native nativesdk"
