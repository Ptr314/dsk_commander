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

    scheduleStyleSheetRefresh();
}

void ThemeManager::scheduleStyleSheetRefresh()
{
    // A switch at run time can run from inside colorSchemeChanged, while Qt is
    // still propagating the new palette down the widget tree. Setting the
    // application stylesheet re-polishes every widget, and a widget polished at
    // that moment gets the *outgoing* palette pinned on it — which is why the
    // panels used to keep showing the previous scheme. Let the event loop
    // finish the propagation first.
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
            // Best effort: the desktop style and the palette as they were when
            // the program started. A desktop theme changed in the meantime is
            // only picked up again on the next run.
            if (!m_default_style.isEmpty()) {
                if (QStyle * style = QStyleFactory::create(m_default_style))
                    QApplication::setStyle(style);
            }
            QApplication::setPalette(m_default_palette);
            scheduleStyleSheetRefresh();
        }
        // On desktops that report no scheme of their own, the palette is the
        // only clue and it may only have settled by now.
        refresh();
        return;
    }

    const bool want_dark = (m_mode == Mode::Dark);
    if (!m_fallback_active && paletteIsDark() == want_dark)
        return;                                 // the platform honoured the request

    // The desktop kept its own scheme (KDE/Breeze and some GTK setups never
    // accept the request). Fusion follows a plain QPalette faithfully, so drive
    // the colours ourselves from here on.
    if (!m_fallback_active) {
        if (QStyle * fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            QApplication::setStyle(fusion);
        m_fallback_active = true;
    }
    QApplication::setPalette(buildPalette(want_dark));
    refresh();
    scheduleStyleSheetRefresh();
#endif
}

QPalette ThemeManager::buildPalette(bool dark)
{
    // Both schemes are spelled out here on purpose. QStyle::standardPalette()
    // cannot be used for this: since Qt 6.8 Fusion derives it from the colour
    // scheme *reported by the platform*, and this code only runs on platforms
    // that refused our request in the first place — on a KDE desktop set to
    // Breeze Dark it would hand back a dark palette for the light theme.

    QColor window, windowText, base, alternateBase, text, button, buttonText;
    QColor brightText, highlight, highlightedText, link, disabled;
    QColor light, midlight, mid, darkc, shadow, toolTipBase, toolTipText;

    if (dark) {
        window          = QColor(0x35, 0x35, 0x35);
        windowText      = QColor(0xe6, 0xe6, 0xe6);
        base            = QColor(0x2a, 0x2a, 0x2a);
        alternateBase   = QColor(0x35, 0x35, 0x35);
        text            = QColor(0xe6, 0xe6, 0xe6);
        button          = QColor(0x35, 0x35, 0x35);
        buttonText      = QColor(0xe6, 0xe6, 0xe6);
        brightText      = QColor(0xff, 0x6b, 0x6b);
        highlight       = QColor(0x2a, 0x82, 0xda);
        highlightedText = QColor(0x00, 0x00, 0x00);
        link            = QColor(0x2a, 0x82, 0xda);
        disabled        = QColor(0x80, 0x80, 0x80);
        light           = QColor(0x4a, 0x4a, 0x4a);
        midlight        = QColor(0x3f, 0x3f, 0x3f);
        mid             = QColor(0x2f, 0x2f, 0x2f);
        darkc           = QColor(0x20, 0x20, 0x20);
        shadow          = QColor(0x00, 0x00, 0x00);
        toolTipBase     = QColor(0x35, 0x35, 0x35);
        toolTipText     = QColor(0xe6, 0xe6, 0xe6);
    } else {
        window          = QColor(0xef, 0xef, 0xef);
        windowText      = QColor(0x00, 0x00, 0x00);
        base            = QColor(0xff, 0xff, 0xff);
        alternateBase   = QColor(0xf7, 0xf7, 0xf7);
        text            = QColor(0x00, 0x00, 0x00);
        button          = QColor(0xef, 0xef, 0xef);
        buttonText      = QColor(0x00, 0x00, 0x00);
        brightText      = QColor(0xff, 0xff, 0xff);
        highlight       = QColor(0x30, 0x8c, 0xc6);
        highlightedText = QColor(0xff, 0xff, 0xff);
        link            = QColor(0x00, 0x00, 0xff);
        disabled        = QColor(0xbe, 0xbe, 0xbe);
        light           = QColor(0xff, 0xff, 0xff);
        midlight        = QColor(0xca, 0xca, 0xca);
        mid             = QColor(0xb8, 0xb8, 0xb8);
        darkc           = QColor(0x9f, 0x9f, 0x9f);
        shadow          = QColor(0x76, 0x76, 0x76);
        toolTipBase     = QColor(0xff, 0xff, 0xdc);
        toolTipText     = QColor(0x00, 0x00, 0x00);
    }

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, windowText);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alternateBase);
    p.setColor(QPalette::ToolTipBase, toolTipBase);
    p.setColor(QPalette::ToolTipText, toolTipText);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, buttonText);
    p.setColor(QPalette::BrightText, brightText);
    p.setColor(QPalette::Link, link);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, highlightedText);
    p.setColor(QPalette::Light, light);
    p.setColor(QPalette::Midlight, midlight);
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Dark, darkc);
    p.setColor(QPalette::Shadow, shadow);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    p.setColor(QPalette::Accent, highlight);
#endif
    p.setColor(QPalette::PlaceholderText, disabled);

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