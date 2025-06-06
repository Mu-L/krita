/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "SvgTextEditor.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSvgGenerator>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QScreen>
#include <QScreen>

#include <kcharselect.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <kactioncollection.h>
#include <kxmlguifactory.h>
#include <ktoolbar.h>
#include <ktoggleaction.h>
#include <kguiitem.h>
#include <kstandardguiitem.h>

#include <KoDialog.h>
#include <KoResourcePaths.h>
#include <KoSvgTextShape.h>
#include <KoSvgTextShapeMarkupConverter.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorPopupAction.h>
#include <svg/SvgUtil.h>
#include <KisPortingUtils.h>

#include <KisSpinBoxI18nHelper.h>
#include <KisScreenColorSampler.h>
#include <kis_icon.h>
#include <kis_config.h>
#include <kis_file_name_requester.h>
#include <kis_action_registry.h>

#include "kis_font_family_combo_box.h"
#include "FontSizeAction.h"
#include "kis_signals_blocker.h"

using WrappingMode = KoSvgTextShapeMarkupConverter::WrappingMode;


class SvgTextEditor::Private
{
public:

    Private()
    {
        // some platform plugins may have set a pixelsize which could create a
        // conflicting property in QTextCharFormat
        font.setPointSize(fontSize);
    }

    QActionGroup *textWrappingActionGroup {};

    // collection of last-used properties
    QColor fontColor {Qt::black};
    //QColor backgroundColor {Qt::transparent};
    qreal fontSize {10.0};
    QFont font;
    bool kerning {true};

    qreal letterSpacing {0.0};

    bool bold {false};
    bool italic {false};
    bool underline {false};

    bool strikeThrough {false};
    bool superscript {false};
    bool subscript {false};

    // unsupported properties:
    // backgroundColor - because there is no button for that
    // horizontal alignment - it seems to work without saving
    // line height - it seems to work without saving

    void saveFromWidgets(KisKActionCollection* actions)
    {

        FontSizeAction *fontSizeAction = qobject_cast<FontSizeAction*>(actions->action("svg_font_size"));
        fontSize = fontSizeAction->fontSize();

        KisFontComboBoxes* fontComboBox2 = qobject_cast<KisFontComboBoxes*>(qobject_cast<QWidgetAction*>(actions->action("svg_font"))->defaultWidget());
        font = fontComboBox2->currentFont(fontSize);

        KoColorPopupAction *fontColorAction = qobject_cast<KoColorPopupAction*>(actions->action("svg_format_textcolor"));
        fontColor = fontColorAction->currentColor();

        QWidgetAction *letterSpacingAction = qobject_cast<QWidgetAction*>(actions->action("svg_letter_spacing"));
        letterSpacing = qobject_cast<QDoubleSpinBox*>(letterSpacingAction->defaultWidget())->value();

        saveBoolActionFromWidget(actions, "svg_weight_bold", bold);
        saveBoolActionFromWidget(actions, "svg_format_italic", italic);
        saveBoolActionFromWidget(actions, "svg_format_underline", underline);

        saveBoolActionFromWidget(actions, "svg_format_strike_through", strikeThrough);
        saveBoolActionFromWidget(actions, "svg_format_superscript", superscript);
        saveBoolActionFromWidget(actions, "svg_format_subscript", subscript);

        saveBoolActionFromWidget(actions, "svg_font_kerning", kerning);
    }

    void setSavedToWidgets(KisKActionCollection* actions)
    {

        FontSizeAction *fontSizeAction = qobject_cast<FontSizeAction*>(actions->action("svg_font_size"));
        fontSizeAction->setFontSize(fontSize);

        KisFontComboBoxes* fontComboBox2 = qobject_cast<KisFontComboBoxes*>(qobject_cast<QWidgetAction*>(actions->action("svg_font"))->defaultWidget());
        fontComboBox2->setCurrentFont(font);

        KoColorPopupAction *fontColorAction = qobject_cast<KoColorPopupAction*>(actions->action("svg_format_textcolor"));
        fontColorAction->setCurrentColor(fontColor);

        QWidgetAction *letterSpacingAction = qobject_cast<QWidgetAction*>(actions->action("svg_letter_spacing"));
        qobject_cast<QDoubleSpinBox*>(letterSpacingAction->defaultWidget())->setValue(letterSpacing);

        setBoolActionToWidget(actions, "svg_weight_bold", bold);
        setBoolActionToWidget(actions, "svg_format_italic", italic);

        setSavedLineDecorationToWidgets(actions);

        setBoolActionToWidget(actions, "svg_format_superscript", superscript);
        setBoolActionToWidget(actions, "svg_format_subscript", subscript);

        setBoolActionToWidget(actions, "svg_font_kerning", kerning);
    }

    void setSavedToFormat(QTextCharFormat &format)
    {

        format.setFont(font);
        format.setFontPointSize(fontSize);
        format.setForeground(fontColor);

        format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        format.setFontLetterSpacing(letterSpacing);

        format.setFontUnderline(underline);
        format.setFontStrikeOut(strikeThrough);
        format.setFontOverline(false);

        if (subscript) {
            format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
        } else if (superscript) {
            format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
        } else {
            format.setVerticalAlignment(QTextCharFormat::AlignMiddle);
        }

        if (bold) {
            format.setFontWeight(QFont::Bold);
        }

        format.setFontItalic(italic);
        format.setFontKerning(kerning);
    }

    void saveFontLineDecoration(KoSvgText::TextDecoration decoration)
    {
        // Krita for now cannot handle both at the same time; and there is no way to set overline
        // FIXME: Krita should support all three at the same time
        // (It does support it in SVG)
        switch (decoration) {
        case KoSvgText::DecorationUnderline:
            underline = true;
            strikeThrough = false;
            break;
        case KoSvgText::DecorationLineThrough:
            underline = false;
            strikeThrough = true;
            break;
        case KoSvgText::DecorationOverline:
            Q_FALLTHROUGH();
        case KoSvgText::DecorationNone:
            Q_FALLTHROUGH();
         default:
            underline = false;
            strikeThrough = false;
            break;
        }
    }


    void setSavedLineDecorationToWidgets(KisKActionCollection* actions)
    {
        setBoolActionToWidget(actions, "svg_format_underline", underline);
        setBoolActionToWidget(actions, "svg_format_strike_through", strikeThrough);
    }

private:

    void saveBoolActionFromWidget(KisKActionCollection* actions, QString actionName, bool &variable)
    {
        QAction *boolAction = actions->action(actionName);
        KIS_ASSERT_RECOVER_RETURN(boolAction);
        variable = boolAction->isChecked();
    }

    void setBoolActionToWidget(KisKActionCollection* actions, QString actionName, bool variable)
    {
        QAction *boolAction = actions->action(actionName);
        KIS_ASSERT_RECOVER_RETURN(boolAction);
        boolAction->setChecked(variable);
    }

};


SvgTextEditor::SvgTextEditor(QWidget *parent, Qt::WindowFlags flags)
    : KXmlGuiWindow(parent, flags)
    , m_page(new QWidget(this))
#ifndef Q_OS_WIN
    , m_charSelectDialog(new KoDialog(this))
#endif
    , d(new Private())
{
    m_textEditorWidget.setupUi(m_page);
    setCentralWidget(m_page);

#ifndef Q_OS_WIN
    KCharSelect *charSelector = new KCharSelect(m_charSelectDialog, 0, KCharSelect::AllGuiElements);
    m_charSelectDialog->setMainWidget(charSelector);
    connect(charSelector, SIGNAL(currentCharChanged(QChar)), SLOT(insertCharacter(QChar)));
    m_charSelectDialog->hide();
    m_charSelectDialog->setButtons(KoDialog::Close);
#endif
    KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Save), KStandardGuiItem::save());
    KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    connect(m_textEditorWidget.buttons, SIGNAL(accepted()), this, SLOT(save()));
    connect(m_textEditorWidget.buttons, SIGNAL(rejected()), this, SLOT(slotCloseEditor()));
    connect(m_textEditorWidget.buttons, SIGNAL(clicked(QAbstractButton*)), this, SLOT(dialogButtonClicked(QAbstractButton*)));

    KConfigGroup cg(KSharedConfig::openConfig(), "SvgTextTool");
    actionCollection()->setConfigGroup("SvgTextTool");
    actionCollection()->setComponentName("svgtexttool");
    actionCollection()->setComponentDisplayName(i18n("Text Tool"));

    if (cg.hasKey("WindowState")) {
        QByteArray state = cg.readEntry("State", QByteArray());
        // One day will need to load the version number, but for now, assume 0
        restoreState(QByteArray::fromBase64(state));
    }
    if (cg.hasKey("Geometry")) {
        QByteArray ba = cg.readEntry("Geometry", QByteArray());
        restoreGeometry(QByteArray::fromBase64(ba));
    }
    else {
        const int scnum = KisPortingUtils::getScreenNumberForWidget(QApplication::activeWindow());
        QRect desk = QGuiApplication::screens().at(scnum)->availableGeometry();

        quint32 x = desk.x();
        quint32 y = desk.y();
        quint32 w = 0;
        quint32 h = 0;
        const int deskWidth = desk.width();
        w = (deskWidth / 3) * 2;
        h = (desk.height() / 3) * 2;
        x += (desk.width() - w) / 2;
        y += (desk.height() - h) / 2;

        move(x,y);
        setGeometry(geometry().x(), geometry().y(), w, h);

    }

    setAcceptDrops(true);
    //setStandardToolBarMenuEnabled(true);
#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
#endif
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    m_syntaxHighlighter = new BasicXMLSyntaxHighlighter(m_textEditorWidget.svgTextEdit);
    m_textEditorWidget.svgTextEdit->setFont(QFontDatabase().systemFont(QFontDatabase::FixedFont));

    createActions();
    // If we have customized the toolbars, load that first
    setLocalXMLFile(KoResourcePaths::locateLocal("data", "svgtexttool.xmlgui"));
    setXMLFile(":/kxmlgui5/svgtexttool.xmlgui");

    guiFactory()->addClient(this);

    // Create and plug toolbar list for Settings menu
    QList<QAction *> toolbarList;
    Q_FOREACH (QWidget* it, guiFactory()->containers("ToolBar")) {
        KisToolBar * toolBar = ::qobject_cast<KisToolBar *>(it);
        if (toolBar) {
            toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
            KToggleAction* act = new KToggleAction(i18n("Show %1 Toolbar", toolBar->windowTitle()), this);
            actionCollection()->addAction(toolBar->objectName().toUtf8(), act);
            act->setCheckedState(KGuiItem(i18n("Hide %1 Toolbar", toolBar->windowTitle())));
            connect(act, SIGNAL(toggled(bool)), this, SLOT(slotToolbarToggled(bool)));
            act->setChecked(!toolBar->isHidden());
            toolbarList.append(act);
        }
    }

    {
        // Make the text color and text wrapping actions a simple drop-down
        // button and not the split button/drop-down combo.
        QAction *const color = actionCollection()->action("svg_format_textcolor");
        Q_FOREACH(KisToolBar *const toolBar, toolBars()) {
            if (QToolButton *const btn = qobject_cast<QToolButton *>(toolBar->widgetForAction(color))) {
                btn->setPopupMode(QToolButton::InstantPopup);
            }
            Q_FOREACH(QAction *const action, toolBar->actions()) {
                if (action->objectName() == QLatin1String("svg_text_wrapping")) {
                    if (QToolButton *const btn = qobject_cast<QToolButton *>(toolBar->widgetForAction(action))) {
                        btn->setPopupMode(QToolButton::InstantPopup);
                    }
                }
            }
        }
    }

    plugActionList("toolbarlist", toolbarList);
    connect(m_textEditorWidget.textTab, SIGNAL(currentChanged(int)), this, SLOT(switchTextEditorTab()));
    switchTextEditorTab();

    m_textEditorWidget.richTextEdit->document()->setDefaultStyleSheet("p {margin:0px;}");
    m_textEditorWidget.richTextEdit->installEventFilter(this);

    applySettings();

}

SvgTextEditor::~SvgTextEditor()
{
    KConfigGroup g(KSharedConfig::openConfig(), "SvgTextTool");
    QByteArray ba = saveState();
    g.writeEntry("windowState", ba.toBase64());
    ba = saveGeometry();
    g.writeEntry("Geometry", ba.toBase64());
}


void SvgTextEditor::setInitialShape(KoSvgTextShape *shape)
{
    m_shape = shape;
    if (m_shape) {
        KoSvgTextShapeMarkupConverter converter(m_shape);

        QString svg;
        QString styles;
        QTextDocument *doc = m_textEditorWidget.richTextEdit->document();

        if (converter.convertToSvg(&svg, &styles)) {
            m_textEditorWidget.svgTextEdit->setPlainText(svg);
            m_textEditorWidget.svgStylesEdit->setPlainText(styles);
            m_textEditorWidget.svgTextEdit->document()->setModified(false);

            /// Todo: remove this bool when removing rich text editor.
            bool richtextPreferred = false;
            if (richtextPreferred &&
                converter.convertSvgToDocument(svg, doc)) {

                m_textEditorWidget.richTextEdit->setDocument(doc);
                KisSignalsBlocker b(m_textEditorWidget.textTab);
                m_textEditorWidget.textTab->setCurrentIndex(Editor::Richtext);
                doc->clearUndoRedoStacks();
                switchTextEditorTab(false);
            } else {
                KisSignalsBlocker b(m_textEditorWidget.textTab);
                m_textEditorWidget.textTab->setCurrentIndex(Editor::SVGsource);
                switchTextEditorTab(false);
            }
        }
        else {
            QMessageBox::warning(this, i18n("Conversion failed"), "Could not get svg text from the shape:\n" + converter.errors().join('\n') + "\n" + converter.warnings().join('\n'));
        }
    }
    KisFontComboBoxes* fontComboBox = qobject_cast<KisFontComboBoxes*>(qobject_cast<QWidgetAction*>(actionCollection()->action("svg_font"))->defaultWidget());
    fontComboBox->setInitialized();

    KConfigGroup cfg(KSharedConfig::openConfig(), "SvgTextTool");

    d->saveFromWidgets(actionCollection());

    QTextCursor cursor = m_textEditorWidget.richTextEdit->textCursor();
    QTextCharFormat format = cursor.blockCharFormat();

    d->setSavedToFormat(format);

    KisSignalsBlocker b(m_textEditorWidget.richTextEdit);
    cursor.setBlockCharFormat(format);

    m_textEditorWidget.richTextEdit->document()->setModified(false);
    checkDocumentFormat();
}

void SvgTextEditor::save()
{
    if (m_shape) {
        if (isRichTextEditorTabActive()) {
            QString svg;
            QString styles = m_textEditorWidget.svgStylesEdit->document()->toPlainText();
            KoSvgTextShapeMarkupConverter converter(m_shape);

            if (!converter.convertDocumentToSvg(m_textEditorWidget.richTextEdit->document(), &svg)) {
                    qWarning()<<"new converter doesn't work!";
            }
            m_textEditorWidget.richTextEdit->document()->setModified(false);
            Q_EMIT textUpdated(m_shape, svg, styles);
        } else if (isSvgSourceEditorTabActive()) {
            Q_EMIT textUpdated(m_shape, m_textEditorWidget.svgTextEdit->document()->toPlainText(), m_textEditorWidget.svgStylesEdit->document()->toPlainText());
            m_textEditorWidget.svgTextEdit->document()->setModified(false);
        }
    }

}

void SvgTextEditor::switchTextEditorTab(bool convertData)
{
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);

    bool wasModified = false;
    if (m_currentEditor) {
        disconnect(m_currentEditor->document(), SIGNAL(modificationChanged(bool)), this, SLOT(setModified(bool)));
        wasModified = m_currentEditor->document()->isModified();
    }

    // do not switch to the same tab again, otherwise we're losing current changes
    if (m_currentEditor != m_textEditorWidget.richTextEdit && isRichTextEditorTabActive()) {
        //first, make buttons checkable
        enableRichTextActions(true);
        enableSvgTextActions(false);

        //then connect the cursor change to the checkformat();
        connect(m_textEditorWidget.richTextEdit, SIGNAL(cursorPositionChanged()), this, SLOT(checkFormat()));
        connect(m_textEditorWidget.richTextEdit, SIGNAL(textChanged()), this, SLOT(slotFixUpEmptyTextBlock()));
        connect(m_textEditorWidget.richTextEdit, SIGNAL(textChanged()), this, SLOT(checkDocumentFormat()));
        checkFormat();

        if (m_shape && convertData) {
            QTextDocument *doc = m_textEditorWidget.richTextEdit->document();
            if (!converter.convertSvgToDocument(m_textEditorWidget.svgTextEdit->document()->toPlainText(), doc)) {
                qWarning()<<"new converter svgToDoc doesn't work!";
            }
            m_textEditorWidget.richTextEdit->setDocument(doc);
            doc->clearUndoRedoStacks();
        }
        m_currentEditor = m_textEditorWidget.richTextEdit;
    } else if (m_currentEditor != m_textEditorWidget.svgTextEdit && isSvgSourceEditorTabActive()) {
        //first, make buttons uncheckable
        enableRichTextActions(false);
        enableSvgTextActions(true);
        disconnect(m_textEditorWidget.richTextEdit, SIGNAL(cursorPositionChanged()), this, SLOT(checkFormat()));

        // Convert the rich text to svg and styles strings
        if (m_shape && convertData) {
            QString svg;
            if (!converter.convertDocumentToSvg(m_textEditorWidget.richTextEdit->document(), &svg)) {
                    qWarning()<<"new converter docToSVG doesn't work!";
            }
            m_textEditorWidget.svgTextEdit->setPlainText(svg);
        }
        m_currentEditor = m_textEditorWidget.svgTextEdit;
    }

    m_currentEditor->document()->setModified(wasModified);
    connect(m_currentEditor->document(), SIGNAL(modificationChanged(bool)), SLOT(setModified(bool)));
}

void SvgTextEditor::checkFormat()
{
    QTextCharFormat format = m_textEditorWidget.richTextEdit->textCursor().charFormat();
    QTextBlockFormat blockFormat = m_textEditorWidget.richTextEdit->textCursor().blockFormat();

    // checkboxes do not Q_EMIT signals on manual switching, so we
    // can avoid blocking them

    if (format.fontWeight() > QFont::Normal) {
        actionCollection()->action("svg_weight_bold")->setChecked(true);
    } else {
        actionCollection()->action("svg_weight_bold")->setChecked(false);
    }
    actionCollection()->action("svg_format_italic")->setChecked(format.fontItalic());
    actionCollection()->action("svg_format_underline")->setChecked(format.fontUnderline());
    actionCollection()->action("svg_format_strike_through")->setChecked(format.fontStrikeOut());
    actionCollection()->action("svg_font_kerning")->setChecked(format.fontKerning());

    {
        FontSizeAction *fontSizeAction = qobject_cast<FontSizeAction*>(actionCollection()->action("svg_font_size"));
        KisSignalsBlocker b(fontSizeAction);
        qreal pointSize = format.fontPointSize();
        if (pointSize <= 0.0) {
            pointSize = format.font().pointSizeF();
        }
        fontSizeAction->setFontSize(pointSize);
    }


    {
        KoColor fg(format.foreground().color(), KoColorSpaceRegistry::instance()->rgb8());
        KoColorPopupAction *fgColorPopup = qobject_cast<KoColorPopupAction*>(actionCollection()->action("svg_format_textcolor"));
        KisSignalsBlocker b(fgColorPopup);
        fgColorPopup->setCurrentColor(fg);
    }

    {
        KoColor bg(format.foreground().color(), KoColorSpaceRegistry::instance()->rgb8());
        KoColorPopupAction *bgColorPopup = qobject_cast<KoColorPopupAction*>(actionCollection()->action("svg_background_color"));
        KisSignalsBlocker b(bgColorPopup);
        bgColorPopup->setCurrentColor(bg);
    }

    {
        KisFontComboBoxes* fontComboBox = qobject_cast<KisFontComboBoxes*>(qobject_cast<QWidgetAction*>(actionCollection()->action("svg_font"))->defaultWidget());
        KisSignalsBlocker b(fontComboBox);
        fontComboBox->setCurrentFont(format.font());
    }

    {
        QDoubleSpinBox *spnLineHeight = qobject_cast<QWidgetAction*>(actionCollection()->action("svg_line_height"))->defaultWidget()->findChild<QDoubleSpinBox *>();
        KisSignalsBlocker b(spnLineHeight);

        if (blockFormat.lineHeightType() == QTextBlockFormat::SingleHeight) {
            spnLineHeight->setValue(-1.0);
            spnLineHeight->setSingleStep(101.0);
        } else if(blockFormat.lineHeightType() == QTextBlockFormat::ProportionalHeight) {
            spnLineHeight->setValue(double(blockFormat.lineHeight()));
            spnLineHeight->setSingleStep(10.0);
        }
    }

    {
        QDoubleSpinBox* spnLetterSpacing = qobject_cast<QDoubleSpinBox*>(qobject_cast<QWidgetAction*>(actionCollection()->action("svg_letter_spacing"))->defaultWidget());
        KisSignalsBlocker b(spnLetterSpacing);
        spnLetterSpacing->setValue(format.fontLetterSpacing());
    }
}

void SvgTextEditor::checkDocumentFormat()
{
    QTextFrameFormat f = m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat();
    WrappingMode wrappingMode = KoSvgTextShapeMarkupConverter::getWrappingMode(f);
    switch (wrappingMode) {
    case WrappingMode::QtLegacy:
    default:
        if (wrappingMode != WrappingMode::QtLegacy) {
            // For sanity
            KoSvgTextShapeMarkupConverter::setWrappingMode(&f, WrappingMode::QtLegacy);
            m_textEditorWidget.richTextEdit->document()->rootFrame()->setFrameFormat(f);
        }
        m_textEditorWidget.richTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
        actionCollection()->action("svg_text_wrapping_legacy")->setChecked(true);
        break;
    case WrappingMode::WhiteSpacePre:
        m_textEditorWidget.richTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
        actionCollection()->action("svg_text_wrapping_css_pre")->setChecked(true);
        break;
    case WrappingMode::WhiteSpacePreWrap:
        const double inlineSize = KoSvgTextShapeMarkupConverter::getInlineSize(f).value_or(100.0);
        const int wrapWidth = inlineSize * (96.0 / 72.0) + f.leftMargin() + f.rightMargin();
        m_textEditorWidget.richTextEdit->setLineWrapColumnOrWidth(wrapWidth);
        m_textEditorWidget.richTextEdit->setLineWrapMode(QTextEdit::FixedPixelWidth);
        m_textEditorWidget.richTextEdit->setWordWrapMode(QTextOption::WordWrap);
        actionCollection()->action("svg_text_wrapping_css_pre_wrap")->setChecked(true);
        break;
    }
}

void SvgTextEditor::slotFixUpEmptyTextBlock()
{
    if (m_textEditorWidget.richTextEdit->document()->isEmpty()) {
        QTextCursor cursor = m_textEditorWidget.richTextEdit->textCursor();
        QTextCharFormat format = cursor.blockCharFormat();


        KisSignalsBlocker b(m_textEditorWidget.richTextEdit);

        d->setSavedToFormat(format);
        d->setSavedToWidgets(actionCollection());

        cursor.setBlockCharFormat(format);
    }
}

void SvgTextEditor::undo()
{
    m_currentEditor->undo();
}

void SvgTextEditor::redo()
{
    m_currentEditor->redo();
}

void SvgTextEditor::cut()
{
    m_currentEditor->cut();
}

void SvgTextEditor::copy()
{
    m_currentEditor->copy();
}

void SvgTextEditor::paste()
{
    m_currentEditor->paste();
}

void SvgTextEditor::selectAll()
{
    m_currentEditor->selectAll();
}

void SvgTextEditor::deselect()
{
    QTextCursor cursor(m_currentEditor->textCursor());
    cursor.clearSelection();
    m_currentEditor->setTextCursor(cursor);
}

void SvgTextEditor::find()
{
    QDialog findDialog;
    findDialog.setWindowTitle(i18n("Find Text"));
    QFormLayout *layout = new QFormLayout(&findDialog);
    QLineEdit *lnSearchKey = new QLineEdit();
    layout->addRow(i18n("Find:"), lnSearchKey);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    KGuiItem::assign(buttons->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttons->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());

    connect(buttons, SIGNAL(accepted()), &findDialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &findDialog, SLOT(reject()));

    if (findDialog.exec() == QDialog::Accepted) {
        m_searchKey = lnSearchKey->text();
        m_currentEditor->find(m_searchKey);
    }
}

void SvgTextEditor::findNext()
{
    if (!m_currentEditor->find(m_searchKey)) {
        QTextCursor cursor(m_currentEditor->textCursor());
        cursor.movePosition(QTextCursor::Start);
        m_currentEditor->setTextCursor(cursor);
        m_currentEditor->find(m_searchKey);
    }
}

void SvgTextEditor::findPrev()
{
    if (!m_currentEditor->find(m_searchKey,QTextDocument::FindBackward)) {
        QTextCursor cursor(m_currentEditor->textCursor());
        cursor.movePosition(QTextCursor::End);
        m_currentEditor->setTextCursor(cursor);
        m_currentEditor->find(m_searchKey,QTextDocument::FindBackward);
    }
}

void SvgTextEditor::replace()
{
    QDialog findDialog;
    findDialog.setWindowTitle(i18n("Find and Replace all"));
    QFormLayout *layout = new QFormLayout(&findDialog);
    QLineEdit *lnSearchKey = new QLineEdit();
    QLineEdit *lnReplaceKey = new QLineEdit();
    layout->addRow(i18n("Find:"), lnSearchKey);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    layout->addRow(i18n("Replace:"), lnReplaceKey);
    layout->addWidget(buttons);

    KGuiItem::assign(buttons->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(buttons->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());

    connect(buttons, SIGNAL(accepted()), &findDialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &findDialog, SLOT(reject()));

    if (findDialog.exec() == QDialog::Accepted) {
        QString search = lnSearchKey->text();
        QString replace = lnReplaceKey->text();
        QTextCursor cursor(m_currentEditor->textCursor());
        cursor.movePosition(QTextCursor::Start);
        m_currentEditor->setTextCursor(cursor);
        while(m_currentEditor->find(search)) {
            m_currentEditor->textCursor().removeSelectedText();
            m_currentEditor->textCursor().insertText(replace);
        }

    }
}


void SvgTextEditor::zoomOut()
{
    m_currentEditor->zoomOut();
}

void SvgTextEditor::zoomIn()
{
    m_currentEditor->zoomIn();
}

#ifndef Q_OS_WIN
void SvgTextEditor::showInsertSpecialCharacterDialog()
{
    m_charSelectDialog->setVisible(!m_charSelectDialog->isVisible());
}

void SvgTextEditor::insertCharacter(const QChar &c)
{
    m_currentEditor->textCursor().insertText(QString(c));
}
#endif

void SvgTextEditor::setTextBold(QFont::Weight weight)
{
    if (isRichTextEditorTabActive()) {
        QTextCharFormat format;
        QTextCursor oldCursor = setTextSelection();
        if (m_textEditorWidget.richTextEdit->textCursor().charFormat().fontWeight() > QFont::Normal && weight==QFont::Bold) {
            format.setFontWeight(QFont::Normal);
        } else {
            format.setFontWeight(weight);
        }
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    } else if (isSvgSourceEditorTabActive()) {
        QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"font-weight:700;\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    d->bold = weight == QFont::Bold;

    checkFormat();
}

void SvgTextEditor::setTextWeightLight()
{
    if (m_textEditorWidget.richTextEdit->textCursor().charFormat().fontWeight() < QFont::Normal) {
        setTextBold(QFont::Normal);
    } else {
        setTextBold(QFont::Light);
    }
}

void SvgTextEditor::setTextWeightNormal()
{
    setTextBold(QFont::Normal);
}

void SvgTextEditor::setTextWeightDemi()
{
    if (m_textEditorWidget.richTextEdit->textCursor().charFormat().fontWeight() != QFont::Normal) {
        setTextBold(QFont::Normal);
    } else {
        setTextBold(QFont::DemiBold);
    }
}

void SvgTextEditor::setTextWeightBlack()
{
    if (m_textEditorWidget.richTextEdit->textCursor().charFormat().fontWeight()>QFont::Normal) {
        setTextBold(QFont::Normal);
    } else {
        setTextBold(QFont::Black);
    }
}

void SvgTextEditor::setTextItalic(QFont::Style style)
{
    QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
    QString fontStyle = "inherit";

    if (style == QFont::StyleItalic) {
        fontStyle = "italic";
        d->italic = true;
    } else if (style == QFont::StyleOblique) {
        fontStyle = "oblique";
        d->italic = true;
    } else {
        d->italic = false;
    }


    if (isRichTextEditorTabActive()) {
        QTextCharFormat format;
        QTextCursor origCursor = setTextSelection();
        format.setFontItalic(!m_textEditorWidget.richTextEdit->textCursor().charFormat().fontItalic());
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(origCursor);
    } else if (isSvgSourceEditorTabActive()) {
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"font-style:"+fontStyle+";\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }

    checkFormat();
}

void SvgTextEditor::setTextDecoration(KoSvgText::TextDecoration decor)
{
    QTextCursor cursor = setTextSelection();
    QTextCharFormat currentFormat = m_textEditorWidget.richTextEdit->textCursor().charFormat();
    QTextCharFormat format;
    QString textDecoration = "inherit";

    if (decor == KoSvgText::DecorationUnderline) {
        textDecoration = "underline";
        if (currentFormat.fontUnderline()) {
            format.setFontUnderline(false);
        }
        else {
            format.setFontUnderline(true);
        }
        format.setFontOverline(false);
        format.setFontStrikeOut(false);
    }
    else if (decor == KoSvgText::DecorationLineThrough) {
        textDecoration = "line-through";
        format.setFontUnderline(false);
        format.setFontOverline(false);
        if (currentFormat.fontStrikeOut()) {
            format.setFontStrikeOut(false);
        }
        else {
            format.setFontStrikeOut(true);
        }
    }
    else if (decor == KoSvgText::DecorationOverline) {
        textDecoration = "overline";
        format.setFontUnderline(false);
        if (currentFormat.fontOverline()) {
            format.setFontOverline(false);
        }
        else {
            format.setFontOverline(true);
        }
        format.setFontStrikeOut(false);
    }

    if (isRichTextEditorTabActive()) {
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
    } else if (isSvgSourceEditorTabActive()) {
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"text-decoration:" + textDecoration + ";\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    m_textEditorWidget.richTextEdit->setTextCursor(cursor);

    d->saveFontLineDecoration(decor);
    d->setSavedLineDecorationToWidgets(actionCollection());
}

void SvgTextEditor::setTextUnderline()
{
    setTextDecoration(KoSvgText::DecorationUnderline);
}

void SvgTextEditor::setTextOverline()
{
    setTextDecoration(KoSvgText::DecorationOverline);
}

void SvgTextEditor::setTextStrikethrough()
{
    setTextDecoration(KoSvgText::DecorationLineThrough);
}

void SvgTextEditor::setTextSubscript()
{
    QTextCharFormat format = m_textEditorWidget.richTextEdit->textCursor().charFormat();
    if (format.verticalAlignment()==QTextCharFormat::AlignSubScript) {
        format.setVerticalAlignment(QTextCharFormat::AlignNormal);
        d->subscript = false;
    } else {
        format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
        d->subscript = true;
        d->superscript = false;
    }
    m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
}

void SvgTextEditor::setTextSuperScript()
{
    QTextCharFormat format = m_textEditorWidget.richTextEdit->textCursor().charFormat();
    if (format.verticalAlignment()==QTextCharFormat::AlignSuperScript) {
        format.setVerticalAlignment(QTextCharFormat::AlignNormal);
        d->superscript = false;
    } else {
        format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
        d->superscript = true;
        d->subscript = false;
    }
    m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
}

void SvgTextEditor::increaseTextSize()
{
    QTextCursor oldCursor = setTextSelection();
    QTextCharFormat format;
    qreal pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().fontPointSize();
    if (pointSize <= 0.0) {
        pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().font().pointSizeF();
    }
    if (pointSize <= 0.0) {
        pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().font().pixelSize();
    }
    format.setFontPointSize(pointSize+1.0);
    d->fontSize = format.fontPointSize();
    m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
    m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
}

void SvgTextEditor::decreaseTextSize()
{
    QTextCursor oldCursor = setTextSelection();
    QTextCharFormat format;
    qreal pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().fontPointSize();
    if (pointSize <= 0.0) {
        pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().font().pointSizeF();
    }
    if (pointSize <= 0.0) {
        pointSize = m_textEditorWidget.richTextEdit->textCursor().charFormat().font().pixelSize();
    }
    if (pointSize <= 1.0) {
        return;
    }
    format.setFontPointSize(qMax(pointSize-1.0, 1.0));
    d->fontSize = format.fontPointSize();
    m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
    m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
}

void SvgTextEditor::setLineHeight(double lineHeightPercentage)
{
    QDoubleSpinBox *spnLineHeight = qobject_cast<QWidgetAction*>(actionCollection()->action("svg_line_height"))->defaultWidget()->findChild<QDoubleSpinBox *>();
    QTextCursor oldCursor = setTextSelection();
    QTextBlockFormat format = m_textEditorWidget.richTextEdit->textCursor().blockFormat();
    if (lineHeightPercentage < 0.0) {
        format.setLineHeight(1.0, QTextBlockFormat::SingleHeight);
        spnLineHeight->setSingleStep(101.0);
    } else {
        format.setLineHeight(lineHeightPercentage, QTextBlockFormat::ProportionalHeight);
        spnLineHeight->setSingleStep(10.0);
    }
    m_textEditorWidget.richTextEdit->textCursor().mergeBlockFormat(format);
    m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
}

void SvgTextEditor::setLetterSpacing(double letterSpacing)
{
    QTextCursor cursor = setTextSelection();
    if (isRichTextEditorTabActive()) {
        QTextCharFormat format;
        format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        format.setFontLetterSpacing(letterSpacing);
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(cursor);
    } else if (isSvgSourceEditorTabActive()) {
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"letter-spacing:" + QString::number(letterSpacing) + "\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    d->letterSpacing = letterSpacing;
}

void SvgTextEditor::alignLeft()
{
    QTextBlockFormat format;
    format.setAlignment(Qt::AlignLeft);
    if (KoSvgTextShapeMarkupConverter::getWrappingMode(
            m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat())
        == WrappingMode::WhiteSpacePreWrap) {
        QTextCursor cursor(m_textEditorWidget.richTextEdit->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(format);
    } else {
        QTextCursor oldCursor = setTextSelection();
        m_textEditorWidget.richTextEdit->textCursor().mergeBlockFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    }
}

void SvgTextEditor::alignRight()
{
    QTextBlockFormat format;
    format.setAlignment(Qt::AlignRight);
    if (KoSvgTextShapeMarkupConverter::getWrappingMode(
            m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat())
        == WrappingMode::WhiteSpacePreWrap) {
        QTextCursor cursor(m_textEditorWidget.richTextEdit->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(format);
    } else {
        QTextCursor oldCursor = setTextSelection();
        m_textEditorWidget.richTextEdit->textCursor().mergeBlockFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    }
}

void SvgTextEditor::alignCenter()
{
    QTextBlockFormat format;
    format.setAlignment(Qt::AlignCenter);
    if (KoSvgTextShapeMarkupConverter::getWrappingMode(
            m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat())
        == WrappingMode::WhiteSpacePreWrap) {
        QTextCursor cursor(m_textEditorWidget.richTextEdit->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(format);
    } else {
        QTextCursor oldCursor = setTextSelection();
        m_textEditorWidget.richTextEdit->textCursor().mergeBlockFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    }
}

void SvgTextEditor::alignJustified()
{
    QTextCursor oldCursor = setTextSelection();
    QTextBlockFormat format = m_textEditorWidget.richTextEdit->textCursor().blockFormat();
    format.setAlignment(Qt::AlignJustify);
    m_textEditorWidget.richTextEdit->textCursor().mergeBlockFormat(format);
    m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
}

void SvgTextEditor::setSettings()
{
    KoDialog settingsDialog(this);
    Ui_WdgSvgTextSettings textSettings;
    QWidget *settingsPage = new QWidget(&settingsDialog);
    settingsDialog.setMainWidget(settingsPage);
    textSettings.setupUi(settingsPage);

    // get the settings and initialize the dialog
    KConfigGroup cfg(KSharedConfig::openConfig(), "SvgTextTool");

    QStringList selectedWritingSystems = cfg.readEntry("selectedWritingSystems", "").split(",");

    QList<QFontDatabase::WritingSystem> scripts = QFontDatabase().writingSystems();
    QStandardItemModel *writingSystemsModel = new QStandardItemModel(&settingsDialog);
    for (int s = 0; s < scripts.size(); s ++) {
        QString writingSystem = QFontDatabase().writingSystemName(scripts.at(s));
        QStandardItem *script = new QStandardItem(writingSystem);
        script->setCheckable(true);
        script->setCheckState(selectedWritingSystems.contains(QString::number(scripts.at(s))) ? Qt::Checked : Qt::Unchecked);
        script->setData((int)scripts.at(s));
        writingSystemsModel->appendRow(script);
    }
    textSettings.lwScripts->setModel(writingSystemsModel);

    QColor background = cfg.readEntry("colorEditorBackground", qApp->palette().window().color());
    textSettings.colorEditorBackground->setColor(background);
    textSettings.colorEditorForeground->setColor(cfg.readEntry("colorEditorForeground", qApp->palette().text().color()));

    textSettings.colorKeyword->setColor(cfg.readEntry("colorKeyword", QColor(background.value() < 100 ? Qt::cyan : Qt::blue)));
    textSettings.chkBoldKeyword->setChecked(cfg.readEntry("BoldKeyword", true));
    textSettings.chkItalicKeyword->setChecked(cfg.readEntry("ItalicKeyword", false));

    textSettings.colorElement->setColor(cfg.readEntry("colorElement", QColor(background.value() < 100 ? Qt::magenta : Qt::darkMagenta)));
    textSettings.chkBoldElement->setChecked(cfg.readEntry("BoldElement", true));
    textSettings.chkItalicElement->setChecked(cfg.readEntry("ItalicElement", false));

    textSettings.colorAttribute->setColor(cfg.readEntry("colorAttribute", QColor(background.value() < 100 ? Qt::green : Qt::darkGreen)));
    textSettings.chkBoldAttribute->setChecked(cfg.readEntry("BoldAttribute", true));
    textSettings.chkItalicAttribute->setChecked(cfg.readEntry("ItalicAttribute", true));

    textSettings.colorValue->setColor(cfg.readEntry("colorValue", QColor(background.value() < 100 ? Qt::red: Qt::darkRed)));
    textSettings.chkBoldValue->setChecked(cfg.readEntry("BoldValue", true));
    textSettings.chkItalicValue->setChecked(cfg.readEntry("ItalicValue", false));

    textSettings.colorComment->setColor(cfg.readEntry("colorComment", QColor(background.value() < 100 ? Qt::lightGray : Qt::gray)));
    textSettings.chkBoldComment->setChecked(cfg.readEntry("BoldComment", false));
    textSettings.chkItalicComment->setChecked(cfg.readEntry("ItalicComment", false));

    settingsDialog.setButtons(KoDialog::Ok | KoDialog::Cancel);
    if (settingsDialog.exec() == QDialog::Accepted) {
        // save  and set the settings
        QStringList writingSystems;
        for (int i = 0; i < writingSystemsModel->rowCount(); i++) {
            QStandardItem *item = writingSystemsModel->item(i);
            if (item->checkState() == Qt::Checked) {
                writingSystems.append(QString::number(item->data().toInt()));
            }
        }
        cfg.writeEntry("selectedWritingSystems", writingSystems.join(','));

        cfg.writeEntry("colorEditorBackground", textSettings.colorEditorBackground->color());
        cfg.writeEntry("colorEditorForeground", textSettings.colorEditorForeground->color());

        cfg.writeEntry("colorKeyword", textSettings.colorKeyword->color());
        cfg.writeEntry("BoldKeyword", textSettings.chkBoldKeyword->isChecked());
        cfg.writeEntry("ItalicKeyWord", textSettings.chkItalicKeyword->isChecked());

        cfg.writeEntry("colorElement", textSettings.colorElement->color());
        cfg.writeEntry("BoldElement", textSettings.chkBoldElement->isChecked());
        cfg.writeEntry("ItalicElement", textSettings.chkItalicElement->isChecked());

        cfg.writeEntry("colorAttribute", textSettings.colorAttribute->color());
        cfg.writeEntry("BoldAttribute", textSettings.chkBoldAttribute->isChecked());
        cfg.writeEntry("ItalicAttribute", textSettings.chkItalicAttribute->isChecked());

        cfg.writeEntry("colorValue", textSettings.colorValue->color());
        cfg.writeEntry("BoldValue", textSettings.chkBoldValue->isChecked());
        cfg.writeEntry("ItalicValue", textSettings.chkItalicValue->isChecked());

        cfg.writeEntry("colorComment", textSettings.colorComment->color());
        cfg.writeEntry("BoldComment", textSettings.chkBoldComment->isChecked());
        cfg.writeEntry("ItalicComment", textSettings.chkItalicComment->isChecked());

        applySettings();
    }
}

void SvgTextEditor::slotToolbarToggled(bool)
{
}

void SvgTextEditor::setFontColor(const KoColor &c)
{
    QColor color = c.toQColor();
    if (isRichTextEditorTabActive()) {
        QTextCursor oldCursor = setTextSelection();
        QTextCharFormat format;
        format.setForeground(QBrush(color));
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    } else if (isSvgSourceEditorTabActive()) {
        QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan fill=\""+color.name()+"\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    // save last used color to be used when the editor is cleared
    d->fontColor = color;
}

void SvgTextEditor::setBackgroundColor(const KoColor &c)
{
    QColor color = c.toQColor();
    QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
    if (cursor.hasSelection()) {
        QString selectionModified = "<tspan stroke=\""+color.name()+"\">" + cursor.selectedText() + "</tspan>";
        cursor.removeSelectedText();
        cursor.insertText(selectionModified);
    }
}

void SvgTextEditor::setModified(bool modified)
{
    if (modified) {
        m_textEditorWidget.buttons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Discard);
        KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Save), KStandardGuiItem::save());
        KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Discard), KStandardGuiItem::discard());
    }
    else {
        m_textEditorWidget.buttons->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Close);
        KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Save), KStandardGuiItem::save());
        KGuiItem::assign(m_textEditorWidget.buttons->button(QDialogButtonBox::Close), KStandardGuiItem::close());
    }
}

void SvgTextEditor::dialogButtonClicked(QAbstractButton *button)
{
    if (m_textEditorWidget.buttons->standardButton(button) == QDialogButtonBox::Discard) {
        if (QMessageBox::warning(this, i18nc("@title:window", "Krita"), i18n("You have modified the text. Discard changes?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            close();
        }
    }
}

void SvgTextEditor::setFont(const QString &fontName)
{
    QFont font;
    font.fromString(fontName);
    QTextCharFormat curFormat = m_textEditorWidget.richTextEdit->textCursor().charFormat();
    qreal pointSize = curFormat.fontPointSize();
    if (pointSize <= 0.0) {
        pointSize = curFormat.font().pointSizeF();
    }
    font.setPointSize(pointSize);

    QFontDatabase fontDatabase;
    const bool italic = fontDatabase.italic(font.family(), font.styleName());
    const int fontWeight = fontDatabase.weight(font.family(), font.styleName());
    if (isRichTextEditorTabActive()) {
        QTextCharFormat format;
        format.setFont(font);

        QTextCursor oldCursor = setTextSelection();
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    } else if (isSvgSourceEditorTabActive()) {
        QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"font-family:" + font.family() + ";" +
                                        "font-weight:" + QString::number(fontWeight) + ";"
                                        "font-style:" + (italic ? "italic" : "normal") + ";\">" +
                                        cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    // save last used font to be used when the editor is cleared
    d->font = font;

    checkFormat();
}

void SvgTextEditor::setFontSize(qreal fontSize)
{
    if (isRichTextEditorTabActive()) {
        QTextCursor oldCursor = setTextSelection();
        QTextCharFormat format;
        format.setFontPointSize(fontSize);
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(oldCursor);
    } else if (isSvgSourceEditorTabActive()) {
        QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
        if (cursor.hasSelection()) {
            QString selectionModified = "<tspan style=\"font-size:" + QString::number(fontSize) + ";\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
    // set last used font size to be used when the editor is cleared
    d->fontSize = fontSize;
}

void SvgTextEditor::setBaseline(KoSvgText::BaselineShiftMode)
{

    QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();
    if (cursor.hasSelection()) {
        QString selectionModified = "<tspan style=\"font-size:50%;baseline-shift:super;\">" + cursor.selectedText() + "</tspan>";
        cursor.removeSelectedText();
        cursor.insertText(selectionModified);
    }
}

void SvgTextEditor::setKerning(bool enable)
{
    d->kerning = enable;

    if (isRichTextEditorTabActive()) {
        QTextCharFormat format;
        QTextCursor origCursor = setTextSelection();
        format.setFontKerning(enable);
        m_textEditorWidget.richTextEdit->mergeCurrentCharFormat(format);
        m_textEditorWidget.richTextEdit->setTextCursor(origCursor);
    } else if (isSvgSourceEditorTabActive()) {
        QTextCursor cursor = m_textEditorWidget.svgTextEdit->textCursor();

        if (cursor.hasSelection()) {
            QString value;
            if (enable) {
                value = "auto";
            } else {
                value = "0";
            }
            
            QString selectionModified = "<tspan style=\"kerning:"+value+";\">" + cursor.selectedText() + "</tspan>";
            cursor.removeSelectedText();
            cursor.insertText(selectionModified);
        }
    }
}

void SvgTextEditor::setWrappingLegacy()
{
    QTextFrameFormat f = m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat();
    KoSvgTextShapeMarkupConverter::setWrappingMode(&f, WrappingMode::QtLegacy);
    m_textEditorWidget.richTextEdit->document()->rootFrame()->setFrameFormat(f);
    checkDocumentFormat();
}

void SvgTextEditor::setWrappingPre()
{
    QTextFrameFormat f = m_textEditorWidget.richTextEdit->document()->rootFrame()->frameFormat();
    KoSvgTextShapeMarkupConverter::setWrappingMode(&f, WrappingMode::WhiteSpacePre);
    m_textEditorWidget.richTextEdit->document()->rootFrame()->setFrameFormat(f);
    checkDocumentFormat();
}

void SvgTextEditor::setWrappingPreWrap()
{
    KisSignalsBlocker b(m_textEditorWidget.richTextEdit);

    QTextDocument *doc = m_textEditorWidget.richTextEdit->document();

    // `inline-size` can only support one block alignment for the whole text
    // element, therefore we should make all text blocks use the same alignment.
    {
        const Qt::Alignment firstBlockAlignment = doc->firstBlock().blockFormat().alignment();
        QTextBlockFormat blockFmt;
        blockFmt.setAlignment(firstBlockAlignment);
        QTextCursor cursor(doc);
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(blockFmt);
    }

    QTextFrameFormat f = doc->rootFrame()->frameFormat();
    const double inlineSize = KoSvgTextShapeMarkupConverter::getInlineSize(f).value_or(100.0);
    KoSvgTextShapeMarkupConverter::setWrappingMode(&f, WrappingMode::WhiteSpacePreWrap);
    KoSvgTextShapeMarkupConverter::setInlineSize(&f, inlineSize);
    doc->rootFrame()->setFrameFormat(f);
    checkDocumentFormat();
}

void SvgTextEditor::wheelEvent(QWheelEvent *event)
{
    if (!isSvgSourceEditorTabActive()) {
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        int numDegrees = event->angleDelta().y() / 8;
        int numSteps = numDegrees / 7;
        m_textEditorWidget.svgTextEdit->zoomOut(numSteps);
        event->accept();
    }
}

QTextCursor SvgTextEditor::setTextSelection()
{
    QTextCursor orignalCursor(m_textEditorWidget.richTextEdit->textCursor());
    if (!orignalCursor.hasSelection()){
        m_textEditorWidget.richTextEdit->selectAll();
    }
    return orignalCursor;
}

void SvgTextEditor::applySettings()
{
    KConfigGroup cfg(KSharedConfig::openConfig(), "SvgTextTool");

    QWidget *richTab = m_textEditorWidget.richTab;
    QWidget *svgTab = m_textEditorWidget.svgTab;

    m_page->setUpdatesEnabled(false);
    m_textEditorWidget.textTab->clear();

    m_textEditorWidget.textTab->addTab(richTab, i18n("Rich text"));
    m_textEditorWidget.textTab->addTab(svgTab, i18n("SVG Source"));

    m_syntaxHighlighter->setFormats();

    QPalette palette = m_textEditorWidget.svgTextEdit->palette();

    QColor background = cfg.readEntry("colorEditorBackground", qApp->palette().window().color());
    palette.setBrush(QPalette::Active, QPalette::Window, QBrush(background));
    m_textEditorWidget.richTextEdit->setStyleSheet(QString("background-color:%1").arg(background.name()));
    m_textEditorWidget.svgStylesEdit->setStyleSheet(QString("background-color:%1").arg(background.name()));
    m_textEditorWidget.svgTextEdit->setStyleSheet(QString("background-color:%1").arg(background.name()));

    QColor foreground = cfg.readEntry("colorEditorForeground", qApp->palette().text().color());
    palette.setBrush(QPalette::Active, QPalette::Text, QBrush(foreground));

    QStringList selectedWritingSystems = cfg.readEntry("selectedWritingSystems", "").split(",");

    QVector<QFontDatabase::WritingSystem> writingSystems;
    for (int i=0; i<selectedWritingSystems.size(); i++) {
        writingSystems.append((QFontDatabase::WritingSystem)QString(selectedWritingSystems.at(i)).toInt());
    }

    {
        FontSizeAction *fontSizeAction = qobject_cast<FontSizeAction*>(actionCollection()->action("svg_font_size"));
        KisFontComboBoxes* fontComboBox = qobject_cast<KisFontComboBoxes*>(qobject_cast<QWidgetAction*>(actionCollection()->action("svg_font"))->defaultWidget());

        const QFont oldFont = fontComboBox->currentFont(fontSizeAction->fontSize());
        fontComboBox->refillComboBox(writingSystems);
        fontComboBox->setCurrentFont(oldFont);
    }

    m_page->setUpdatesEnabled(true);
}

QAction *SvgTextEditor::createAction(const QString &name, const char *member)
{
    QAction *action = new QAction(this);
    KisActionRegistry *actionRegistry = KisActionRegistry::instance();
    actionRegistry->propertizeAction(name, action);

    actionCollection()->addAction(name, action);
    QObject::connect(action, SIGNAL(triggered(bool)), this, member);
    return action;
}


void SvgTextEditor::createActions()
{
    KisActionRegistry *actionRegistry = KisActionRegistry::instance();


    // File: new, open, save, save as, close
    KStandardAction::save(this, SLOT(save()), actionCollection());
    KStandardAction::close(this, SLOT(slotCloseEditor()), actionCollection());

    // Edit
    KStandardAction::undo(this, SLOT(undo()), actionCollection());
    KStandardAction::redo(this, SLOT(redo()), actionCollection());
    KStandardAction::cut(this, SLOT(cut()), actionCollection());
    KStandardAction::copy(this, SLOT(copy()), actionCollection());
    KStandardAction::paste(this, SLOT(paste()), actionCollection());
    KStandardAction::selectAll(this, SLOT(selectAll()), actionCollection());
    KStandardAction::deselect(this, SLOT(deselect()), actionCollection());
    KStandardAction::find(this, SLOT(find()), actionCollection());
    KStandardAction::findNext(this, SLOT(findNext()), actionCollection());
    KStandardAction::findPrev(this, SLOT(findPrev()), actionCollection());
    KStandardAction::replace(this, SLOT(replace()), actionCollection());

    // View
    // WISH: we cannot zoom-in/out in rech-text mode
    m_svgTextActions << KStandardAction::zoomOut(this, SLOT(zoomOut()), actionCollection());
    m_svgTextActions << KStandardAction::zoomIn(this, SLOT(zoomIn()), actionCollection());
#ifndef Q_OS_WIN
    // Insert:
    QAction * insertAction = createAction("svg_insert_special_character",
                                          SLOT(showInsertSpecialCharacterDialog()));
    insertAction->setCheckable(true);
    insertAction->setChecked(false);
#endif
    // Format:
    m_richTextActions << createAction("svg_weight_bold",
                                      SLOT(setTextBold()));

    m_richTextActions << createAction("svg_format_italic",
                                      SLOT(setTextItalic()));

    m_richTextActions << createAction("svg_format_underline",
                                      SLOT(setTextUnderline()));

    m_richTextActions << createAction("svg_format_strike_through",
                                      SLOT(setTextStrikethrough()));

    m_richTextActions << createAction("svg_format_superscript",
                                      SLOT(setTextSuperScript()));

    m_richTextActions << createAction("svg_format_subscript",
                                      SLOT(setTextSubscript()));

    m_richTextActions << createAction("svg_weight_light",
                                      SLOT(setTextWeightLight()));

    m_richTextActions << createAction("svg_weight_normal",
                                      SLOT(setTextWeightNormal()));

    m_richTextActions << createAction("svg_weight_demi",
                                      SLOT(setTextWeightDemi()));

    m_richTextActions << createAction("svg_weight_black",
                                      SLOT(setTextWeightBlack()));

    m_richTextActions << createAction("svg_increase_font_size",
                                      SLOT(increaseTextSize()));

    m_richTextActions << createAction("svg_decrease_font_size",
                                      SLOT(decreaseTextSize()));

    m_richTextActions << createAction("svg_align_left",
                                      SLOT(alignLeft()));

    m_richTextActions << createAction("svg_align_right",
                                      SLOT(alignRight()));

    m_richTextActions << createAction("svg_align_center",
                                      SLOT(alignCenter()));

//    m_richTextActions << createAction("svg_align_justified",
//                                      SLOT(alignJustified()));

    m_richTextActions << createAction("svg_font_kerning",
                                      SLOT(setKerning(bool)));

    d->textWrappingActionGroup = new QActionGroup(this);
    m_richTextActions << d->textWrappingActionGroup->addAction(
        createAction("svg_text_wrapping_legacy", SLOT(setWrappingLegacy())));
    m_richTextActions << d->textWrappingActionGroup->addAction(
        createAction("svg_text_wrapping_css_pre", SLOT(setWrappingPre())));
    m_richTextActions << d->textWrappingActionGroup->addAction(
        createAction("svg_text_wrapping_css_pre_wrap", SLOT(setWrappingPreWrap())));

    // Settings
    // do not add settings action to m_richTextActions list,
    // it should always be active, regardless of which editor mode is used.
    // otherwise we can lock the user out of being able to change
    // editor mode, if user changes to SVG only mode.
    createAction("svg_settings", SLOT(setSettings()));

    QWidgetAction *fontComboAction = new QWidgetAction(this);
    fontComboAction->setToolTip(i18n("Font"));
    KisFontComboBoxes *fontCombo = new KisFontComboBoxes();
    connect(fontCombo, SIGNAL(fontChanged(QString)), SLOT(setFont(QString)));
    fontComboAction->setDefaultWidget(fontCombo);
    actionCollection()->addAction("svg_font", fontComboAction);
    m_richTextActions << fontComboAction;
    actionRegistry->propertizeAction("svg_font", fontComboAction);

    QWidgetAction *fontSizeAction = new FontSizeAction(this);
    fontSizeAction->setToolTip(i18n("Size"));
    connect(fontSizeAction, SIGNAL(fontSizeChanged(qreal)), this, SLOT(setFontSize(qreal)));
    actionCollection()->addAction("svg_font_size", fontSizeAction);
    m_richTextActions << fontSizeAction;
    actionRegistry->propertizeAction("svg_font_size", fontSizeAction);

    KoColorPopupAction *fgColor = new KoColorPopupAction(this);
    fgColor->setCurrentColor(QColor(Qt::black));
    fgColor->setToolTip(i18n("Text Color"));
    connect(fgColor, SIGNAL(colorChanged(KoColor)), SLOT(setFontColor(KoColor)));
    actionCollection()->addAction("svg_format_textcolor", fgColor);
    m_richTextActions << fgColor;
    actionRegistry->propertizeAction("svg_format_textcolor", fgColor);

    KoColorPopupAction *bgColor = new KoColorPopupAction(this);
    bgColor->setCurrentColor(QColor(Qt::white));
    bgColor->setToolTip(i18n("Background Color"));
    connect(bgColor, SIGNAL(colorChanged(KoColor)), SLOT(setBackgroundColor(KoColor)));
    actionCollection()->addAction("svg_background_color", bgColor);
    actionRegistry->propertizeAction("svg_background_color", bgColor);
    m_richTextActions << bgColor;

    QWidgetAction *colorSamplerAction = new QWidgetAction(this);
    colorSamplerAction->setToolTip(i18n("Sample a Color"));
    KisScreenColorSampler *colorSampler = new KisScreenColorSampler(false);
    connect(colorSampler, SIGNAL(sigNewColorSampled(KoColor)), fgColor, SLOT(setCurrentColor(KoColor)));
    connect(colorSampler, SIGNAL(sigNewColorSampled(KoColor)), SLOT(setFontColor(KoColor)));
    colorSamplerAction->setDefaultWidget(colorSampler);
    actionCollection()->addAction("svg_sample_color", colorSamplerAction);
    m_richTextActions << colorSamplerAction;
    actionRegistry->propertizeAction("svg_sample_color", colorSamplerAction);

    QWidgetAction *lineHeight = new QWidgetAction(this);
    QWidget *lineHeightWdg = new QWidget();
    QHBoxLayout *lineHeightLayout = new QHBoxLayout(lineHeightWdg);
    lineHeightLayout->setSpacing(0);
    QDoubleSpinBox *spnLineHeight = new QDoubleSpinBox();
    spnLineHeight->setToolTip(i18n("Line height"));
    spnLineHeight->setRange(-1.0, 1000.0);
    spnLineHeight->setSingleStep(10.0);
    KisSpinBoxI18nHelper::setText(spnLineHeight, i18nc("{n} is the number value, % is the percent sign", "{n}%"));
    spnLineHeight->setSpecialValueText(i18nc("Default line height for text", "Normal"));
    connect(spnLineHeight, SIGNAL(valueChanged(double)), SLOT(setLineHeight(double)));
    lineHeightLayout->addWidget(spnLineHeight);
    {
        QToolButton *btn = new QToolButton();
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        btn->setArrowType(Qt::DownArrow);
        btn->setPopupMode(QToolButton::InstantPopup);
        QMenu *lineHeightMenu = new QMenu(this);
        QAction *actionNormal = lineHeightMenu->addAction(i18nc("Default line height for text", "Normal"));
        connect(actionNormal, &QAction::triggered, spnLineHeight, [spnLineHeight]() {
            spnLineHeight->setValue(-1.0);
        });
        QAction *action100 = lineHeightMenu->addAction(i18nc("line height for text", "100%"));
        connect(action100, &QAction::triggered, spnLineHeight, [spnLineHeight]() {
            spnLineHeight->setValue(100.0);
        });
        btn->setMenu(lineHeightMenu);
        lineHeightLayout->addWidget(btn);
    }
    lineHeight->setDefaultWidget(lineHeightWdg);
    actionCollection()->addAction("svg_line_height", lineHeight);
    m_richTextActions << lineHeight;
    actionRegistry->propertizeAction("svg_line_height", lineHeight);

    QWidgetAction *letterSpacing = new QWidgetAction(this);
    QDoubleSpinBox *spnletterSpacing = new QDoubleSpinBox();
    spnletterSpacing->setToolTip(i18n("Letter Spacing"));
    spnletterSpacing->setRange(-20.0, 20.0);
    spnletterSpacing->setSingleStep(0.5);
    connect(spnletterSpacing, SIGNAL(valueChanged(double)), SLOT(setLetterSpacing(double)));
    letterSpacing->setDefaultWidget(spnletterSpacing);
    actionCollection()->addAction("svg_letter_spacing", letterSpacing);
    m_richTextActions << letterSpacing;
    actionRegistry->propertizeAction("svg_letter_spacing", letterSpacing);
}

void SvgTextEditor::enableRichTextActions(bool enable)
{
    Q_FOREACH(QAction *action, m_richTextActions) {
        action->setEnabled(enable);
    }
}

void SvgTextEditor::enableSvgTextActions(bool enable)
{
    Q_FOREACH(QAction *action, m_svgTextActions) {
        action->setEnabled(enable);
    }
}

bool SvgTextEditor::isRichTextEditorTabActive() {
    return m_textEditorWidget.textTab->currentIndex() == Editor::Richtext;
}

bool SvgTextEditor::isSvgSourceEditorTabActive() {
    return m_textEditorWidget.textTab->currentIndex() == Editor::SVGsource;
}

void SvgTextEditor::slotCloseEditor()
{
    close();
    Q_EMIT textEditorClosed();
}

bool SvgTextEditor::eventFilter(QObject *const watched, QEvent *const event)
{
    if (watched == m_textEditorWidget.richTextEdit) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            QKeyEvent *const keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
                && (keyEvent->modifiers() & Qt::ShiftModifier)) {
                // Disable soft line breaks
                return true;
            }
        }
        return false;
    }
    return KXmlGuiWindow::eventFilter(watched, event);
}
