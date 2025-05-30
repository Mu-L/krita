/*
 *  SPDX-FileCopyrightText: 2020 Saurabh Kumar <saurabhk660@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StoryboardDelegate.h"
#include "StoryboardModel.h"

#include <QLineEdit>
#include <QTextEdit>
#include <QDebug>
#include <QStyle>
#include <QPainter>
#include <QApplication>
#include <QSize>
#include <QMouseEvent>
#include <QSpinBox>
#include <QScrollBar>

#include <kis_icon.h>
#include <kis_image_animation_interface.h>
#include <commands_new/kis_switch_current_time_command.h>
#include "KisAddRemoveStoryboardCommand.h"


StoryboardDelegate::StoryboardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_view(nullptr)
    , m_imageSize(QSize())
{
}

StoryboardDelegate::~StoryboardDelegate()
{
}

void StoryboardDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    p->save();
    {
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, p, option.widget);

        p->setFont(option.font);
        if (!index.isValid()) {
            p->restore();
            return;
        }
        if (!index.parent().isValid()) {
            QRect parentRect = option.rect;
            p->setPen(QPen(option.palette.window(), 2));
            p->drawRect(parentRect);

            parentRect.setTopLeft(parentRect.topLeft() + QPoint(4, 4));
            parentRect.setBottomRight(parentRect.bottomRight() - QPoint(4, 4));

            if (option.state & QStyle::State_Selected) {
                p->fillRect(option.rect, option.palette.highlight());
            }
            else {
                p->fillRect(option.rect, option.palette.window());
            }
            p->eraseRect(parentRect);
        }
        else {
            //draw the child items
            int childNum = index.row();
            QString data = index.data().toString();

            switch (childNum)
            {
            case StoryboardItem::FrameNumber:
            {
                if (m_view->thumbnailIsVisible()) {
                    QRect frameNumRect = option.rect;
                    frameNumRect.setHeight(m_view->fontMetrics().height()+3);
                    frameNumRect.setWidth(3 * m_view->fontMetrics().horizontalAdvance("0") + 2);
                    frameNumRect.moveBottom(option.rect.top()-1);
                    p->setPen(QPen(option.palette.dark(), 2));
                    p->drawRect(frameNumRect);
                    p->setPen(QPen(option.palette.text(), 1));
                    p->drawText(frameNumRect, Qt::AlignHCenter | Qt::AlignVCenter, data);

                    if (!m_imageSize.isEmpty()) {
                        float scale = qMin(option.rect.height() / (float)m_imageSize.height(), (float)option.rect.width() / m_imageSize.width());
                        QRect thumbnailRect = option.rect;
                        thumbnailRect.setSize(m_imageSize * scale);
                        thumbnailRect.moveCenter(option.rect.center());

                        QPixmap  thumbnailPixmap= index.data(Qt::UserRole).value<QPixmap>();
                        p->drawPixmap(thumbnailRect, thumbnailPixmap);
                    }
                    p->setPen(QPen(option.palette.dark(), 2));
                    p->drawRect(option.rect);

                    QRect buttonsRect = option.rect;
                    buttonsRect.setTop(option.rect.bottom() - 22);

                    buttonsRect.setWidth(22);
                    buttonsRect.moveBottomLeft(option.rect.bottomLeft());
                    QIcon addIcon = KisIconUtils::loadIcon("list-add");
                    p->fillRect(buttonsRect, option.palette.window());
                    addIcon.paint(p, buttonsRect);

                    buttonsRect.moveBottomRight(option.rect.bottomRight());
                    QIcon deleteIcon = KisIconUtils::loadIcon("edit-delete");
                    p->fillRect(buttonsRect, option.palette.window());
                    deleteIcon.paint(p, buttonsRect);
                }
                else {
                    QRect frameNumRect = option.rect;
                    p->setPen(QPen(option.palette.dark(), 2));
                    p->drawRect(frameNumRect);
                    p->setPen(QPen(option.palette.text(), 1));
                    p->drawText(frameNumRect, Qt::AlignHCenter | Qt::AlignVCenter, data);
                }
                break;
            }
            case StoryboardItem::ItemName:
            {
                QRect itemNameRect = option.rect;
                itemNameRect.setLeft(option.rect.left() + 5);
                p->setPen(QPen(option.palette.text(), 1));
                p->drawText(itemNameRect, Qt::AlignLeft | Qt::AlignVCenter, data);
                p->setPen(QPen(option.palette.dark(), 2));
                p->drawRect(option.rect);
                break;
            }
            case StoryboardItem::DurationSecond:
            {
                drawSpinBox(p, option, data, i18nc("suffix in spin box in storyboard that means 'seconds'", "s"));
                break;
            }
            case StoryboardItem::DurationFrame:
            {
                drawSpinBox(p, option, data, i18nc("suffix in spin box in storyboard that means 'frames'", "f"));
                break;
            }
            default:
            {
                KIS_SAFE_ASSERT_RECOVER_RETURN(index.model());
                const StoryboardModel* model = dynamic_cast<const StoryboardModel*>(index.model());
                KIS_SAFE_ASSERT_RECOVER_RETURN(model);
                if (m_view->commentIsVisible() && model->getComment(index.row() - 4).visibility) {
                    p->setPen(QPen(option.palette.dark(), 2));
                    drawCommentHeader(p, option, index);
                }
                break;
            }
            }
        }
    }
    p->restore();
}

void StoryboardDelegate::drawSpinBox(QPainter *p, const QStyleOptionViewItem &option, QString data, QString suffix) const
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QStyleOptionSpinBox spinBoxOption;
    spinBoxOption.stepEnabled = QAbstractSpinBox::StepDownEnabled | QAbstractSpinBox::StepUpEnabled;
    spinBoxOption.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown;
    spinBoxOption.rect = option.rect;
    p->setPen(QPen(option.palette.dark(), 2));
    p->drawRect(option.rect);
    style->drawComplexControl(QStyle::CC_SpinBox, &spinBoxOption, p, option.widget);

    QRect rect = style->subControlRect(QStyle::CC_SpinBox, &spinBoxOption,
                                       QStyle::QStyle::SC_SpinBoxEditField);
    rect.moveTopLeft(option.rect.topLeft());
    p->setPen(QPen(option.palette.text(), 1));
    p->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, data + suffix);
}

QStyleOptionSlider StoryboardDelegate::drawCommentHeader(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(index.model(), QStyleOptionSlider());
    const StoryboardModel* model = dynamic_cast<const StoryboardModel*>(index.model());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(model, QStyleOptionSlider());
    QString data = index.data().toString();

    QRect titleRect = option.rect;
    titleRect.setHeight(option.fontMetrics.height() + 3);
    if (p) {
        p->setPen(QPen(option.palette.text(), 1));
        p->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, model->getComment(index.row() - 4).name);
        p->setPen(QPen(option.palette.dark(), 2));
        p->drawRect(titleRect);
    }

    QRect contentRect = option.rect;
    contentRect.setTop(option.rect.top() + option.fontMetrics.height() + 3);
    if (p) {
        p->setPen(QPen(option.palette.dark(), 2));
        p->drawRect(contentRect);
        p->save();
    }
    contentRect.setTopLeft(contentRect.topLeft() + QPoint(5, 5));
    contentRect.setBottomRight(contentRect.bottomRight() - QPoint(5, 5));

    int scrollValue = index.data(Qt::UserRole).toInt();

    //draw comment
    QRect commentRect = contentRect;
    commentRect.setRight(contentRect.right() - 15);
    QTextDocument doc;

    doc.setTextWidth(commentRect.width());
    doc.setDocumentMargin(0);
    doc.setDefaultFont(option.font);
    QStringList lines = data.split('\n');
    QString HTML;
    Q_FOREACH( const QString& line, lines) {
        HTML.append("<p>" + line + "</p>");
    }
    doc.setHtml(HTML);
    QRectF clipRect = commentRect;
    clipRect.moveTopLeft(QPoint(0, 0 + scrollValue));
    if (p) {
        p->translate(QPoint(commentRect.topLeft().x(), commentRect.topLeft().y() - scrollValue));
        p->setPen(QPen(option.palette.text(), 1));
        doc.drawContents(p, clipRect);
        p->restore();
    }
    //draw scroll bar
    QStyleOptionSlider scrollbarOption;
    scrollbarOption.sliderPosition = scrollValue;
    scrollbarOption.minimum = 0;
    scrollbarOption.maximum = qMax(0.0, doc.size().height() - contentRect.height());
    scrollbarOption.sliderPosition = qMin(scrollValue, scrollbarOption.maximum);
    scrollbarOption.pageStep = contentRect.height() - 2;
    scrollbarOption.orientation = Qt::Vertical;

    QRect scrollRect = option.rect;
    scrollRect.setSize(QSize(15, option.rect.height() - option.fontMetrics.height() - 3));
    scrollRect.moveTopLeft(QPoint(0, 0));
    scrollbarOption.rect = scrollRect;

    if (p && scrollbarOption.pageStep <= doc.size().height()) {
        p->save();
        p->setPen(QPen(option.palette.dark(), 2));
        p->translate(QPoint( option.rect.right()-15, option.rect.top() + option.fontMetrics.height() + 3));
        style->drawComplexControl(QStyle::CC_ScrollBar, &scrollbarOption, p, option.widget);
        p->restore();
    }
    return scrollbarOption;
}

QSize StoryboardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    if (!index.parent().isValid()) {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(index.model(), option.rect.size());
        if (m_view->itemOrientation() == Qt::Vertical) {
            int width = option.widget->width() - 17;
            const StoryboardModel* model = dynamic_cast<const StoryboardModel*>(index.model());
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(model, option.rect.size());
            int numComments = model->visibleCommentCount();
            int numItem = width/250;
            if (numItem <= 0) {
                numItem = 1;
            }

            int thumbnailheight = m_view->thumbnailIsVisible() ? 120 : 0;
            int commentHeight = m_view->commentIsVisible() ? numComments*100 : 0;
            return QSize(width / numItem, thumbnailheight  + option.fontMetrics.height() + 3 + commentHeight + 10);
        }
        else {
            const StoryboardModel* model = dynamic_cast<const StoryboardModel*>(index.model());
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(model, option.rect.size());
            int numComments = model->visibleCommentCount();
            int commentWidth = 0;
            if (numComments && m_view->commentIsVisible()) {
                commentWidth = qMax(200, (m_view->viewport()->width() - 250) / numComments);
            }
            int width = 250 + numComments * commentWidth;
            return QSize(width + 10, 120 + option.fontMetrics.height() + 3 + 10);
        }
    }
    else {
        return option.rect.size();
    }
}

QWidget *StoryboardDelegate::createEditor(QWidget *parent,
                                          const QStyleOptionViewItem &option ,
                                          const QModelIndex &index) const
{
    Q_UNUSED(option);
    //only create editor for children
    if (index.parent().isValid()) {
        int row = index.row();
        switch (row)
        {
        case StoryboardItem::FrameNumber:
            return nullptr;
        case StoryboardItem::ItemName:
        {
            QLineEdit *editor = new QLineEdit(parent);
            return editor;
        }
        case StoryboardItem::DurationSecond:
        {
            QSpinBox *spinbox = new QSpinBox(parent);
            spinbox->setRange(0, 999);
            spinbox->setSuffix(i18nc("suffix in spin box in storyboard that means 'seconds'", "s"));
            return spinbox;
        }
        case StoryboardItem::DurationFrame:
        {
            QSpinBox *spinbox = new QSpinBox(parent);
            spinbox->setRange(0, 99);
            spinbox->setSuffix(i18nc("suffix in spin box in storyboard that means 'frames'", "f"));
            return spinbox;
        }
        default:              //for comments
        {
            QTextEdit *editor = new LimitedTextEditor(280, parent);
            return editor;
        }
        }
    }
    return nullptr;
}

bool StoryboardDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(model, false);
    if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
            && (index.flags() & Qt::ItemIsEnabled))
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        const bool leftButton = mouseEvent->buttons() & Qt::LeftButton;

        //handle the duration edit event
        if (index.parent().isValid() && (index.row() == StoryboardItem::DurationSecond || index.row() == StoryboardItem::DurationFrame)) {
            QRect upButton = spinBoxUpButton(option);
            QRect downButton = spinBoxDownButton(option);

            bool upButtonClicked = upButton.isValid() && upButton.contains(mouseEvent->pos());
            bool downButtonClicked = downButton.isValid() && downButton.contains(mouseEvent->pos());

            StoryboardModel* sbModel = dynamic_cast<StoryboardModel*>(model);
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(sbModel, false);
            if (leftButton && upButtonClicked) {
                KisStoryboardChildEditCommand *cmd = new KisStoryboardChildEditCommand(index.data(),
                                                                                    index.data().toInt() + 1,
                                                                                    index.parent().row(),
                                                                                    index.row(),
                                                                                    sbModel);
                if (sbModel->setData(index, index.data().toInt() + 1)) {
                    sbModel->pushUndoCommand(cmd);
                }
                return true;
            }
            else if (leftButton && downButtonClicked) {
                KisStoryboardChildEditCommand *cmd = new KisStoryboardChildEditCommand(index.data(),
                                                                                    index.data().toInt() - 1,
                                                                                    index.parent().row(),
                                                                                    index.row(),
                                                                                    sbModel);
                if (sbModel->setData(index, index.data().toInt() - 1)) {
                    sbModel->pushUndoCommand(cmd);
                }
                return true;
            }
        }
        else if (index.parent().isValid() && index.row() >= StoryboardItem::Comments) {
            QStyleOptionSlider scrollBarOption = drawCommentHeader(nullptr, option, index);
            QRect upButton = scrollUpButton(option, scrollBarOption);
            QRect downButton = scrollDownButton(option, scrollBarOption);

            bool upButtonClicked = upButton.isValid() && upButton.contains(mouseEvent->pos());
            bool downButtonClicked = downButton.isValid() && downButton.contains(mouseEvent->pos());

            if (leftButton && upButtonClicked) {
                int lastValue = model->data(index, Qt::UserRole).toInt();
                int value = lastValue - option.fontMetrics.height();
                StoryboardModel* modelSB = dynamic_cast<StoryboardModel*>(model);
                KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(modelSB, false);
                modelSB->setCommentScrollData(index, qMax(0, value));
                return true;
            }
            else if (leftButton && downButtonClicked) {
                int lastValue = model->data(index, Qt::UserRole).toInt();
                int value = lastValue + option.fontMetrics.height();
                StoryboardModel* modelSB = dynamic_cast<StoryboardModel*>(model);
                KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(modelSB, false);
                modelSB->setCommentScrollData(index, qMin(scrollBarOption.maximum, value));
                return true;
            }
        }

        else if (index.parent().isValid() && index.row() == StoryboardItem::FrameNumber && m_view->thumbnailIsVisible()) {     //thumbnail add/delete events
            QRect addItemButton(QPoint(0, 0), QSize(22, 22));
            addItemButton.moveBottomLeft(option.rect.bottomLeft());

            QRect deleteItemButton(QPoint(0, 0), QSize(22, 22));
            deleteItemButton.moveBottomRight(option.rect.bottomRight());

            bool addItemButtonClicked = addItemButton.isValid() && addItemButton.contains(mouseEvent->pos());
            bool deleteItemButtonClicked = deleteItemButton.isValid() && deleteItemButton.contains(mouseEvent->pos());

            StoryboardModel* sbModel = dynamic_cast<StoryboardModel*>(model);
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(sbModel, false);
            if (leftButton && addItemButtonClicked) {
                sbModel->insertItem(index.parent(), true);
                return true;
            }
            else if (leftButton && deleteItemButtonClicked) {
                int row = index.parent().row();
                KisRemoveStoryboardCommand *command = new KisRemoveStoryboardCommand(row, sbModel->getData().at(row), sbModel);

                sbModel->removeItem(index.parent(), command);
                sbModel->pushUndoCommand(command);
                return true;
            }
        }
    }

    if ((event->type() == QEvent::MouseMove) && (index.flags() & Qt::ItemIsEnabled)) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        const bool leftButton = mouseEvent->buttons() & Qt::LeftButton;

        QStyleOptionSlider scrollBarOption = drawCommentHeader(nullptr, option, index);
        QRect scrollBarRect = scrollBar(option, scrollBarOption);

        bool lastClickPosInScroll = scrollBarRect.isValid() && scrollBarRect.contains(m_lastDragPos);
        bool currClickPosInScroll = scrollBarRect.isValid() && scrollBarRect.contains(mouseEvent->pos());

        if (leftButton && index.parent().isValid() && index.row() >= StoryboardItem::Comments) {
            if (lastClickPosInScroll && currClickPosInScroll) {
                int lastValue = model->data(index, Qt::UserRole).toInt();
                int value = lastValue + mouseEvent->pos().y() - m_lastDragPos.y();

                StoryboardModel* modelSB = dynamic_cast<StoryboardModel*>(model);
                KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(modelSB, false);
                if (value >= 0 && value <= scrollBarOption.maximum) {
                    modelSB->setCommentScrollData(index, value);
                    return true;
                }
                return false;
            }
            m_lastDragPos = mouseEvent->pos();
        }
    }

    return false;
}

//set the existing data in the editor
void StoryboardDelegate::setEditorData(QWidget *editor,
                                       const QModelIndex &index) const
{
    QVariant value = index.data();
    if (index.parent().isValid()) {
        int row = index.row();
        switch (row)
        {
        case StoryboardItem::FrameNumber:             //frame thumbnail is uneditable
            return;
        case StoryboardItem::ItemName:
        {
            QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
            lineEdit->setText(value.toString());
            return;
        }
        case StoryboardItem::DurationSecond:
        case StoryboardItem::DurationFrame:
        {
            QSpinBox *spinbox = static_cast<QSpinBox*>(editor);
            spinbox->setValue(value.toInt());
            return;
        }
        default:             // for comments
        {
            QTextEdit *textEdit = static_cast<QTextEdit*>(editor);
            textEdit->setText(value.toString());
            textEdit->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
            textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            textEdit->verticalScrollBar()->setProperty("index", index);
            connect(textEdit->verticalScrollBar(), SIGNAL(sliderMoved(int)), this, SLOT(slotCommentScrolledTo(int)));
            return;
        }
        }
    }
}

void StoryboardDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                      const QModelIndex &index) const
{
    KIS_ASSERT(model);
    QVariant value = index.data();
    if (index.parent().isValid()) {
        int row = index.row();
        switch (row)
        {
        case StoryboardItem::FrameNumber:             //frame thumbnail is uneditable
            return;
        case StoryboardItem::ItemName:
        {
            QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
            QString value = lineEdit->text();
            model->setData(index, value, Qt::EditRole);
            return;
        }
        case StoryboardItem::DurationSecond:
        case StoryboardItem::DurationFrame:
        {
            QSpinBox *spinbox = static_cast<QSpinBox*>(editor);
            int value = spinbox->value();

            StoryboardModel* sbModel = dynamic_cast<StoryboardModel*>(model);
            KIS_SAFE_ASSERT_RECOVER_RETURN(sbModel);
            KisStoryboardChildEditCommand *cmd = new KisStoryboardChildEditCommand(index.data(),
                                                                                    value,
                                                                                    index.parent().row(),
                                                                                    index.row(),
                                                                                    sbModel);
            if (sbModel->setData(index, value)) {
                sbModel->pushUndoCommand(cmd);
            }
            return;
        }
        default:             // for comments
        {
            QTextEdit *textEdit = static_cast<QTextEdit*>(editor);
            QString value = textEdit->toPlainText();

            StoryboardModel* sbModel = dynamic_cast<StoryboardModel*>(model);
            KIS_SAFE_ASSERT_RECOVER_RETURN(sbModel);
            KisStoryboardChildEditCommand *cmd = new KisStoryboardChildEditCommand(index.data(),
                                                                                   value,
                                                                                   index.parent().row(),
                                                                                   index.row(),
                                                                                   sbModel);

            if (sbModel->setData(index, value)) {
                sbModel->pushUndoCommand(cmd);
            }

            return;
        }
        }
    }
}

void StoryboardDelegate::updateEditorGeometry(QWidget *editor,
                                              const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.row() < StoryboardItem::Comments) {
        editor->setGeometry(option.rect);
    }
    else {                                                //for comment textedits
        QRect commentRect = option.rect;
        commentRect.setTop(option.rect.top() + option.fontMetrics.height() + 3);
        editor->setGeometry(commentRect);
    }
}

void StoryboardDelegate::setView(StoryboardView *view)
{
    m_view = view;
}

QRect StoryboardDelegate::spinBoxUpButton(const QStyleOptionViewItem &option)
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QStyleOptionSpinBox spinOption;
    spinOption.rect = option.rect;
    QRect rect = style->subControlRect(QStyle::CC_SpinBox, &spinOption,
                                       QStyle::QStyle::SC_SpinBoxUp);
    rect.moveTopRight(option.rect.topRight());
    return rect;
}

QRect StoryboardDelegate::spinBoxDownButton(const QStyleOptionViewItem &option)
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QStyleOptionSpinBox spinOption;
    spinOption.rect = option.rect;
    QRect rect = style->subControlRect(QStyle::CC_SpinBox, &spinOption,
                                       QStyle::QStyle::SC_SpinBoxDown);
    rect.moveBottomRight(option.rect.bottomRight());
    return rect;
}

QRect StoryboardDelegate::spinBoxEditField(const QStyleOptionViewItem &option)
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QStyleOptionSpinBox spinOption;
    spinOption.rect = option.rect;
    QRect rect = style->subControlRect(QStyle::CC_SpinBox, &spinOption,
                                       QStyle::QStyle::SC_SpinBoxEditField);
    rect.moveTopLeft(option.rect.topLeft());
    return rect;
}

void StoryboardDelegate::slotCommentScrolledTo(int value) const
{
    const QModelIndex index = sender()->property("index").toModelIndex();
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_view->model());
    StoryboardModel* model = dynamic_cast<StoryboardModel*>(m_view->model());
    KIS_SAFE_ASSERT_RECOVER_RETURN(model);
    model->setCommentScrollData(index, value);
}

QRect StoryboardDelegate::scrollBar(const QStyleOptionViewItem &option, QStyleOptionSlider &scrollBarOption) const
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QRect rect = style->subControlRect(QStyle::CC_ScrollBar, &scrollBarOption,
                                       QStyle::QStyle::SC_ScrollBarSlider);
    rect.moveTopLeft(rect.topLeft() + scrollBarOption.rect.topLeft());
    rect.moveTopLeft(rect.topLeft() + option.rect.bottomRight() - scrollBarOption.rect.bottomRight());
    return rect;
}

QRect StoryboardDelegate::scrollDownButton(const QStyleOptionViewItem &option, QStyleOptionSlider &scrollBarOption)
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QRect rect = style->subControlRect(QStyle::CC_ScrollBar, &scrollBarOption,
                                       QStyle::QStyle::SC_ScrollBarAddLine);
    rect.moveTopLeft(rect.topLeft() + scrollBarOption.rect.topLeft());
    rect.moveBottomRight(option.rect.bottomRight());
    return rect;
}

QRect StoryboardDelegate::scrollUpButton(const QStyleOptionViewItem &option, QStyleOptionSlider &scrollBarOption)
{
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    QRect rect = style->subControlRect(QStyle::CC_ScrollBar, &scrollBarOption,
                                       QStyle::QStyle::SC_ScrollBarSubLine);
    rect.moveTopLeft(rect.topLeft() + scrollBarOption.rect.topLeft());
    rect.moveTop(option.rect.bottom() - scrollBarOption.rect.height());
    rect.moveRight(option.rect.right());
    return rect;
}

void StoryboardDelegate::setImageSize(QSize imageSize)
{
    m_imageSize = imageSize;
}

bool StoryboardDelegate::isOverlappingActionIcons(const QRect &rect, const QMouseEvent *event)
{
    QRect addItemButton(QPoint(0, 0), QSize(22, 22));
    addItemButton.moveBottomLeft(rect.bottomLeft());

    QRect deleteItemButton(QPoint(0, 0), QSize(22, 22));
    deleteItemButton.moveBottomRight(rect.bottomRight());

    bool addItemButtonHover = addItemButton.isValid() && addItemButton.contains(event->pos());
    bool deleteItemButtonHover = deleteItemButton.isValid() && deleteItemButton.contains(event->pos());

    return addItemButtonHover || deleteItemButtonHover;
}

bool StoryboardDelegate::eventFilter(QObject *editor, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* kEvent = static_cast<QKeyEvent*>(event);
        QTextEdit* textEditor = qobject_cast<QTextEdit*>(editor);
        if (textEditor && kEvent->key() == Qt::Key_Escape) {
            Q_EMIT commitData(textEditor);
            Q_EMIT closeEditor(textEditor, QAbstractItemDelegate::SubmitModelCache);
            return true;
        }
    }
    QStyledItemDelegate::eventFilter(editor, event);
    return false;
}
