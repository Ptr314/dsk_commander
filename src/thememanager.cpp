// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025-2026 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Application colour theme (system/light/dark) switching

#include "thememanager.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

#if DSKCOM_THEME_SWITCHING
    #include <QGuiApplication>
    #include <QStyleHints>
#endif

// On Unix desktops other than macOS the platform theme plugin (GTK, Breeze, ...)
// may silently ignore an explicit colour scheme request. There we verify the
// result and fall back to Fusion with a hand-built palette.
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    #define DSKCOM_THEME_NEEDS_FALLBACK DSKCOM_THEME_SWITCHING
#else
    #define DSKCOM_THEME_NEEDS_FALLBACK 0
#endif

namespace {

const char * const SETTINGS_KEY = "interface/theme";

#if DSKCOM_THEME_SWITCHING
bool paletteIsDark()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}
#endif

} // namespace

ThemeManager::ThemeManager(QObject * parent)
    : QObject(parent)
{
}

ThemeManager & ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::Mode ThemeManager::modeFromString(const QString & value)
{
    const QString v = value.trimmed().toLower();
    if (v == QLatin1String("light")) return Mode::Light;
    if (v == QLatin1String("dark"))  return Mode::Dark;
    return Mode::System;
}

QString ThemeManager::modeToString(Mode mode)
{
    switch (mode) {
        case Mode::Light: return QStringLiteral("light");
        case Mode::Dark:  return QStringLiteral("dark");
        case Mode::System:
        default:          return QStringLiteral("system");
    }
}

void ThemeManager::initialize(QSettings * settings)
{
    m_settings = settings;

    // Remember what the platform gave us, so that the Linux fallback can be
    // undone when the user goes back to the system theme.
    if (QApplication::style())
        m_default_style = QApplication::style()->objectName();
    m_default_palette = QApplication::palette();

    if (isSupported() && m_settings)
        m_mode = modeFromString(m_settings->value(SETTINGS_KEY, QStringLiteral("system")).toString());
    else
        m_mode = Mode::System;

#if DSKCOM_THEME_SWITCHING
    // The system scheme can change while we are running (Windows/macOS
    // auto night mode, GNOME/KDE preference via the XDG portal).
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) { refresh(); });

    // Some platform themes only report the change through the palette.
    if (qApp) qApp->installEventFilter(this);
#endif

    apply();
}

void ThemeManager::setMode(Mode mode)
{
    if (!isSupported()) return;

    m_mode = mode;
    if (m_settings) {
        m_settings->setValue(SETTINGS_KEY, modeToString(mode));
        m_settings->sync();
    }
    apply();
}

void ThemeManager::apply()
{
#if DSKCOM_THEME_SWITCHING
    QStyleHints * hints = QGuiApplication::styleHints();
    switch (m_mode) {
        case Mode::Light:
            hints->setColorScheme(Qt::ColorScheme::Light);
            break;
        case Mode::Dark:
            hints->setColorScheme(Qt::ColorScheme::Dark);
            break;
        case Mode::System:
        default:
            hints->unsetColorScheme();
            break;
    }

    #if DSKCOM_THEME_NEEDS_FALLBACK
        // The platform applies the new palette asynchronously, so the result can
        // only be judged once the event loop has processed the theme change.
        QTimer::singleShot(0, this, [this]() { enforceColorScheme(); });
    #endif
#endif

    refresh();
}

bool ThemeManager::computeDark() const
{
#if DSKCOM_THEME_SWITCHING
    if (m_mode == Mode::Dark)  return true;
    if (m_mode == Mode::Light) return false;

    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)  return true;
    if (scheme == Qt::ColorScheme::Light) return false;

    // Platform could not tell us — judge by what the palette actually looks like.
    return paletteIsDark();
#else
    return false;
#endif
}

void ThemeManager::refresh()
{
    const bool dark = computeDark();
    if (m_applied && dark == m_dark) return;

    const bool first = !m_applied;
    m_dark = dark;
    m_applied = true;

    if (first) {
        // Start-up: no panel widget exists yet, so the sheet can go in right
        // away and every widget is built with its final look.
        applyStyleSheet();
        emit themeChanged(m_dark);
        return;
    }

    // A switch at run time. This runs from inside colorSchemeChanged, while Qt
    // is still propagating the new palette down the widget tree. Setting the
    // application stylesheet re-polishes every widget, and a widget polished at
    // that moment gets the *outgoing* palette pinned on it — which is why the
    // panels kept showing the previous scheme. Let the event loop finish the
    // propagation first.
    if (m_pending) return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() {
        m_pending = false;
        applyStyleSheet();
        emit themeChanged(m_dark);
    });
}

void ThemeManager::applyStyleSheet()
{
    const QString path = m_dark ? QStringLiteral(":/files/stylesheet_dark")
                                : QStringLiteral(":/files/stylesheet");
    QFile file(path);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void ThemeManager::enforceColorScheme()
{
#if DSKCOM_THEME_NEEDS_FALLBACK
    if (m_mode == Mode::System) {
        if (m_fallback_active) {
            m_fallback_active = false;
            if (!m_default_style.isEmpty()) {
                if (QStyle * style = QStyleFactory::create(m_default_style))
                    QApplication::setStyle(style);
            }
            QApplication::setPalette(m_default_palette);
        }
        refresh();
        return;
    }

    const bool want_dark = (m_mode == Mode::Dark);
    if (paletteIsDark() == want_dark) return;   // the platform honoured the request

    if (!m_fallback_active) {
        if (QStyle * fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            QApplication::setStyle(fusion);
        m_fallback_active = true;
    }
    QApplication::setPalette(buildPalette(want_dark));
    refresh();
#endif
}

QPalette ThemeManager::buildPalette(bool dark)
{
    if (!dark) {
        // Fusion's own light palette is a good match for the light stylesheet.
        return QApplication::style() ? QApplication::style()->standardPalette() : QPalette();
    }

    const QColor window(0x35, 0x35, 0x35);
    const QColor base(0x2a, 0x2a, 0x2a);
    const QColor text(0xe6, 0xe6, 0xe6);
    const QColor disabled(0x80, 0x80, 0x80);
    const QColor highlight(0x2a, 0x82, 0xda);

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, window);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
    p.setColor(QPalette::Link, highlight);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Light, QColor(0x4a, 0x4a, 0x4a));
    p.setColor(QPalette::Midlight, QColor(0x3f, 0x3f, 0x3f));
    p.setColor(QPalette::Mid, QColor(0x2f, 0x2f, 0x2f));
    p.setColor(QPalette::Dark, QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::Shadow, Qt::black);

    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);

    return p;
}

bool ThemeManager::eventFilter(QObject * watched, QEvent * event)
{
    // In system mode a desktop that reports its scheme only through the palette
    // (some GTK setups) still has to flip our own colours.
    if (watched == qApp
        && event->type() == QEvent::ApplicationPaletteChange
        && m_mode == Mode::System)
    {
        refresh();
    }
    return QObject::eventFilter(watched, event);
}