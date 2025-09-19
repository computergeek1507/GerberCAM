#-------------------------------------------------
#
# Project created by QtCreator 2016-01-30T18:52:57
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = GerberCAM1
TEMPLATE = app


SOURCES += main.cpp\
    source/aboutwindow.cpp \
    source/calculation.cpp \
    source/clipper.cpp \
    source/drawPCB.cpp \
    source/gerber.cpp \
    source/mainwindow.cpp \
    source/preprocess.cpp \
    source/setting.cpp \
    source/settingwindow.cpp \
    source/toolpath.cpp \
    source/treeitem.cpp \
    source/treemodel.cpp \


HEADERS  +=source/aboutwindow.h \
    source/calculation.h \
    source/drawPCB.h \
    source/gerber.h \
    source/mainwindow.h \
    source/preprocess.h \
    source/Read me.h \
    source/setting.h \
    source/settingwindow.h \
    source/toolpath.h \
    source/treeitem.h \
    source/treemodel.h \
    source/clipper.hpp

FORMS    += mainwindow.ui \
    settingwindow.ui \
    aboutwindow.ui \
    aboutwindow.ui

RESOURCES += \
    resources.qrc

TOOL_FILE = "tool_library.con"

win32{
    QMAKE_POST_LINK +=$$quote(cmd /c copy /y $${TOOL_FILE} $${DESTDIR_WIN}$$escape_expand(\n\t))
}

unix{
    QMAKE_POST_LINK += $$quote(cp $${TOOL_FILE} $${DESTDIR}$$escape_expand(\n\t))
}

INCLUDEPATH += $$PWD/source
