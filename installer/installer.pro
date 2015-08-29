QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = installer
TEMPLATE = app

unix:QMAKE_CXXFLAGS += -std=c++11
unix:LIBS += -lz

SOURCES += main.cpp\
        mainwindow.cpp \
    handle.cpp \
    cab.cpp

HEADERS  += mainwindow.h \
    handle.h \
    cab.h

FORMS    += mainwindow.ui
