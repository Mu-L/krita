/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSVGTEXTSHAPEMARKUPCONVERTER_H
#define KOSVGTEXTSHAPEMARKUPCONVERTER_H

#include "kritaflake_export.h"

#include <QScopedPointer>
#include <QTextDocument>
#include <QTextCharFormat>

#include <optional>

class QRectF;
class KoSvgTextShape;

/**
 * KoSvgTextShapeMarkupConverter is a utility class for converting a
 * KoSvgTextShape to/from user-editable markup/svg representation.
 *
 * Please note that the converted SVG is **not** the same as when saved into
 * .kra! Some attributes are dropped to make the editing is easier for the
 * user.
 */
class KRITAFLAKE_EXPORT KoSvgTextShapeMarkupConverter
{
public:
    KoSvgTextShapeMarkupConverter(KoSvgTextShape *shape);
    ~KoSvgTextShapeMarkupConverter();

    /**
     * Convert the text shape into two strings: text and styles. Styles string
     * is non-empty only when the text has some gradient/pattern attached. It is
     * intended to be places into a separate tab in the GUI.
     *
     * @return true on success
     */
    bool convertToSvg(QString *svgText, QString *stylesText);

    /**
     * @brief upload the svg representation of text into the shape
     * @param svgText \<text\> part of SVG
     * @param stylesText \<defs\> part of SVG (used only for gradients and patterns)
     * @param boundsInPixels bounds of the entire image in pixel. Used for parsing percentage units.
     * @param pixelsPerInch resolution of the image where we load the shape to
     *
     * @return true if the text was parsed successfully. Check `errors()` and `warnings()` for details.
     */
    bool convertFromSvg(const QString &svgText, const QString &stylesText, const QRectF &boundsInPixels, qreal pixelsPerInch);

    /**
     * @brief convertToHtml convert the text in the text shape to html
     * @param htmlText will be filled with correct html representing the text in the shape
     * @return @c true on success
     */
    bool convertToHtml(QString *htmlText);

    /**
     * @brief convertFromHtml converted Qt rich text html (and no other: https://doc.qt.io/qt-5/richtext-html-subset.html) to SVG
     * @param htmlText the input html
     * @param svgText the converted svg text element
     * @param styles
     * @return @c true if the conversion was successful
     */
    bool convertFromHtml(const QString &htmlText, QString *svgText, QString *styles);

    /**
     * @brief convertDocumentToSvg
     * @param doc the QTextDocument to convert.
     * @param svgText the converted svg text element
     * @return @c true if the conversion was successful
     */
    bool convertDocumentToSvg(const QTextDocument *doc, QString *svgText);

    /**
     * @brief convertSvgToDocument
     * @param svgText the \<text\> element and it's children as a string.
     * @param doc the QTextDocument that the conversion is written to.
     * @return @c true if the conversion was successful
     */
    bool convertSvgToDocument(const QString &svgText, QTextDocument *doc);


    /**
     * A list of errors happened during loading the user's text
     */
    QStringList errors() const;

    /**
     * A list of warnings produced during loading the user's text
     */
    QStringList warnings() const;

    /**
     * @brief style
     * creates a style string based on the blockformat and the format.
     * @param format the textCharFormat of the current text.
     * @param blockFormat the block format of the current text.
     * @param mostCommon the most common format to compare the format to.
     * @param includeLineHeight whether the style should include `line-height`.
     * @return a string that can be written into a style element.
     */
    QString style(QTextCharFormat format, QTextBlockFormat blockFormat, QTextCharFormat mostCommon = QTextCharFormat(), bool includeLineHeight = false);

    struct ExtraStyles;

    /**
     * @brief stylesFromString
     * returns a qvector with two textformats:
     * at 0 is the QTextCharFormat
     * at 1 is the QTextBlockFormat
     * @param styles a style string split at ";"
     * @param currentCharFormat the current charformat to compare against.
     * @param currentBlockFormat the current blockformat to compare against.
     * @param[out] extraStyles other styles.
     * @return A QVector with at 0 a QTextCharFormat and at 1 a QBlockCharFormat.
     */
    static QVector<QTextFormat> stylesFromString(QStringList styles, QTextCharFormat currentCharFormat, QTextBlockFormat currentBlockFormat, ExtraStyles &extraStyles);
    /**
     * @brief formatDifference
     * A class to get the difference between two text-char formats.
     * @param test the format to test
     * @param reference the format to test against.
     * @return the difference between the two.
     */
    QTextFormat formatDifference(QTextFormat test, QTextFormat reference);

private:
    struct Private;
    const QScopedPointer<Private> d;

public:
    /*
     * Members for dealing with special document-level properties not supported
     * by Qt: We abuse QTextFormat a bit to store our special properties in the
     * top-level root QTextFrame of the QTextDocument.
     */

    /**
     * The wrapping mode for the conversion between QTextDocument and SVG.
     */
    enum class WrappingMode {
        /**
         * Use the legacy conversion with line breaks performed by setting `dy`
         * on `<tspan>` with offsets calculated from `QTextLayout`.
         */
        QtLegacy = 0,
        /**
         * Use `white-space: pre` and output hard line-breaks in the SVG.
         */
        WhiteSpacePre,
        /**
         * Use `white-space: pre-wrap` and output hard line-breaks in the SVG.
         * `inline-size` is also set to enable automatic wrapping.
         */
        WhiteSpacePreWrap,
    };

    static constexpr QTextFormat::Property WrappingModeProperty =
        static_cast<QTextFormat::Property>(QTextFormat::UserProperty + 56784);
    static constexpr QTextFormat::Property InlineSizeProperty =
        static_cast<QTextFormat::Property>(WrappingModeProperty + 1);

    /**
     * @brief Get the Wrapping Mode from the frameFormat of the rootFrame of a
     * QTextDocument
     * 
     * @param frameFormat
     * @return WrappingMode if set, or the default value WrappingMode::QtLegacy
     */
    static WrappingMode getWrappingMode(const QTextFrameFormat &frameFormat);
    /**
     * @brief Set the Wrapping Mode on a frame format to be applied to the
     * rootFrame of a QTextDocument
     * 
     * @param frameFormat pointer to the franeFormat to be modified
     * @param wrappingMode
     */
    static void setWrappingMode(QTextFrameFormat *frameFormat, WrappingMode wrappingMode);

    /**
     * @brief Get the inline-size from the frameFormat of the rootFrame of a
     * QTextDocument
     * 
     * @param frameFormat
     * @return std::optional<double> the inline-size if set, or std::nullopt
     */
    static std::optional<double> getInlineSize(const QTextFrameFormat &frameFormat);
    /**
     * @brief Set or unset the inline-size on a frameFormat to be applied to
     * the rootFrame of a QTextDocument
     * 
     * @param frameFormat pointer to the frameFormat to be modified
     * @param inlineSize the inline-size; pass a negative value to unset the
     * property
     */
    static void setInlineSize(QTextFrameFormat *frameFormat, double inlineSize);
};

#endif // KOSVGTEXTSHAPEMARKUPCONVERTER_H
