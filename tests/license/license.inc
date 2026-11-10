# SH-102: license suite scans the repository; root injected at build time.
SHANTY_ROOT = $$clean_path($$PWD/../..)
DEFINES += SHANTY_ROOT=\\\"$$SHANTY_ROOT\\\"
