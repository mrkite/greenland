#-------------------------------------------------
#
# Project created by QtCreator 2015-08-29T12:41:58
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = inspector
TEMPLATE = app
unix:QMAKE_CXXFLAGS += -std=c++11
unix:LIBS += -lz

SOURCES += main.cpp\
        mainwindow.cpp \
    handle.cpp \
    bmp.cpp \
    view.cpp \
    bif.cpp \
    unk.cpp \
    bam.cpp \
    mos.cpp \
    mve.cpp

HEADERS  += mainwindow.h \
    handle.h \
    bmp.h \
    view.h \
    bif.h \
    unk.h \
    bam.h \
    mos.h \
    mve.h

FORMS    += mainwindow.ui
