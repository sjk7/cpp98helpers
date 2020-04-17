TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++98

SOURCES += $$PWD/../../SPSCAudio.cpp

unix|win32: LIBS += -lwinmm

INCLUDEPATH += $$PWD/../../
DEPENDPATH += $$PWD/../../

HEADERS += \
    ../../my_concurrent.h \
    ../../my_spsc_buffer.h
