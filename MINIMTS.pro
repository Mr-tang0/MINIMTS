TEMPLATE = subdirs

SUBDIRS += \
    MGMotorLogic \
    MainUI \
    MiniMTSLogic \
    components \
    qtTools \
    tool

MainUI.depends = MiniMTSLogic
