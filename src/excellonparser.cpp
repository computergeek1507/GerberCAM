#include "excellonparser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <cmath>

#include "scale.h"   // PRECISIONSCALE

// ---------------------------------------------------------------------------
// holesByDiameter
// ---------------------------------------------------------------------------
QMap<qint64, QList<QPoint>> ExcellonParser::holesByDiameter() const
{
    QMap<qint64, QList<QPoint>> result;
    for (const ExcellonTool &t : tools)
    {
        if (t.diameterMm <= 0.0 || t.holes.isEmpty())
            continue;
        qint64 key = static_cast<qint64>(t.diameterMm * PRECISIONSCALE + 0.5);
        result[key].append(t.holes);
    }
    return result;
}

// ---------------------------------------------------------------------------
// parseCoord
// ---------------------------------------------------------------------------
qint64 ExcellonParser::parseCoord(const QString &token) const
{
    if (token.isEmpty())
        return 0;

    double mm = 0.0;

    if (token.contains('.'))
    {
        // Modern format: explicit decimal point — parse directly.
        double val = token.toDouble();
        mm = (m_units == Units::Inch) ? val * 25.4 : val;
    }
    else
    {
        // Fixed-format integer encoding — reconstruct from digit-count rules.
        bool negative = token.startsWith('-');
        QString digits = negative ? token.mid(1) : token;

        int totalDigits = m_intDigits + m_fracDigits;

        if (m_zeroMode == ZeroMode::TrailingZerosSuppressed)
        {
            // TZ: number is left-aligned; right-pad with zeros to full width.
            while (digits.length() < totalDigits)
                digits.append('0');
        }
        else
        {
            // LZ: number is right-aligned; left-pad with zeros to full width.
            while (digits.length() < totalDigits)
                digits.prepend('0');
        }

        double val = digits.toLongLong() / std::pow(10.0, m_fracDigits);
        if (negative) val = -val;
        mm = (m_units == Units::Inch) ? val * 25.4 : val;
    }

    // Round to nearest internal unit.
    return static_cast<qint64>(mm >= 0.0 ? mm * PRECISIONSCALE + 0.5
                                          : mm * PRECISIONSCALE - 0.5);
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------
bool ExcellonParser::parse(const QString &filePath, QString &errorMsg)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file: " + f.errorString();
        return false;
    }

    tools.clear();

    QMap<int, int> indexByNumber; // tool-number → index in tools[]

    auto getOrCreate = [&](int num) -> int {
        if (!indexByNumber.contains(num))
        {
            ExcellonTool t;
            t.number = num;
            tools.append(t);
            indexByNumber[num] = tools.size() - 1;
        }
        return indexByNumber[num];
    };

    // Regex for tool definition lines: T1C0.800 or T01F200S3000C0.8
    static const QRegularExpression reTool("^T(\\d+).*C([0-9]*\\.?[0-9]+)",
                                           QRegularExpression::CaseInsensitiveOption);
    // Regex for tool-selection lines (body): just T1 with nothing else meaningful
    static const QRegularExpression reToolSel("^T(\\d+)\\s*$");

    QTextStream stream(&f);
    bool inHeader    = false;
    bool headerSeen  = false;
    int  currentIdx  = -1;
    qint64 lastX = 0, lastY = 0;

    while (!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();

        // Skip blank lines and pure comments
        if (line.isEmpty() || line.startsWith(';'))
            continue;

        // ---------------------------------------------------------------
        // Header control
        // ---------------------------------------------------------------
        if (line == "M48")  { inHeader = true;  headerSeen = true; continue; }
        if (line == "%")    { inHeader = false;                     continue; }
        if (line == "M30" || line == "M00") break; // end-of-file marker

        // ---------------------------------------------------------------
        // Header section: unit/format/tool-definition directives
        // ---------------------------------------------------------------
        if (inHeader || !headerSeen)
        {
            QString upper = line.toUpper();

            if (upper.startsWith("METRIC"))
            {
                m_units     = Units::Metric;
                m_intDigits = 3; m_fracDigits = 3;
                if (upper.contains(",LZ")) m_zeroMode = ZeroMode::LeadingZerosSuppressed;
                else                       m_zeroMode = ZeroMode::TrailingZerosSuppressed;
            }
            else if (upper.startsWith("INCH"))
            {
                m_units     = Units::Inch;
                m_intDigits = 2; m_fracDigits = 4;
                if (upper.contains(",TZ")) m_zeroMode = ZeroMode::TrailingZerosSuppressed;
                else                       m_zeroMode = ZeroMode::LeadingZerosSuppressed;
            }
            // FMAT,2 is the default format implied by METRIC/INCH above — ignore explicitly.

            // Tool definition with diameter
            auto m = reTool.match(line);
            if (m.hasMatch())
            {
                int    num    = m.captured(1).toInt();
                double diam   = m.captured(2).toDouble();
                double diamMm = (m_units == Units::Inch) ? diam * 25.4 : diam;
                tools[getOrCreate(num)].diameterMm = diamMm;
            }

            // Skip body processing while in header
            if (inHeader)
                continue;
        }

        // ---------------------------------------------------------------
        // Body section
        // ---------------------------------------------------------------

        // Pure tool-selection line: "T1" or "T02"
        {
            auto m = reToolSel.match(line);
            if (m.hasMatch())
            {
                currentIdx = getOrCreate(m.captured(1).toInt());
                continue;
            }
        }

        // G-code lines — strip mode code, keep any X/Y that follow
        if (line.startsWith('G') || line.startsWith('M'))
        {
            // Find first X or Y in the line
            int xp = line.indexOf('X'), yp = line.indexOf('Y');
            if (xp < 0 && yp < 0)
                continue;
            // Trim the G/M prefix so the coordinate parser can handle it
            int first = (xp >= 0 && yp >= 0) ? qMin(xp, yp)
                      : (xp >= 0 ? xp : yp);
            line = line.mid(first);
        }

        // Coordinate line: optional X, optional Y (absolute positioning)
        if ((line.startsWith('X') || line.startsWith('Y')) && currentIdx >= 0)
        {
            int xp = line.indexOf('X');
            int yp = line.indexOf('Y');

            bool hasX = (xp >= 0);
            bool hasY = (yp >= 0);

            auto extractToken = [&](int start) -> QString {
                // Characters that are part of the number: digits, '.', '-', '+'
                int i = start + 1;
                while (i < line.length())
                {
                    QChar c = line[i];
                    if (c.isDigit() || c == '.' || c == '-' || c == '+')
                        ++i;
                    else
                        break;
                }
                return line.mid(start + 1, i - start - 1);
            };

            if (hasX)
            {
                // Limit X token to before Y (if Y comes after X)
                int xEnd = (hasY && yp > xp) ? yp : line.length();
                QString tok = line.mid(xp + 1, xEnd - xp - 1);
                // Trim non-numeric suffix
                int i = 0;
                while (i < tok.length() && (tok[i].isDigit() || tok[i] == '.' || tok[i] == '-' || tok[i] == '+'))
                    ++i;
                tok = tok.left(i);
                if (!tok.isEmpty())
                    lastX = parseCoord(tok);
            }

            if (hasY)
            {
                QString tok = extractToken(yp);
                if (!tok.isEmpty())
                    lastY = parseCoord(tok);
            }

            tools[currentIdx].holes.append(QPoint(static_cast<int>(lastX),
                                                   static_cast<int>(lastY)));
        }
    }

    f.close();

    // Drop tools that ended up with no holes
    tools.erase(std::remove_if(tools.begin(), tools.end(),
                               [](const ExcellonTool &t){ return t.holes.isEmpty(); }),
                tools.end());

    if (tools.isEmpty())
    {
        errorMsg = "No drill holes found in the file. "
                   "Check that the file is a valid Excellon drill file.";
        return false;
    }

    return true;
}
