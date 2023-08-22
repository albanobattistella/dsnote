/* Copyright (C) 2021-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "settings.h"

#ifdef USE_DESKTOP
#include <QQuickStyle>
#endif
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariant>
#include <QVariantList>
#include <algorithm>
#include <fstream>
#include <iostream>

#include "config.h"
#ifdef ARCH_X86_64
#include "opencl_tools.hpp"
#endif

QDebug operator<<(QDebug d, settings::mode_t mode) {
    switch (mode) {
        case settings::mode_t::Stt:
            d << "stt";
            break;
        case settings::mode_t::Tts:
            d << "tts";
            break;
    }

    return d;
}

QDebug operator<<(QDebug d, settings::launch_mode_t launch_mode) {
    switch (launch_mode) {
        case settings::launch_mode_t::service:
            d << "service";
            break;
        case settings::launch_mode_t::app_stanalone:
            d << "app-standalone";
            break;
        case settings::launch_mode_t::app:
            d << "app";
            break;
    }

    return d;
}

QDebug operator<<(QDebug d, settings::speech_mode_t speech_mode) {
    switch (speech_mode) {
        case settings::speech_mode_t::SpeechAutomatic:
            d << "automatic";
            break;
        case settings::speech_mode_t::SpeechManual:
            d << "manual";
            break;
        case settings::speech_mode_t::SpeechSingleSentence:
            d << "single-sentence";
            break;
    }

    return d;
}

settings::settings() : QSettings{settings_filepath(), QSettings::NativeFormat} {
    qDebug() << "app:" << APP_ORG << APP_ID;
    qDebug() << "config location:"
             << QStandardPaths::writableLocation(
                    QStandardPaths::ConfigLocation);
    qDebug() << "data location:"
             << QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation);
    qDebug() << "cache location:"
             << QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    qDebug() << "settings file:" << fileName();

    update_qt_style();
    update_gpu_devices();
}

QString settings::settings_filepath() {
    QDir conf_dir{
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)};
    conf_dir.mkpath(QCoreApplication::organizationName() + QDir::separator() +
                    QCoreApplication::applicationName());
    return conf_dir.absolutePath() + QDir::separator() +
           QCoreApplication::organizationName() + QDir::separator() +
           QCoreApplication::applicationName() + QDir::separator() +
           settings_filename;
}

QString settings::models_dir() const {
    auto dir = value(QStringLiteral("service/models_dir"),
                     value(QStringLiteral("lang_models_dir")))
                   .toString();
    if (dir.isEmpty()) {
#ifdef USE_SFOS
        dir = QDir{QStandardPaths::writableLocation(
                       QStandardPaths::DownloadLocation)}
                  .filePath(QStringLiteral("speech-models"));
#else
        dir = QDir{QStandardPaths::writableLocation(
                       QStandardPaths::CacheLocation)}
                  .filePath(QStringLiteral("speech-models"));
#endif
        QDir{}.mkpath(dir);
    }

    return dir;
}

QUrl settings::models_dir_url() const {
    return QUrl::fromLocalFile(models_dir());
}

void settings::set_models_dir(const QString& value) {
    if (models_dir() != value) {
        setValue(QStringLiteral("service/models_dir"), value);
        emit models_dir_changed();
    }
}

void settings::set_models_dir_url(const QUrl& value) {
    set_models_dir(value.toLocalFile());
}

QString settings::models_dir_name() const {
    return models_dir_url().fileName();
}

QString settings::cache_dir() const {
    auto dir = value(QStringLiteral("service/cache_dir"),
                     value(QStringLiteral("cache_dir")))
                   .toString();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir{}.mkpath(dir);
    }

    return dir;
}

QUrl settings::cache_dir_url() const {
    return QUrl::fromLocalFile(cache_dir());
}

void settings::set_cache_dir(const QString& value) {
    if (cache_dir() != value) {
        setValue(QStringLiteral("service/cache_dir"), value);
        emit cache_dir_changed();
    }
}

void settings::set_cache_dir_url(const QUrl& value) {
    set_cache_dir(value.toLocalFile());
}

QString settings::default_stt_model() const {
    return value(QStringLiteral("service/default_model"),
                 QStringLiteral("en"))
        .toString();  // english is a default;
}

void settings::set_default_stt_model(const QString& value) {
    if (default_stt_model() != value) {
        setValue(QStringLiteral("service/default_model"), value);
        emit default_stt_model_changed();
    }
}

QString settings::default_tts_model() const {
    return value(QStringLiteral("service/default_tts_model"),
                 QStringLiteral("en"))
        .toString();  // english is a default;
}

void settings::set_default_tts_model(const QString& value) {
    if (default_tts_model() != value) {
        setValue(QStringLiteral("service/default_tts_model"), value);
        emit default_tts_model_changed();
    }
}

QString settings::default_mnt_lang() const {
    return value(QStringLiteral("service/default_mnt_lang"),
                 QStringLiteral("en"))
        .toString();  // english is a default;
}

void settings::set_default_mnt_lang(const QString& value) {
    if (default_mnt_lang() != value) {
        setValue(QStringLiteral("service/default_mnt_lang"), value);
        emit default_mnt_lang_changed();
    }
}

QString settings::default_mnt_out_lang() const {
    return value(QStringLiteral("service/default_mnt_out_lang"),
                 QStringLiteral("en"))
        .toString();  // english is a default;
}

void settings::set_default_mnt_out_lang(const QString& value) {
    if (default_mnt_out_lang() != value) {
        setValue(QStringLiteral("service/default_mnt_out_lang"), value);
        emit default_mnt_out_lang_changed();
    }
}

QStringList settings::enabled_models() {
    return value(QStringLiteral("service/enabled_models"), {}).toStringList();
}

void settings::set_enabled_models(const QStringList& value) {
    if (enabled_models() != value) {
        setValue(QStringLiteral("service/enabled_models"), value);
    }
}

QString settings::default_stt_model_for_lang(const QString& lang) {
    return value(QStringLiteral("service/default_model_%1").arg(lang), {})
        .toString();
}

void settings::set_default_stt_model_for_lang(const QString& lang,
                                              const QString& value) {
    if (default_stt_model_for_lang(lang) != value) {
        setValue(QStringLiteral("service/default_model_%1").arg(lang), value);
        emit default_stt_models_changed(lang);
    }
}

QString settings::default_tts_model_for_lang(const QString& lang) {
    return value(QStringLiteral("service/default_tts_model_%1").arg(lang), {})
        .toString();
}

void settings::set_default_tts_model_for_lang(const QString& lang,
                                              const QString& value) {
    if (default_tts_model_for_lang(lang) != value) {
        setValue(QStringLiteral("service/default_tts_model_%1").arg(lang),
                 value);
        emit default_tts_models_changed(lang);
    }
}

bool settings::restore_punctuation() const {
    return value(QStringLiteral("service/restore_punctuation"), false).toBool();
}

void settings::set_restore_punctuation(bool value) {
    if (restore_punctuation() != value) {
        setValue(QStringLiteral("service/restore_punctuation"), value);
        emit restore_punctuation_changed();
    }
}

bool settings::whisper_use_gpu() const {
    return value(QStringLiteral("service/whisper_use_gpu"), false).toBool();
}

void settings::set_whisper_use_gpu(bool value) {
    if (whisper_use_gpu() != value) {
        setValue(QStringLiteral("service/whisper_use_gpu"), value);
        emit whisper_use_gpu_changed();
    }
}

QString settings::default_tts_model_for_mnt_lang(const QString& lang) {
    return value(QStringLiteral("default_tts_model_for_mnt_%1").arg(lang), {})
        .toString();
}

void settings::set_default_tts_model_for_mnt_lang(const QString& lang,
                                                  const QString& value) {
    if (default_tts_model_for_mnt_lang(lang) != value) {
        setValue(QStringLiteral("default_tts_model_for_mnt_%1").arg(lang),
                 value);
        emit default_tts_models_for_mnt_changed(lang);
    }
}

QString settings::file_save_dir() const {
    auto dir = value(QStringLiteral("file_save_dir")).toString();
    if (dir.isEmpty() || !QFileInfo::exists(dir)) {
        dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    }

    return dir;
}

QUrl settings::file_save_dir_url() const {
    return QUrl::fromLocalFile(file_save_dir());
}

void settings::set_file_save_dir(const QString& value) {
    if (file_save_dir() != value) {
        setValue(QStringLiteral("file_save_dir"), value);
        emit file_save_dir_changed();
    }
}

void settings::set_file_save_dir_url(const QUrl& value) {
    set_file_save_dir(value.toLocalFile());
}

QString settings::file_save_dir_name() const {
    return file_save_dir_url().fileName();
}

QString settings::file_save_filename() const {
    auto dir = QDir{file_save_dir()};

    auto filename = QStringLiteral("speech-note-%1.wav");

    for (int i = 1; i <= 1000; ++i) {
        auto fn = filename.arg(i);
        if (!QFileInfo::exists(dir.filePath(fn))) return fn;
    }

    return filename.arg(1);
}

QString settings::file_open_dir() const {
    auto dir = value(QStringLiteral("file_open_dir")).toString();
    if (dir.isEmpty() || !QFileInfo::exists(dir)) {
        dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    }

    return dir;
}

QUrl settings::file_open_dir_url() const {
    return QUrl::fromLocalFile(file_open_dir());
}

void settings::set_file_open_dir(const QString& value) {
    if (file_open_dir() != value) {
        setValue(QStringLiteral("file_open_dir"), value);
        emit file_open_dir_changed();
    }
}

void settings::set_file_open_dir_url(const QUrl& value) {
    set_file_open_dir(value.toLocalFile());
}

QString settings::file_open_dir_name() const {
    return file_open_dir_url().fileName();
}

settings::mode_t settings::mode() const {
    return static_cast<mode_t>(
        value(QStringLiteral("mode"), static_cast<int>(mode_t::Stt)).toInt());
}

void settings::set_mode(mode_t value) {
    if (mode() != value) {
        setValue(QStringLiteral("mode"), static_cast<int>(value));
        emit mode_changed();
    }
}

settings::speech_mode_t settings::speech_mode() const {
    return static_cast<speech_mode_t>(
        value(QStringLiteral("speech_mode"),
              static_cast<int>(speech_mode_t::SpeechSingleSentence))
            .toInt());
}

void settings::set_speech_mode(speech_mode_t value) {
    if (speech_mode() != value) {
        setValue(QStringLiteral("speech_mode"), static_cast<int>(value));
        emit speech_mode_changed();
    }
}

settings::speech_speed_t settings::speech_speed() const {
    return static_cast<speech_speed_t>(
        value(QStringLiteral("speech_speed"),
              static_cast<int>(speech_speed_t::SpeechSpeedNormal))
            .toInt());
}

void settings::set_speech_speed(speech_speed_t value) {
    if (speech_speed() != value) {
        setValue(QStringLiteral("speech_speed"), static_cast<int>(value));
        emit speech_speed_changed();
    }
}

QString settings::note() const {
    return value(QStringLiteral("note"), {}).toString();
}

void settings::set_note(const QString& value) {
    if (note() != value) {
        setValue(QStringLiteral("note"), value);
        emit note_changed();
    }
}

int settings::font_size() const {
    return value(QStringLiteral("font_size"), 13).toInt();
}

void settings::set_font_size(int value) {
    if (font_size() != value) {
        setValue(QStringLiteral("font_size"), value);
        emit font_size_changed();
    }
}

bool settings::translator_mode() const {
    return value(QStringLiteral("translator_mode"), false).toBool();
}

void settings::set_translator_mode(bool value) {
    if (translator_mode() != value) {
        setValue(QStringLiteral("translator_mode"), value);
        emit translator_mode_changed();
    }
}

bool settings::translate_when_typing() const {
    return value(QStringLiteral("translate_when_typing"), false).toBool();
}

void settings::set_translate_when_typing(bool value) {
    if (translate_when_typing() != value) {
        setValue(QStringLiteral("translate_when_typing"), value);
        emit translate_when_typing_changed();
    }
}

bool settings::hint_translator() const {
    return value(QStringLiteral("hint_translator"), true).toBool();
}

void settings::set_hint_translator(bool value) {
    if (hint_translator() != value) {
        setValue(QStringLiteral("hint_translator"), value);
        emit hint_translator_changed();
    }
}

settings::insert_mode_t settings::insert_mode() const {
    return static_cast<insert_mode_t>(
        value(QStringLiteral("insert_mode"),
              static_cast<int>(insert_mode_t::InsertInLine))
            .toInt());
}

void settings::set_insert_mode(insert_mode_t value) {
    if (insert_mode() != value) {
        setValue(QStringLiteral("insert_mode"), static_cast<int>(value));
        emit insert_mode_changed();
    }
}

int settings::qt_style_idx() const {
    return value(QStringLiteral("qt_style_idx"), 0).toInt();
}

void settings::set_qt_style_idx(int value) {
    if (qt_style_idx() != value) {
        setValue(QStringLiteral("qt_style_idx"), value);
        emit qt_style_idx_changed();
        set_restart_required();
    }
}

QUrl settings::app_icon() const {
#ifdef USE_SFOS
    return QUrl::fromLocalFile(
        QStringLiteral("/usr/share/icons/hicolor/172x172/apps/%1.png")
            .arg(QLatin1String{APP_BINARY_ID}));
#else
    return QUrl{QStringLiteral("qrc:/app_icon.svg")};
#endif
}

bool settings::py_supported() const {
#ifdef USE_PY
    return true;
#else
    return false;
#endif
}

bool settings::gpu_supported() const {
#ifdef ARCH_X86_64
    return true;
#else
    return false;
#endif
}

settings::launch_mode_t settings::launch_mode() const { return m_launch_mode; }

void settings::set_launch_mode(launch_mode_t launch_mode) {
    m_launch_mode = launch_mode;
}

QString settings::module_checksum(const QString& name) const {
    return value(QStringLiteral("service/module_%1_checksum").arg(name))
        .toString();
}

void settings::set_module_checksum(const QString& name, const QString& value) {
    if (value != module_checksum(name)) {
        setValue(QStringLiteral("service/module_%1_checksum").arg(name), value);
    }
}

QString settings::prev_app_ver() const {
    return value(QStringLiteral("prev_app_ver")).toString();
}

void settings::set_prev_app_ver(const QString& value) {
    if (prev_app_ver() != value) {
        setValue(QStringLiteral("prev_app_ver"), value);
        emit prev_app_ver_changed();
    }
}

bool settings::restart_required() const { return m_restart_required; }

void settings::set_restart_required() {
    if (!restart_required()) {
        m_restart_required = true;
        emit restart_required_changed();
    }
}

void settings::update_qt_style() const {
#ifdef USE_DESKTOP
    QString style;

    switch (qt_style_idx()) {
        case 0:
            style = QStringLiteral("org.kde.desktop");  // default
            break;
        case 1:
            style = QStringLiteral("Basic");
            break;
        case 2:
            style = QStringLiteral("Default");
            break;
        case 3:
            style = QStringLiteral("Plasma");
            break;
    }

    qDebug() << "swithing to style:" << style;

    QQuickStyle::setStyle(style);
#endif
}

QStringList settings::gpu_devices() const { return m_gpu_devices; }

bool settings::has_gpu_device() const { return m_gpu_devices.size() > 1; }

void settings::update_gpu_devices() {
#ifdef ARCH_X86_64
    auto devices = opencl_tools::available_devices();

    m_gpu_devices.clear();
    m_gpu_devices.push_back(tr("Auto"));

    std::transform(devices.cbegin(), devices.cend(),
                   std::back_inserter(m_gpu_devices), [](const auto& device) {
                       return QStringLiteral("%1, %2").arg(
                           QString::fromStdString(device.platform_name),
                           QString::fromStdString(device.device_name));
                   });

    emit gpu_devices_changed();
#endif
}

QString settings::gpu_device() const {
    return value(QStringLiteral("service/gpu_device")).toString();
}

void settings::set_gpu_device(QString value) {
    if (value == tr("Auto")) value.clear();

    if (value != gpu_device()) {
        setValue(QStringLiteral("service/gpu_device"), value);
        emit gpu_device_changed();
    }
}

int settings::gpu_device_idx() const {
    auto current_device = gpu_device();

    if (current_device.isEmpty()) return 0;

    auto it =
        std::find(m_gpu_devices.cbegin(), m_gpu_devices.cend(), current_device);
    if (it == m_gpu_devices.cend()) return 0;

    return std::distance(m_gpu_devices.cbegin(), it);
}

void settings::set_gpu_device_idx(int value) {
    if (value < 0 || value >= m_gpu_devices.size()) return;
    set_gpu_device(value == 0 ? "" : m_gpu_devices.at(value));
}
