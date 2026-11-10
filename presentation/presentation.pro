# SH-E6: presentation layer. View models are pure C++ with callbacks
# (consistent with core/playback/PlaybackController, SH-404): fully
# host-testable, the QML layer (app/, SDK-only) wraps/exposes them later.
TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS += viewmodels
