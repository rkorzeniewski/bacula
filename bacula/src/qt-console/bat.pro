######################################################################

CONFIG += qt debug

TEMPLATE = app
TARGET = bat
DEPENDPATH += .
INCLUDEPATH += . ./console ./restore
INCLUDEPATH += ..
LIBS        += -L../lib
LIBS        += -lbac
LIBS        += -lssl -lcrypto
RESOURCES = main.qrc
MOC_DIR = moc
OBJECTS_DIR = obj

# Main window
FORMS += main.ui
FORMS += label/label.ui
FORMS += console/console.ui
FORMS += restore/restore.ui restore/prerestore.ui restore/brestore.ui
FORMS += run/run.ui


HEADERS += mainwin.h bat.h bat_conf.h qstd.h
SOURCES += main.cpp bat_conf.cpp mainwin.cpp qstd.cpp

# Console
HEADERS += console/console.h
SOURCES += console/authenticate.cpp console/console.cpp

# Restore
HEADERS += restore/restore.h
SOURCES += restore/restore.cpp restore/brestore.cpp

# Label dialog
HEADERS += label/label.h
SOURCES += label/label.cpp

# Run dialog
HEADERS += run/run.h
SOURCES += run/run.cpp
