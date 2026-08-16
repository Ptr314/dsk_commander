// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025-2026 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Application colour theme (system/light/dark) switching

#pragma once

#include <QObject>
#include <QPalette>
#include <QString>
#include <QtGlobal>

class QSettings;

// Run-time theme switching relies on QStyleHints::setColorScheme(), which only
// exists from Qt 6.8 onwards. On older Qt versions the manager degrades to a
// no-op that keeps the historic light appearance and the UI hides the menu.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    #define DSKCOM_THEME_SWITCHING 1
#else
    #define DSKCOM_THEME_SWITCHING 0
#endif

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        System,     // follow the OS/desktop setting
        Light,
        Dark
    };

    static ThemeManager & instance();

    // Whether the Qt version in use can switch themes at all.
    static bool isSupported() { return DSKCOM_THEME_SWITCHING != 0; }

    static Mode modeFromString(const QString & value);
    static QString modeToString(Mode mode);

    // Reads the stored mode, applies it and starts tracking system changes.
    // Must be called once, before the main window widgets are built.
    void initialize(QSettings * settings);

    Mode mode() const { return m_mode; }

    // Applies and persists the mode.
    void setMode(Mode mode);

    // Effective scheme currently in use (resolved for Mode::System).
    bool isDark() const { return m_dark; }

signals:
    // Emitted whenever the effective scheme changes, so that widgets painting
    // their own colours can repaint.
    void themeChanged(bool dark);

protected:
    bool eventFilter(QObject * watched, QEvent * event) override;

private:
    explicit ThemeManager(QObject * parent = nullptr);

    void apply();               // pushes m_mode into the platform
    void refresh();             // re-resolves the effective scheme
    void applyStyleSheet();
    void scheduleStyleSheetRefresh();
    void enforceColorScheme();  // fallback for platforms ignoring the request
    bool computeDark() const;

    static QPalette buildPalette(bool dark);

    QSettings * m_settings {nullptr};
    Mode m_mode {Mode::System};
    bool m_dark {false};
    bool m_applied {false};
    bool m_pending {false};
    bool m_fallback_active {false};
    QString m_default_style;
    QPalette m_default_palette;
};

// Shorthand for painting code that needs to pick a colour per scheme.
inline bool themeIsDark() { return ThemeManager::instance().isDark(); }