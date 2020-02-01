// QCodeEditor
#include <QCXXHighlighter>
#include <QCodeEditor>
#include <QLineNumberArea>
#include <QPythonHighlighter>
#include <QStyleSyntaxHighlighter>
#include <QSyntaxStyle>

// Qt
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QCompleter>
#include <QCursor>
#include <QFontDatabase>
#include <QMimeData>
#include <QPaintEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextStream>

static QVector<QPair<QString, QString>> parentheses = {{"(", ")"}, {"{", "}"}, {"[", "]"}, {"\"", "\""}, {"'", "'"}};

QCodeEditor::QCodeEditor(QWidget *widget)
    : QTextEdit(widget), m_highlighter(nullptr), m_syntaxStyle(nullptr), m_lineNumberArea(new QLineNumberArea(this)),
      m_completer(nullptr), m_autoIndentation(true), m_autoParentheses(true), m_replaceTab(true),
      m_tabReplace(QString(4, ' ')), extra1(), extra2()
{
    initFont();
    performConnections();

    setSyntaxStyle(QSyntaxStyle::defaultStyle());
}

void QCodeEditor::initFont()
{
    auto fnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fnt.setFixedPitch(true);
    fnt.setPointSize(10);

    setFont(fnt);
}

void QCodeEditor::performConnections()
{
    connect(document(), &QTextDocument::blockCountChanged, this, &QCodeEditor::updateLineNumberAreaWidth);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, [this](int) { m_lineNumberArea->update(); });

    connect(this, &QTextEdit::cursorPositionChanged, this, &QCodeEditor::updateExtraSelection1);
    connect(this, &QTextEdit::selectionChanged, this, &QCodeEditor::updateExtraSelection2);
}

void QCodeEditor::setHighlighter(QStyleSyntaxHighlighter *highlighter)
{
    if (m_highlighter)
    {
        m_highlighter->setDocument(nullptr);
    }

    m_highlighter = highlighter;

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
        m_highlighter->setDocument(document());
    }
}

void QCodeEditor::setSyntaxStyle(QSyntaxStyle *style)
{
    m_syntaxStyle = style;

    m_lineNumberArea->setSyntaxStyle(m_syntaxStyle);

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
    }

    updateStyle();
}

void QCodeEditor::updateStyle()
{
    if (m_highlighter)
    {
        m_highlighter->rehighlight();
    }

    if (m_syntaxStyle)
    {
        auto currentPalette = palette();

        // Setting text format/color
        currentPalette.setColor(QPalette::ColorRole::Text, m_syntaxStyle->getFormat("Text").foreground().color());

        // Setting common background
        currentPalette.setColor(QPalette::Base, m_syntaxStyle->getFormat("Text").background().color());

        // Setting selection color
        currentPalette.setColor(QPalette::Highlight, m_syntaxStyle->getFormat("Selection").background().color());

        setPalette(currentPalette);
    }

    updateExtraSelection1();
    updateExtraSelection2();
}

void QCodeEditor::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);

    updateLineGeometry();
}

void QCodeEditor::updateLineGeometry()
{
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), m_lineNumberArea->sizeHint().width(), cr.height()));
}

void QCodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(m_lineNumberArea->sizeHint().width(), 0, 0, 0);
}

void QCodeEditor::updateLineNumberArea(const QRect &rect)
{
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->sizeHint().width(), rect.height());
    updateLineGeometry();

    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberAreaWidth(0);
    }
}

void QCodeEditor::updateExtraSelection1()
{
    extra1.clear();

    highlightCurrentLine();
    highlightParenthesis();

    setExtraSelections(extra1 + extra2);
}

void QCodeEditor::updateExtraSelection2()
{
    extra2.clear();

    highlightOccurrences();

    setExtraSelections(extra1 + extra2);
}

void QCodeEditor::highlightParenthesis()
{
    auto currentSymbol = charUnderCursor();
    auto prevSymbol = charUnderCursor(-1);

    for (auto &pair : parentheses)
    {
        int direction;

        QChar counterSymbol;
        QChar activeSymbol;
        auto position = textCursor().position();

        if (pair.first == currentSymbol)
        {
            direction = 1;
            counterSymbol = pair.second[0];
            activeSymbol = currentSymbol;
        }
        else if (pair.second == prevSymbol)
        {
            direction = -1;
            counterSymbol = pair.first[0];
            activeSymbol = prevSymbol;
            position--;
        }
        else
        {
            continue;
        }

        auto counter = 1;

        while (counter != 0 && position > 0 && position < (document()->characterCount() - 1))
        {
            // Moving position
            position += direction;

            auto character = document()->characterAt(position);
            // Checking symbol under position
            if (character == activeSymbol)
            {
                ++counter;
            }
            else if (character == counterSymbol)
            {
                --counter;
            }
        }

        auto format = m_syntaxStyle->getFormat("Parentheses");

        // Found
        if (counter == 0)
        {
            ExtraSelection selection{};

            auto directionEnum = direction < 0 ? QTextCursor::MoveOperation::Left : QTextCursor::MoveOperation::Right;

            selection.format = format;
            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.movePosition(directionEnum, QTextCursor::MoveMode::MoveAnchor,
                                          std::abs(textCursor().position() - position));

            selection.cursor.movePosition(QTextCursor::MoveOperation::Right, QTextCursor::MoveMode::KeepAnchor, 1);

            extra1.append(selection);

            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.movePosition(directionEnum, QTextCursor::MoveMode::KeepAnchor, 1);

            extra1.append(selection);
        }

        break;
    }
}

void QCodeEditor::highlightCurrentLine()
{
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection{};

        selection.format = m_syntaxStyle->getFormat("CurrentLine");
        selection.format.setForeground(QBrush());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        extra1.append(selection);
    }
}

void QCodeEditor::highlightOccurrences()
{
    auto cursor = textCursor();
    if (cursor.hasSelection())
    {
        auto text = cursor.selectedText();
        if (QRegularExpression(
                R"((?:[_a-zA-Z][_a-zA-Z0-9]*)|(?<=\b|\s|^)(?i)(?:(?:(?:(?:(?:\d+(?:'\d+)*)?\.(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)\.(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)?\.(?:[0-9a-f]+(?:'[0-9a-f]+)*)(?:p[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)\.?(?:p[+-]?(?:\d+(?:'\d+)*))))[lf]?)|(?:(?:(?:[1-9]\d*(?:'\d+)*)|(?:0[0-7]*(?:'[0-7]+)*)|(?:0x[0-9a-f]+(?:'[0-9a-f]+)*)|(?:0b[01]+(?:'[01]+)*))(?:u?l{0,2}|l{0,2}u?)))(?=\b|\s|$))")
                .match(text)
                .captured() == text)
        {
            auto doc = document();
            cursor = doc->find(text, 0, QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
            while (!cursor.isNull())
            {
                if (cursor != textCursor())
                {
                    QTextEdit::ExtraSelection e;
                    e.cursor = cursor;
                    e.format.setFontUnderline(true);
                    extra2.push_back(e);
                }
                cursor = doc->find(text, cursor, QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
            }
        }
    }
}

void QCodeEditor::paintEvent(QPaintEvent *e)
{
    updateLineNumberArea(e->rect());
    QTextEdit::paintEvent(e);
}

int QCodeEditor::getFirstVisibleBlock()
{
    // Detect the first block for which bounding rect - once translated
    // in absolute coordinated - is contained by the editor's text area

    // Costly way of doing but since "blockBoundingGeometry(...)" doesn't
    // exists for "QTextEdit"...

    QTextCursor curs = QTextCursor(document());
    curs.movePosition(QTextCursor::Start);
    for (int i = 0; i < document()->blockCount(); ++i)
    {
        QTextBlock block = curs.block();

        QRect r1 = viewport()->geometry();
        QRect r2 = document()
                       ->documentLayout()
                       ->blockBoundingRect(block)
                       .translated(viewport()->geometry().x(),
                                   viewport()->geometry().y() - verticalScrollBar()->sliderPosition())
                       .toRect();

        if (r1.intersects(r2))
        {
            return i;
        }

        curs.movePosition(QTextCursor::NextBlock);
    }

    return 0;
}

bool QCodeEditor::proceedCompleterBegin(QKeyEvent *e)
{
    if (m_completer && m_completer->popup()->isVisible())
    {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return true; // let the completer do default behavior
        default:
            break;
        }
    }

    // todo: Replace with modifiable QShortcut
    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);

    return !(!m_completer || !isShortcut);
}

void QCodeEditor::proceedCompleterEnd(QKeyEvent *e)
{
    auto ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

    if (!m_completer || (ctrlOrShift && e->text().isEmpty()) || e->key() == Qt::Key_Delete)
    {
        return;
    }

    static QString eow(R"(~!@#$%^&*()_+{}|:"<>?,./;'[]\-=)");

    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
    auto completionPrefix = wordUnderCursor();

    if (!isShortcut && (e->text().isEmpty() || completionPrefix.length() < 2 || eow.contains(e->text().right(1))))
    {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix())
    {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    auto cursRect = cursorRect();
    cursRect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                      m_completer->popup()->verticalScrollBar()->sizeHint().width());

    m_completer->complete(cursRect);
}

void QCodeEditor::keyPressEvent(QKeyEvent *e)
{
#if QT_VERSION >= 0x050A00
    const int defaultIndent = tabStopDistance() / fontMetrics().averageCharWidth();
#else
    const int defaultIndent = tabStopWidth() / fontMetrics().averageCharWidth();
#endif

    auto completerSkip = proceedCompleterBegin(e);

    if (!completerSkip)
    {
        if (e->key() == Qt::Key_Tab && e->modifiers() == Qt::NoModifier)
        {
            if (textCursor().hasSelection())
            {
                addInEachLineOfSelection(QRegularExpression("^"), m_replaceTab ? m_tabReplace : "\t");
                return;
            }
            else if (m_replaceTab)
            {
                insertPlainText(m_tabReplace);
                return;
            }
        }

        // Auto indentation
        int indentationLevel = getIndentationSpaces();

#if QT_VERSION >= 0x050A00
        int tabCounts = indentationLevel * fontMetrics().averageCharWidth() / tabStopDistance();
#else
        int tabCounts = indentationLevel * fontMetrics().averageCharWidth() / tabStopWidth();
#endif

        // Have Qt Edior like behaviour, if {|} and enter is pressed indent the two
        // parenthesis
        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
            charUnderCursor() == '}' && charUnderCursor(-1) == '{')
        {
            int charsBack = 0;
            insertPlainText("\n");

            if (m_replaceTab)
                insertPlainText(QString(indentationLevel + defaultIndent, ' '));
            else
                insertPlainText(QString(tabCounts + 1, '\t'));

            insertPlainText("\n");
            charsBack++;

            if (m_replaceTab)
            {
                insertPlainText(QString(indentationLevel, ' '));
                charsBack += indentationLevel;
            }
            else
            {
                insertPlainText(QString(tabCounts, '\t'));
                charsBack += tabCounts;
            }

            while (charsBack--)
                moveCursor(QTextCursor::MoveOperation::Left);
            return;
        }

        // Shortcut for moving line to left
        if (e->key() == Qt::Key_Backtab)
        {
            removeInEachLineOfSelection(QRegularExpression("^(\t| {1," + QString::number(tabReplaceSize()) + "})"),
                                        true);
            return;
        }

        // Shortcut for swap line up
        if ((e->key() == Qt::Key_Up || e->key() == Qt::Key_Down) &&
            e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
        {
            auto cursor = textCursor();
            auto lines = toPlainText().remove('\r').split('\n');
            int selectionStart = cursor.selectionStart();
            int selectionEnd = cursor.selectionEnd();
            bool cursorAtEnd = cursor.position() == selectionEnd;
            cursor.setPosition(selectionStart);
            int lineStart = cursor.blockNumber();
            cursor.setPosition(selectionEnd);
            int lineEnd = cursor.blockNumber();

            if (e->key() == Qt::Key_Up)
            {
                if (lineStart == 0)
                    return;
                selectionStart -= lines[lineStart - 1].length() + 1;
                selectionEnd -= lines[lineStart - 1].length() + 1;
                lines.move(lineStart - 1, lineEnd);
            }

            if (e->key() == Qt::Key_Down)
            {
                if (lineEnd == document()->blockCount() - 1)
                    return;
                selectionStart += lines[lineEnd + 1].length() + 1;
                selectionEnd += lines[lineEnd + 1].length() + 1;
                lines.move(lineEnd + 1, lineStart);
            }

            cursor.select(QTextCursor::Document);
            cursor.insertText(lines.join('\n'));

            if (cursorAtEnd)
            {
                cursor.setPosition(selectionStart);
                cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
            }
            else
            {
                cursor.setPosition(selectionEnd);
                cursor.setPosition(selectionStart, QTextCursor::KeepAnchor);
            }

            setTextCursor(cursor);
        }

        // Shortcut for delete selected lines
        if (e->key() == Qt::Key_K && e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
        {
            auto cursor = textCursor();
            int selectionStart = cursor.selectionStart();
            int selectionEnd = cursor.selectionEnd();
            cursor.setPosition(selectionStart);
            int lineStart = cursor.blockNumber();
            cursor.setPosition(selectionEnd);
            int lineEnd = cursor.blockNumber();
            moveCursor(QTextCursor::MoveOperation::Up);
            cursor.movePosition(QTextCursor::Start);
            if (lineEnd == document()->blockCount() - 1)
            {
                if (lineStart == 0)
                {
                    cursor.select(QTextCursor::Document);
                }
                else
                {
                    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineStart - 1);
                    cursor.movePosition(QTextCursor::EndOfLine);
                    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                }
            }
            else
            {
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineStart);
                cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, lineEnd - lineStart + 1);
            }
            cursor.insertText("");
        }

        // Shortcut for toggle comments
        // Key_Question because it's Key_Slash after Shift
        if ((e->key() == Qt::Key_Slash || e->key() == Qt::Key_Question) && (e->modifiers() & Qt::ControlModifier))
        {
            bool isCpp = dynamic_cast<QCXXHighlighter *>(m_highlighter);
            bool isPython = dynamic_cast<QPythonHighlighter *>(m_highlighter);
            if (isCpp || isPython)
            {
                if (e->modifiers() == Qt::ControlModifier) // single-line comments
                {
                    QString comment = isCpp ? "//" : "#";
                    if (!removeInEachLineOfSelection(QRegularExpression("^\\s*(" + comment + " ?)"), false))
                    {
                        addInEachLineOfSelection(QRegularExpression("\\S"), comment + " ");
                    }
                    return;
                }
                if (e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) // multi-line comments
                {
                    auto cursor = textCursor();
                    int startPos = cursor.selectionStart();
                    int endPos = cursor.selectionEnd();
                    bool cursorAtEnd = cursor.position() == endPos;
                    QString commentStart = isCpp ? "/*" : "\"\"\"";
                    QString commentEnd = isCpp ? "*/" : "\"\"\"";
                    auto text = cursor.selectedText();
                    int pos1, pos2;
                    if (text.indexOf(commentStart) == 0 &&
                        text.length() >= commentStart.length() + commentEnd.length() &&
                        text.lastIndexOf(commentEnd) + commentEnd.length() == text.length())
                    {
                        insertPlainText(text.mid(commentStart.length(),
                                                 text.length() - commentStart.length() - commentEnd.length()));
                        pos1 = startPos;
                        pos2 = endPos - commentStart.length() - commentEnd.length();
                    }
                    else
                    {
                        insertPlainText(commentStart + text + commentEnd);
                        pos1 = startPos;
                        pos2 = endPos + commentStart.length() + commentEnd.length();
                    }
                    if (cursorAtEnd)
                    {
                        cursor.setPosition(pos1);
                        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
                    }
                    else
                    {
                        cursor.setPosition(pos2);
                        cursor.setPosition(pos1, QTextCursor::KeepAnchor);
                    }
                    setTextCursor(cursor);
                    return;
                }
            }
        }

        if (m_autoParentheses)
        {
            for (auto &&el : parentheses)
            {
                // Add parentheses for selection
                if (el.first == e->text())
                {
                    auto cursor = textCursor();
                    if (cursor.hasSelection())
                    {
                        int startPos = cursor.selectionStart();
                        int endPos = cursor.selectionEnd();
                        bool cursorAtEnd = cursor.position() == endPos;
                        auto text = el.first + cursor.selectedText() + el.second;
                        insertPlainText(text);
                        if (cursorAtEnd)
                        {
                            cursor.setPosition(startPos + 1);
                            cursor.setPosition(endPos + 1, QTextCursor::KeepAnchor);
                        }
                        else
                        {
                            cursor.setPosition(endPos + 1);
                            cursor.setPosition(startPos + 1, QTextCursor::KeepAnchor);
                        }
                        setTextCursor(cursor);
                        return;
                    }
                }
            }
        }

        QTextEdit::keyPressEvent(e);

        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
        {
            if (m_replaceTab)
                insertPlainText(QString(indentationLevel, ' '));
            else
                insertPlainText(QString(tabCounts, '\t'));
        }

        if (m_autoParentheses)
        {
            for (auto &&el : parentheses)
            {
                // Inserting closed brace
                if (el.first == e->text())
                {
                    insertPlainText(el.second);
                    moveCursor(QTextCursor::MoveOperation::Left);
                    break;
                }

                // If it's close brace - check parentheses
                if (el.second == e->text())
                {
                    auto symbol = charUnderCursor();

                    if (symbol == el.second)
                    {
                        textCursor().deletePreviousChar();
                        moveCursor(QTextCursor::MoveOperation::Right);
                    }

                    break;
                }
            }
        }
    }

    proceedCompleterEnd(e);
}

void QCodeEditor::setAutoIndentation(bool enabled)
{
    m_autoIndentation = enabled;
}

bool QCodeEditor::autoIndentation() const
{
    return m_autoIndentation;
}

void QCodeEditor::setAutoParentheses(bool enabled)
{
    m_autoParentheses = enabled;
}

bool QCodeEditor::autoParentheses() const
{
    return m_autoParentheses;
}

void QCodeEditor::setTabReplace(bool enabled)
{
    m_replaceTab = enabled;
}

bool QCodeEditor::tabReplace() const
{
    return m_replaceTab;
}

void QCodeEditor::setTabReplaceSize(int val)
{
    m_tabReplace.clear();

    m_tabReplace.fill(' ', val);
}

int QCodeEditor::tabReplaceSize() const
{
    return m_tabReplace.size();
}

void QCodeEditor::setCompleter(QCompleter *completer)
{
    if (m_completer)
    {
        disconnect(m_completer, nullptr, this, nullptr);
    }

    m_completer = completer;

    if (!m_completer)
    {
        return;
    }

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);

    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this, &QCodeEditor::insertCompletion);
}

void QCodeEditor::focusInEvent(QFocusEvent *e)
{
    if (m_completer)
    {
        m_completer->setWidget(this);
    }

    QTextEdit::focusInEvent(e);
}

void QCodeEditor::insertCompletion(QString s)
{
    if (m_completer->widget() != this)
    {
        return;
    }

    auto tc = textCursor();
    tc.select(QTextCursor::SelectionType::WordUnderCursor);
    tc.insertText(s);
    setTextCursor(tc);
}

QCompleter *QCodeEditor::completer() const
{
    return m_completer;
}

QChar QCodeEditor::charUnderCursor(int offset) const
{
    auto block = textCursor().blockNumber();
    auto index = textCursor().positionInBlock();
    auto text = document()->findBlockByNumber(block).text();

    index += offset;

    if (index < 0 || index >= text.size())
    {
        return {};
    }

    return text[index];
}

QString QCodeEditor::wordUnderCursor() const
{
    auto tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

void QCodeEditor::insertFromMimeData(const QMimeData *source)
{
    insertPlainText(source->text());
}

int QCodeEditor::getIndentationSpaces()
{
    auto blockText = textCursor().block().text();

    int indentationLevel = 0;

    for (auto i = 0; i < blockText.size() && QString("\t ").contains(blockText[i]); ++i)
    {
        if (blockText[i] == ' ')
        {
            indentationLevel++;
        }
        else
        {
#if QT_VERSION >= 0x050A00
            indentationLevel += tabStopDistance() / fontMetrics().averageCharWidth();
#else
            indentationLevel += tabStopWidth() / fontMetrics().averageCharWidth();
#endif
        }
    }

    return indentationLevel;
}

bool QCodeEditor::removeInEachLineOfSelection(const QRegularExpression &regex, bool force)
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();
    QString newText;
    QTextStream stream(&newText);
    int deleteTotal = 0, deleteFirst = 0;
    for (int i = lineStart; i <= lineEnd; ++i)
    {
        auto line = lines[i];
        auto match = regex.match(line).captured(1);
        int len = match.length();
        if (len == 0 && !force)
            return false;
        if (i == lineStart)
            deleteFirst = len;
        deleteTotal += len;
        stream << line.remove(line.indexOf(match), len);
        if (i != lineEnd)
            stream << endl;
    }
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineStart);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, lineEnd - lineStart);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.insertText(newText);
    cursor.setPosition(qMax(0, selectionStart - deleteFirst));
    if (cursor.blockNumber() < lineStart)
    {
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineStart - cursor.blockNumber());
        cursor.movePosition(QTextCursor::StartOfLine);
    }
    int pos = cursor.position();
    cursor.setPosition(selectionEnd - deleteTotal);
    if (cursor.blockNumber() < lineEnd)
    {
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineEnd - cursor.blockNumber());
        cursor.movePosition(QTextCursor::StartOfLine);
    }
    int pos2 = cursor.position();
    if (cursorAtEnd)
    {
        cursor.setPosition(pos);
        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(pos2);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
    return true;
}

void QCodeEditor::addInEachLineOfSelection(const QRegularExpression &regex, const QString &str)
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();
    QString newText;
    QTextStream stream(&newText);
    for (int i = lineStart; i <= lineEnd; ++i)
    {
        auto line = lines[i];
        stream << line.insert(line.indexOf(regex), str);
        if (i != lineEnd)
            stream << endl;
    }
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineStart);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, lineEnd - lineStart);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.insertText(newText);
    int pos = selectionStart + str.length();
    int pos2 = selectionEnd + str.length() * (lineEnd - lineStart + 1);
    if (cursorAtEnd)
    {
        cursor.setPosition(pos);
        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(pos2);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
}
