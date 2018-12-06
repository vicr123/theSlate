#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    //Set up colours depending on palette

    QColor background = QApplication::palette("TextEditor").color(QPalette::Window);
    int avg = (background.blue() + background.green() + background.red()) / 3;
    if (avg > 127) {
        keywordFormat.setForeground(QColor(255, 150, 0));
        classFormat.setForeground(Qt::darkMagenta);
        commentFormat.setForeground(Qt::gray);
        quotationFormat.setForeground(Qt::red);
        functionFormat.setForeground(Qt::blue);
        preprocessorFormat.setForeground(QColor(150, 0, 0));
        controlFormat.setForeground(Qt::blue);
        numberFormat.setForeground(QColor(200, 0, 0));
    } else {
        keywordFormat.setForeground(QColor(255, 150, 0));
        classFormat.setForeground(Qt::magenta);
        commentFormat.setForeground(Qt::gray);
        quotationFormat.setForeground(QColor(255, 100, 100));
        functionFormat.setForeground(QColor(0, 100, 255));
        preprocessorFormat.setForeground(QColor(150, 0, 0));
        controlFormat.setForeground(QColor(0, 100, 255));
        numberFormat.setForeground(QColor(255, 0, 0));
    }
    controlFormat.setFontWeight(90);
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
    for (HighlightingRule rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

void SyntaxHighlighter::setCodeType(codeType type) {
    highlightingRules.clear();

    HighlightingRule rule;
    switch (type) {
        case js: {
            break;
        }
        case xml: {
            //Tag (class)
            rule.pattern = QRegularExpression("<.+?>");
            rule.format = classFormat;
            highlightingRules.append(rule);

            //Attribute (function)
            rule.pattern = QRegularExpression("(?<= ).+?(?=\")");
            rule.format = functionFormat;
            highlightingRules.append(rule);
            break;
        }
        case md: {

        }
        case py: {
            //Keywords
            QStringList keywordPatterns;
            keywordPatterns << "\\bdef\\b" << "\\bclass\\b" << "\\bglobal\\b";
            for (QString pattern : keywordPatterns) {
                rule.pattern = QRegularExpression(pattern);
                rule.format = keywordFormat;
                highlightingRules.append(rule);
            }

            //Controls
            QStringList controlPatterns;
            controlPatterns << "\\bif\\b" << "\\bwhile\\b" << "\\belif\\b"
                            << "\\bfor\\b" << "\\btry\\b" << "\\bexcept\\b"
                            << "\\belse\\b" << "\\bfinally\\b";
            for (QString pattern : controlPatterns) {
                rule.pattern = QRegularExpression(pattern);
                rule.format = controlFormat;
                highlightingRules.append(rule);
            }

            //Class
            rule.pattern = QRegularExpression("\\b[A-Za-z]+(?=(\\.|:))\\b");
            rule.format = classFormat;
            highlightingRules.append(rule);

            //String
            rule.pattern = QRegularExpression("\".*\"");
            rule.format = quotationFormat;
            highlightingRules.append(rule);

            rule.pattern = QRegularExpression("'.*'");
            highlightingRules.append(rule);

            //Function
            rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
            rule.format = functionFormat;
            highlightingRules.append(rule);

            //Comments
            rule.pattern = QRegularExpression("#(.)*");
            rule.format = commentFormat;
            highlightingRules.append(rule);

            commentStartExpression = QRegularExpression("\"\"\"");
            commentEndExpression = QRegularExpression("\"\"\"");
        }
        case json: {
        }
    }

    rehighlight();
    ct = type;
}

SyntaxHighlighter::codeType SyntaxHighlighter::currentCodeType() {
    return ct;
}
