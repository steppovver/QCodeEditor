// QCodeEditor
#include <QCodeEditor>
#include <QLineNumberArea>
#include <QSyntaxStyle>

// Qt
#include <QAbstractTextDocumentLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>

QLineNumberArea::QLineNumberArea(QCodeEditor *parent)
    : QWidget(parent), m_syntaxStyle(nullptr), m_codeEditParent(parent), m_squiggles()
{
}

QSize QLineNumberArea::sizeHint() const
{
    if (m_codeEditParent == nullptr)
    {
        return QWidget::sizeHint();
    }

    const int digits = QString::number(m_codeEditParent->document()->blockCount()).length();
    int space;

#if QT_VERSION >= 0x050B00
    space = 4 + m_codeEditParent->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
#else
    space = 4 + m_codeEditParent->fontMetrics().width(QLatin1Char('9')) * digits;
#endif

    return {space, 0};
}

void QLineNumberArea::setSyntaxStyle(QSyntaxStyle *style)
{
    m_syntaxStyle = style;
}

QSyntaxStyle *QLineNumberArea::syntaxStyle() const
{
    return m_syntaxStyle;
}

void QLineNumberArea::lint(QCodeEditor::SeverityLevel level, int from, int to)
{
    for (int i = from - 1; i < to; ++i)
    {
        m_squiggles[i] = qMax(m_squiggles[i], level);
    }
    update();
}

void QLineNumberArea::clearLint()
{
    m_squiggles.clear();
    update();
}

void QLineNumberArea::paintEvent(QPaintEvent *event)
{
    m_codeEditParent->lineNumberAreaPaintEvent(event);
}
