#-------------------------------------------------
#
# Project created by QtCreator 2018-02-23T21:02:25
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Wichat-qt
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
    loginwindow.cpp \
    serverconnection.cpp \
    encryptor.cpp \
    wichatconfig.cpp \
    usersession.cpp \
    peersession.cpp \
    abstractsession.cpp

HEADERS  += mainwindow.h \
    loginwindow.h \
    serverconnection.h \
    serverconnection_p.h \
    common.h \
    encryptor.h \
    encryptorprivate.h \
    wichatconfig.h \
    wichatconfigprivate.h \
    usersession.h \
    peersession.h \
    peersessionprivate.h \
    usersessionprivate.h \
    abstractsessionprivate.h \
    abstractsession.h

FORMS    += mainwindow.ui \
    loginwindow.ui

RESOURCES += \
    icon.qrc

DISTFILES +=