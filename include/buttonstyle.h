#ifndef BUTTONSTYLE_H
#define BUTTONSTYLE_H

#include <QString>

// Light-blue colour scheme shared by every push/tool button in the app.
// It is appended to the application stylesheet (see wfmain::setAppTheme) and
// reused by widgets that set their own per-button stylesheets.
#define WFVIEW_BTN_BG          "#b3e5fc"
#define WFVIEW_BTN_BG_HOVER    "#81d4fa"
#define WFVIEW_BTN_BG_CHECKED  "#4fc3f7"
#define WFVIEW_BTN_BORDER      "#4fa3c7"
#define WFVIEW_BTN_TEXT        "#202124"
#define WFVIEW_BTN_FONT_SIZE   "13pt"

static inline QString wfview_buttonStyleSheet()
{
    return QStringLiteral(
        "QPushButton, QToolButton {"
        " background-color: " WFVIEW_BTN_BG "; color: " WFVIEW_BTN_TEXT ";"
        " border: 1px solid " WFVIEW_BTN_BORDER "; border-radius: 4px; padding: 3px 6px;"
        " font-size: " WFVIEW_BTN_FONT_SIZE "; }"
        "QPushButton:hover, QToolButton:hover { background-color: " WFVIEW_BTN_BG_HOVER "; }"
        "QPushButton:pressed, QToolButton:pressed,"
        "QPushButton:checked, QToolButton:checked {"
        " background-color: " WFVIEW_BTN_BG_CHECKED "; color: " WFVIEW_BTN_TEXT "; }"
        "QPushButton:disabled, QToolButton:disabled {"
        " background-color: #e1f5fe; color: #9aa0a6; border-color: #b0cfdc; }");
}

// Per-button stylesheet for buttons that override the application sheet.
static inline QString wfview_buttonStyleSheet(const QString &extra)
{
    return QStringLiteral(
        "QPushButton { background-color: " WFVIEW_BTN_BG "; color: " WFVIEW_BTN_TEXT ";"
        " border: 1px solid " WFVIEW_BTN_BORDER "; font-size: " WFVIEW_BTN_FONT_SIZE "; %1 }"
        "QPushButton:hover { background-color: " WFVIEW_BTN_BG_HOVER "; }"
        "QPushButton:pressed, QPushButton:checked { background-color: " WFVIEW_BTN_BG_CHECKED "; }")
        .arg(extra);
}

// Larger buttons for main actions (settings window bottom row, profile
// save / delete) and for the exit confirmation dialogs.
static inline QString wfview_largeButtonStyleSheet()
{
    return wfview_buttonStyleSheet(
        QStringLiteral("font-size: 14pt; border-radius: 4px; padding: 6px 16px; min-height: 22px;"));
}

static inline QString wfview_largeDialogStyleSheet()
{
    return QStringLiteral("QLabel, QCheckBox { font-size: 14pt; }") + wfview_largeButtonStyleSheet();
}

// Darker blue used only for the transmit button so it stands out.
static inline QString wfview_transmitButtonStyleSheet(bool transmitting)
{
    return QStringLiteral(
        "QPushButton { background-color: #29b6f6; color: %1; border: 1px solid #0288d1;"
        " font-size: " WFVIEW_BTN_FONT_SIZE "; font-weight: bold; border-radius: 10px; padding: 4px 10px; }"
        "QPushButton:hover { background-color: #03a9f4; }"
        "QPushButton:pressed { background-color: #0288d1; }")
        .arg(transmitting ? QStringLiteral("crimson") : QStringLiteral(WFVIEW_BTN_TEXT));
}

#endif // BUTTONSTYLE_H
