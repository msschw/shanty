// SH-605 Phase B: SettingsFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsFacade.h"

#include <QFile>
#include <QIODevice>
#include <QStringList>

#include "PickerSupport.h"

#include <vector>

namespace shanty {

SettingsFacade::SettingsFacade(ISettingsStore& store, IEffectChain& chain,
                               IAutoEqReader& reader, QObject* parent)
    : QObject(parent),
      store_(store),
      chain_(chain),
      reader_(reader),
      profileService_(store) {
    rebuildViewModel();
}

void SettingsFacade::createProfile(const QString& name, int bandCount) {
    viewModel_->createProfile(name.toStdString(), bandCount);
}

void SettingsFacade::renameProfile(const QString& oldName,
                                   const QString& newName) {
    viewModel_->renameProfile(oldName.toStdString(), newName.toStdString());
}

void SettingsFacade::deleteProfile(const QString& name) {
    viewModel_->deleteProfile(name.toStdString());
}

void SettingsFacade::selectProfile(const QString& name) {
    viewModel_->selectProfile(name.toStdString());
}

void SettingsFacade::applyCurrentProfile() {
    // The view model fires no hook on the success path; the mirrored
    // onRejected is synchronous, so a clean flag distinguishes success.
    rejected_ = false;
    viewModel_->applyCurrentProfile();
    if (!rejected_) {
        emit applied();
    }
}

void SettingsFacade::resetEqualizer() {
    // reset() has no rejection path (bypass + flat gains sized by the view
    // model itself); publish the completion for the shell's toast.
    viewModel_->resetEqualizer();
    emit resetDone();
}

void SettingsFacade::setBandGain(int band, double gainDb) {
    const EqualizerProfile* current = findCurrentProfile();
    if (current == nullptr) {
        emit rejected(QStringLiteral("band: no profile selected"));
        return;
    }
    if (band < 0 ||
        band >= static_cast<int>(current->gains().size())) {
        emit rejected(QStringLiteral("band: invalid band index"));
        return;
    }

    std::vector<double> gains = current->gains();
    double clamped = gainDb;
    if (clamped < -EqualizerProfile::kMaxAbsGainDb) {
        clamped = -EqualizerProfile::kMaxAbsGainDb;
    } else if (clamped > EqualizerProfile::kMaxAbsGainDb) {
        clamped = EqualizerProfile::kMaxAbsGainDb;
    }
    gains[static_cast<std::size_t>(band)] = clamped;

    // SH-501 overwrite-by-name semantics, same composite the view model
    // uses for rename (SH-D605-B1).
    const EqualizerProfile updated =
        EqualizerProfile::make(current->name(), gains);
    if (!profileService_.saveProfile(updated)) {
        emit rejected(QStringLiteral("band: rejected by store"));
        return;
    }
    rebuildViewModel();  // publishes profilesChanged (decision-6 reload)
}

void SettingsFacade::importAutoEqProfile(const QString& name,
                                         const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        // Application-layer case before the view model's reader contract
        // applies: surface it through the same structured-error channel.
        QStringList messages;
        messages.append(filePath + QStringLiteral(
                           ": cannot open for reading"));
        emit importRejected(messages);
        return;
    }
    const QByteArray data = file.readAll();
    viewModel_->importAutoEqProfile(name.toStdString(),
                                    std::string(data.constData(),
                                                static_cast<std::size_t>(
                                                    data.size())),
                                    filePath.toStdString());
}

QString SettingsFacade::homeDir() const { return pickers::homeDir(); }

QVariantList SettingsFacade::listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
    const {
    return pickers::listDirectory(path, nameFilters);
}

QVariantList SettingsFacade::profiles() const {
    QVariantList list;
    const std::vector<EqualizerProfile>& profiles = viewModel_->profiles();
    list.reserve(static_cast<int>(profiles.size()));
    for (const EqualizerProfile& profile : profiles) {
        QVariantMap item;
        item.insert(QStringLiteral("name"),
                    QString::fromStdString(profile.name()));
        QVariantList gains;
        gains.reserve(static_cast<int>(profile.gains().size()));
        for (double gain : profile.gains()) {
            gains.append(gain);
        }
        item.insert(QStringLiteral("gains"), gains);
        item.insert(QStringLiteral("bandCount"),
                    static_cast<int>(profile.gains().size()));
        item.insert(QStringLiteral("isCurrent"),
                    profile.name() == viewModel_->currentProfileName());
        item.insert(QStringLiteral("source"),
                    profile.source() == ProfileSource::Imported
                        ? QStringLiteral("imported")
                        : QStringLiteral("manual"));
        item.insert(QStringLiteral("isValid"), profile.isValid());
        list.append(item);
    }
    return list;
}

QString SettingsFacade::currentProfileName() const {
    return QString::fromStdString(viewModel_->currentProfileName());
}

void SettingsFacade::rebuildViewModel() {
    // SH-D605-B1: re-construction IS the reload (SH-605 decision 6). The
    // constructor re-reads the profiles and the persisted selection and
    // fires no hooks, so publish after re-attaching the mirrors.
    viewModel_.reset(new SettingsViewModel(store_, chain_, reader_));
    connectViewModelCallbacks();
    emit profilesChanged();
}

void SettingsFacade::connectViewModelCallbacks() {
    viewModel_->onProfilesChanged = [this] { emit profilesChanged(); };
    viewModel_->onSelectionChanged = [this] { emit profilesChanged(); };
    viewModel_->onRejected = [this](const char* reason) {
        rejected_ = true;
        emit rejected(QString::fromUtf8(reason));
    };
    viewModel_->onImported = [this](const std::string& name) {
        emit imported(QString::fromStdString(name));
    };
    viewModel_->onImportRejected = [this](
                                       const std::vector<AutoEqError>&
                                           errors) {
        QStringList messages;
        for (const AutoEqError& error : errors) {
            QString message = QString::fromStdString(error.message);
            if (error.line > 0) {
                message += QStringLiteral(" (line %1)").arg(error.line);
            }
            messages.append(message);
        }
        emit importRejected(messages);
    };
}

const EqualizerProfile* SettingsFacade::findCurrentProfile() const {
    const std::string& currentName = viewModel_->currentProfileName();
    if (currentName.empty()) {
        return nullptr;
    }
    for (const EqualizerProfile& profile : viewModel_->profiles()) {
        if (profile.name() == currentName) {
            return &profile;
        }
    }
    return nullptr;
}

}  // namespace shanty
