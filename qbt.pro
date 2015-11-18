QT       += core network
QT       -= gui

TARGET = qbt
TEMPLATE = lib
CONFIG += staticlib

DEFINES += QT_DEPRECATED_WARNINGS

DISTFILES += \
    README.md \
    LICENSE

HEADERS += \
    bencodeparser.h \
    exception.h \
    filecache.h \
    metainfo.h \
    peer.h \
    protocol.h \
    server.h \
    torrent.h \
    torrentloop.h \
    tracker.h \
    verificator.h

SOURCES += \
    bencodeparser.cpp \
    exception.cpp \
    filecache.cpp \
    metainfo.cpp \
    peer.cpp \
    server.cpp \
    torrent.cpp \
    torrentloop.cpp \
    tracker.cpp \
    verificator.cpp


