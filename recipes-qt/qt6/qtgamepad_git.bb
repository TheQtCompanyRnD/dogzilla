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

# qtdeclarative-native supplies Qt6QuickTools (host qmltyperegistrar/qmlcachegen
# + QuickTools cmake). Without it, find_package(Qt6 ... OPTIONAL_COMPONENTS Quick)
# fails quietly at configure -> the quick* subdirs (the QtGamepad and
# QtUniversalInput QML modules) are skipped and qtgamepad-qmlplugins ships empty.
DEPENDS += "qtbase qtdeclarative qtdeclarative-native"

# qtgamepad lives in its own repo and has no dedicated SRCREV in
# meta-qt6/qt6-git.inc, so pin it here. Upstream stopped branching after 6.3;
# "dev" is the only branch that still tracks recent Qt, so build against it
# (pinned to its current head for reproducibility). Bump the SRCREV if it
# fails to build against the qtbase you are using.
#
# qt6-git.inc builds the URL from meta-qt6's QT_GIT/QT_GIT_PROJECT, which makes
# this git://code.qt.io/qt/qtgamepad
QT_MODULE = "qtgamepad"
QT_MODULE_BRANCH = "dev"
SRCREV = "e3aa67410f2cded7445e540ea8eccc39114ac7be"

# qtgamepad's CMakeLists.txt does find_package(Qt6 6.13.0 REQUIRED) (the "dev"
# branch tracks the next Qt version), but we build it against Qt 6.12. The bare
# QT_NO_PACKAGE_VERSION_CHECK bitbake var does nothing -- nothing reads it and
# passes it to cmake. It has to reach CMake as a -D argument, exactly like the
# local -DQT_NO_PACKAGE_VERSION_CHECK=TRUE that makes the standalone build pass.
EXTRA_OECMAKE += "-DQT_NO_PACKAGE_VERSION_CHECK=TRUE"

# The native Linux backend is evdev (built by default); SDL2 is optional and
# off by default for a headless robot.
PACKAGECONFIG ??= ""
PACKAGECONFIG[sdl2] = "-DFEATURE_sdl2=ON,-DFEATURE_sdl2=OFF,libsdl2"

BBCLASSEXTEND = "native nativesdk"
