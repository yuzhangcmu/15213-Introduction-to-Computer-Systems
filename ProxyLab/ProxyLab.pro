TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -pthread

SOURCES += \
    csapp.c \
    proxy.c \
    cache.c \
    Test.c

HEADERS += \
    csapp.h \
    cache.h

OTHER_FILES += \
    proxy.log

