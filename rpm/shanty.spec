# SPDX-License-Identifier: GPL-3.0-or-later
# SH-702 Phase B: finalised Sailfish OS packaging for Shanty. Summary,
# description, runtime Requires and %changelog aligned with PROJECT.md
# (lean local audio player); Name/Version kept as shipped by the
# template. The spec is built in the Sailfish SDK (mb2/sfdk), not on the
# host — see docs/app-size-audit.md for the accounting procedure.

Name:       shanty

Summary:    Lean local audio player (MP3, OGG, FLAC) for Sailfish OS
Version:    0.1
Release:    1
License:    GPL-3.0-or-later
URL:        http://example.org/
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
# SH-901 (SH-E9): the device engine is GStreamer (playbin +
# audio-filter=equalizer-10bands, docs/eq-investigation.md). Explicit
# runtime contract for the equalizer element's owner; gst core/base
# runtime ships with the base image (and libgstreamer's own soname
# dependency is automatic).
Requires:   gstreamer1.0-plugins-good
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
# SH-901: gst core headers/libs — the whole compile surface of the
# GStreamer engine adapter (elements are created by factory name at
# runtime; no plugins-base API is called directly).
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  desktop-file-utils

%description
Shanty is a lean local audio player for Sailfish OS. It plays MP3, OGG
(Vorbis) and FLAC files stored on the device — no streaming, no network
features. Library scopes are user-managed: folders can be added and
removed, and individual folders or files whitelisted (included) or
blacklisted (excluded). M3U/M3U8 playlists can be imported, edited
(reorder, add, remove entries) and exported back to disk.

The UI provides the three dedicated views of the charter: Library,
Settings (equalizer profiles with AutoEq import) and Player (transport
controls for the current playlist with play/pause, next and previous).

Deliberately minimal dependencies (Qt core/QML/Quick, sailfishapp,
Sailfish Silica) to keep the footprint small. Free software under
GPL-3.0-or-later; see the LICENSE file shipped with the sources.


%prep
%setup -q -n %{name}-%{version}

%build

%qmake5

%make_build


%install
%qmake5_install


desktop-file-install --delete-original         --dir %{buildroot}%{_datadir}/applications                %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

# SH-702: ship the GPL licence text (package documentation).
%doc LICENSE


%changelog
* Tue Nov 10 2026 Shanty contributors <shanty@localhost> 0.1-1
- Initial packaging of Shanty 0.1: lean local audio player with
  user-managed library (whitelist/blacklist), M3U import/edit/export
  and equalizer (AutoEq profile import) for Sailfish OS.
- Licensed under GPL-3.0-or-later (see LICENSE).
