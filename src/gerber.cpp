/*
Copyright (c) <2014> <Lichao Ma>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "gerber.h"

#include "config.h"

#include "scale.h"

/*
● support        ○ not support

● RS-274X format gerber file
○ Other format gerber file

pad types:
    ● pads with rotation
    ● Round
    ● Rectangle
    ● Obround
    ● Teardrop(● Track   ● Arc)
    ○ Thermal
    ○ Octagonal

track types:
    ● straight line
    ○ curve line
*/


/*
 * This program currently only support RS-274X format gerber file only.
 * This program is developed under "Gerber RS-274X Format User's Guide".
 * Strongly recommend reading that document before you modify this program.
 * Here's the quick explanation of the parameters:

    char FormatStatement;//L(Leading) T(Trailing) D
    char CoordinateMode;//A(Absolute) I(Incremental)
    qint32 XInteger,XDecimal;
    qint32 YInteger,YDecimal;
    double ScaleFactorX,ScaleFactorY;
    QString ImagePolarity;//POS(positive,default) NEG(negtive)
    QString ModeofUnit;//MM(milimeter) IN(inch,default)

* FormatStatement
* -L: Omitting leading zeros,default setting.
* -T:Omitting trailing zeros.
*
* CoordinateMode;
* -A: Absolute coordinate,which is the most common one.
* -I:Incremental coordintate.
*
* XInteger,XDecimal
* YInteger,YDecimal
* -The precision of the data.
*
* ScaleFactorX,ScaleFactorY
* -Scale factor of the data,default = 1.0;
*
* ImagePolarity
* -POS:Positive image polarity,defualt setting.
* -NEG:Negative image polarity.
*
* ModeofUnit
* -MM:Using metric unit.
* -IN:Using imperial unit,default setting.
*
*/
void Gerber::initParameters()
{
    FormatStatement = FormatType::Leading;
    CoordinateMode = CoordinateType::Absolute;
    XInteger=2;XDecimal=3;
    YInteger=2;YDecimal=3;

    ScaleFactorX=1;ScaleFactorY=1;
    OffsetX=0;OffsetY=0;
    LayerName = "untitle";
    ImagePolarity = Polarity::Positive;
    ModeofUnit="IN";

    padNum=0;
    trackNum=0;
}


Gerber::Gerber(QString const& fileName): m_logger(spdlog::get(PROJECT_NAME))
{
    initParameters();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_logger->error("Failed to open file: {}", fileName.toStdString());
        return;
    }
    qDebug() << "start to read file\n";

    bool readFileFlag=false;
    int debugcount=0;

    /*
     * Start interpreting the gerber file.The process is done in two steps:
     *
     * 1.Read the raw ASCII gerber data,check if there's any error,and put
     * data in different places.E.g.,Extract the aperture definition and put
     * them in Hash tables.This is what process_line() do.
     *
     * 2.Interpret each line and put them in more complex data structures.
     * E.g.,extract pads and tracks information from the lines.This is what
     * transform_data() do.
     * */
    while (!file.atEnd()) {
        debugcount++;

        /*
         * All the data are interpreted line by line.If there's any error,
         * it returns false and ends the process.
         *
         * Step 1.Raw process.
         * */
        QByteArray line = file.readLine();
        readFileFlag=process_line(line);
        if(readFileFlag==false)
            break;
    }
    file.close();
    qDebug()<<"total line="+QString::number(debugcount);
    totalLine=debugcount;

    /*
     * Step 2.Further interpretation.
     * */
    if(readFileFlag==true)
    {
        qDebug() << "gerber file read success";

        if(transform_data()==true)
        {
            qDebug() << "gerber data transform success";
            Track temptrack;
            qDebug() << "blockNum=" + QString::number(blockNum);
            qDebug() << "tackNum=" + QString::number(trackNum);
            qDebug() << "padNum=" + QString::number(padNum);
            readingFlag=true;
            return;
            /*
            for(i=0;i<tracks.size();i++)
            {
                temptrack=tracks.at(i);
                qDebug()<<temptrack.pointstart<<temptrack.pointstart;
            }
            */

        }
        else
            qDebug() << "gerber data transform fail";
    }
    else
        qDebug() << "gerber file read fail";

    readingFlag=false;
}

/*
 * Interpret the aperture macro raw data into a pad.
 *
 * APERTURE MACRO: A mass parameter that describes the geometry of a special
 * aperture and assigns it to a D code.
 *
 * Supported primitive types (RS-274X):
 *   1  - Circle          : exposure, diameter, X, Y [, rotation]
 *   4  - Outline         : exposure, #vertices, X0,Y0, X1,Y1, ..., rotation
 *   5  - Polygon         : exposure, #vertices, X, Y, diameter, rotation
 *   20 - Vector Line     : exposure, width, X0,Y0, X1,Y1, rotation
 *   21 - Center Line     : exposure, width, height, X, Y, rotation
 *   22 - Lower-Left Line : exposure, width, height, X, Y, rotation
 *
 * The function iterates over every stored primitive and picks the first
 * exposed one to derive a representative shape (SHAPE_C / SHAPE_R / SHAPE_O)
 * used downstream for bounding-rect calculations.
 *
 * A good test case for AM is 'design1.gtl'
 * */
void Gerber::macroToPad(int AMNum, QString AMName)
{
    for (int n = 1; n <= AMNum; n++)
    {
        QString key = AMName + 'n' + QString::number(n);
        int primType = (int)ADHash.value(key + " primType", -1);
        int pNum = (int)ADHash.value(key + " pNum", 0);

        auto p = [&](int idx) -> double {
            return ADHash.value(key + 'p' + QString::number(idx), 0.0);
        };

        if (primType == 1) // Circle: exposure, diameter, cx, cy [, rotation]
        {
            double diameter = p(0);
            double angle    = (pNum >= 4) ? p(3) : 0.0;
            ADHash.insert(AMName + " Num",   1);
            ADHash.insert(AMName + " Shape", SHAPE_C);
            ADHash.insert(AMName + "0",      diameter);
            ADHash.insert(AMName + " Angle", angle);
            return;
        }
        else if (primType == 4) // Outline polygon
        {
            // vertices are stored as x0,y0,x1,y1,...,rotation
            // Build bounding box → represent as rectangle
            if (pNum < 2) continue;
            int vertexCount = pNum / 2; // each vertex is (x,y)
            double minX =  1e18, minY =  1e18;
            double maxX = -1e18, maxY = -1e18;
            for (int v = 0; v < vertexCount; v++)
            {
                double vx = p(v * 2);
                double vy = p(v * 2 + 1);
                if (vx < minX) minX = vx;
                if (vx > maxX) maxX = vx;
                if (vy < minY) minY = vy;
                if (vy > maxY) maxY = vy;
            }
            double width  = maxX - minX;
            double height = maxY - minY;
            ADHash.insert(AMName + " Num",   2);
            ADHash.insert(AMName + " Shape", SHAPE_R);
            ADHash.insert(AMName + "0",      width);
            ADHash.insert(AMName + "1",      height);
            ADHash.insert(AMName + " Angle", 0.0);
            return;
        }
        else if (primType == 5) // Polygon: exposure, vertices, cx, cy, diameter, rotation
        {
            double diameter = (pNum >= 4) ? p(3) : 0.0;
            double angle    = (pNum >= 5) ? p(4) : 0.0;
            ADHash.insert(AMName + " Num",   1);
            ADHash.insert(AMName + " Shape", SHAPE_C); // approximate as circle
            ADHash.insert(AMName + "0",      diameter);
            ADHash.insert(AMName + " Angle", angle);
            return;
        }
        else if (primType == 20) // Vector Line: exposure, width, x0,y0, x1,y1, rotation
        {
            if (pNum < 5) continue;
            double width  = p(0);
            double x0 = p(1), y0 = p(2), x1 = p(3), y1 = p(4);
            double angle  = (pNum >= 6) ? p(5) : 0.0;
            double length = qSqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
            ADHash.insert(AMName + " Num",   2);
            ADHash.insert(AMName + " Shape", SHAPE_O);
            ADHash.insert(AMName + "0",      length + width);
            ADHash.insert(AMName + "1",      width);
            ADHash.insert(AMName + " Angle", angle);
            return;
        }
        else if (primType == 21 || primType == 22) // Center Line / Lower-Left Line
        {
            // exposure, width, height, cx, cy, rotation
            if (pNum < 2) continue;
            double width  = p(0);
            double height = p(1);
            double angle  = (pNum >= 5) ? p(4) : 0.0;
            ADHash.insert(AMName + " Num",   2);
            ADHash.insert(AMName + " Shape", SHAPE_R);
            ADHash.insert(AMName + "0",      width);
            ADHash.insert(AMName + "1",      height);
            ADHash.insert(AMName + " Angle", angle);
            return;
        }
        else
        {
            m_logger->error("macroToPad: unhandled primitive type {} in macro {}", primType, AMName.toStdString());
            qDebug() << "macroToPad: unhandled primitive type" << primType << "in macro" << AMName;
        }
    }
    m_logger->error("macroToPad: no usable primitive found in macro {}", AMName.toStdString());
}

// 1.Read the raw ASCII gerber data,check if there's any error,and put
// data in different places.E.g.,Extract the aperture definition and put
// them in Hash tables.This is what process_line() do.
bool Gerber::process_line(QByteArray line)
{
    static bool AMFlag=false;
    static int AMNum=0;
    static QString AMName;

    // Normalize line endings so all checks work regardless of CRLF vs LF
    line.replace("\r\n", "\n");
    line.replace('\r', '\n');

    //Empty line
    if(line=="\n")
        return true;

    // G04 comments — skip entirely (KiCad emits these between parameter blocks)
    if(line.startsWith("G04"))
        return true;

    /*
     * RS-274X parameters are delimited by a parameter delimiter, typically a percent
     * (%) sign. Because parameters are also contained in a data block, they are also
     * delimited by an end-of-block character. For example,
     *    %FSLAX23Y23*%
     * */
    else if(line.startsWith("%")||AMFlag==true)
    {
        // Skip extended attribute commands (%TA, %TD, %TO, %TF) — not needed for geometry
        if(line.startsWith("%T"))
            return true;

        if(!line.endsWith("%\n")&&!line.endsWith("*\n"))
        {
            m_logger->error("Data error! Incomplete parameter block!");
            return false;
        }

        //qDebug() << "parameters";
        /*
         *A line start with F is usually the first line of the file,e.g.,
         *    %FSTAX23Y23*%
         * This parameter is a Format Statement (FS) describing how the coordinate
         * data in the file should be interpreted (in this case, 4.2 format for both
         * X and Y coordinates).
         * */
        if(line.at(1)=='F')
        {
            FormatStatement = (line.at(3) == 'L') ? FormatType::Leading : FormatType::Trailing;
            CoordinateMode = (line.at(4) == 'A') ? CoordinateType::Absolute : CoordinateType::Incremental;
            QByteArray number=line.mid(6,1);
            XInteger=number.toInt();
            number=line.mid(7,1);
            XDecimal=number.toInt();
            number=line.mid(9,1);
            YInteger=number.toInt();
            number=line.mid(10,1);
            YDecimal=number.toInt();
            //QString debugString=QString::number(XInteger)+QString::number(XDecimal)+" "+
            //            QString::number(YInteger)+QString::number(YDecimal);
            //qDebug()<<debugString;

        }

        /*
         * Aperture parameters definition.Aperture format:
         *    %ADD<D-code number><aperture type>[,<modifier>[X<modifier>]*]*%
         * where:
         *  ADD                 - the AD parameter and D (for D-code)
         *  <D-code number>     - the D-code number being defined (10 - 999)
         *  <aperture type>     - the aperture descriptions.Standard types:
         *                          C - Circle
         *                          R - Rectangle or square
         *                          O - Obround(oval)
         *                          P - Regular polygon
         *                       or an aperture macro name (e.g. MYMACRO)
         *
         * <modifier>           - depends on aperture type,X to separate each modifier.
         *
         * examples:
         * %ADD10C,.025*%               - D-code 10: 25 mil round
         * %ADD22R,.050X.050X.027*%     - D-code 22: 50 mil square with 27 mil round hole
         * %ADD57O,.030X.040X.015*%     - D-code 57: obround 30x40 mil with 15 mil round hole
         * %ADD14MYMACRO*%              - D-code 14: aperture from macro MYMACRO
         * %ADD14MYMACRO,0.5X0.3*%     - D-code 14: macro with modifiers
         *
         * */
        else if(line.startsWith("%AD"))
        {
            // Extract D-code: digits starting at position 4 (after '%ADD')
            int dStart = 4;
            int dEnd   = dStart;
            while(dEnd < line.length() && line.at(dEnd) >= '0' && line.at(dEnd) <= '9')
                dEnd++;

            // ADName is 'D' + the numeric code, e.g. "D14"
            QString ADName = line.mid(3, dEnd - 3); // includes the leading 'D'

            // Shape/macro name runs from dEnd up to ',' or '*'
            int commaPos = line.indexOf(',', dEnd);
            int starPos  = line.indexOf('*', dEnd);
            int nameEnd  = (commaPos >= 0 && commaPos < starPos) ? commaPos : starPos;
            QString shapePart = line.mid(dEnd, nameEnd - dEnd);

            // Is this a standard aperture type?
            static const QByteArray stdShapes = "CROP";
            bool isStandard = (shapePart.length() == 1 &&
                               stdShapes.contains(shapePart.at(0).toLatin1()));

            if(isStandard)
            {
                char shapeName = shapePart.at(0).toLatin1();
                if(shapeName=='R')
                    ADHash.insert(ADName+" Shape",SHAPE_R);
                else if(shapeName=='O')
                    ADHash.insert(ADName+" Shape",SHAPE_O);
                else if(shapeName=='C')
                    ADHash.insert(ADName+" Shape",SHAPE_C);
                else if(shapeName=='P')
                    ADHash.insert(ADName+" Shape",SHAPE_P);

                ADHash.insert(ADName+" Angle", 0.0);

                // Count and save numeric modifiers (separated by 'X')
                int parameterNum = 0;
                if(commaPos >= 0)
                {
                    int j = commaPos + 1;
                    while(j < line.length() && line.at(j) != '*' && line.at(j) != '%')
                    {
                        // Read one number
                        int numStart = j;
                        while(j < line.length() &&
                              ((line.at(j)>='0'&&line.at(j)<='9') || line.at(j)=='.'))
                            j++;
                        if(j > numStart)
                        {
                            QByteArray numBytes = line.mid(numStart, j - numStart);
                            ADHash.insert(ADName + QString::number(parameterNum), numBytes.toDouble());
                            parameterNum++;
                        }
                        // Skip separator ('X' or ',')
                        if(j < line.length() && (line.at(j)=='X' || line.at(j)==','))
                            j++;
                    }
                }
                ADHash.insert(ADName+" Num", parameterNum);
                qDebug() << ADName << "=" << QString::number(ADHash.value(ADName+" Num"));
            }
            else
            {
                // Macro aperture reference: e.g. %ADD16RoundRect,0.25X-0.6X-0.75X0.6X-0.75X...X0*%
                // Modifiers after the comma replace $1, $2, ... in the macro definition.
                // Since variable evaluation is not implemented, we compute a bounding
                // rectangle directly from the modifier values.
                QString macroName = shapePart;
                qDebug() << ADName << "references macro" << macroName;

                // Parse modifiers (comma-separated, then X-separated)
                QList<double> modifiers;
                if(commaPos >= 0)
                {
                    int j = commaPos + 1;
                    while(j < line.length() && line.at(j) != '*' && line.at(j) != '%')
                    {
                        int numStart = j;
                        // Allow negative numbers
                        if(j < line.length() && line.at(j) == '-')
                            j++;
                        while(j < line.length() &&
                              ((line.at(j)>='0' && line.at(j)<='9') || line.at(j)=='.'))
                            j++;
                        if(j > numStart)
                        {
                            QByteArray numBytes = line.mid(numStart, j - numStart);
                            modifiers.append(numBytes.toDouble());
                        }
                        if(j < line.length() && (line.at(j)=='X' || line.at(j)==','))
                            j++;
                    }
                }

                if(modifiers.size() >= 9)
                {
                    // Typical KiCad RoundRect/macro: $1=rounding, ($2,$3),($4,$5),($6,$7),($8,$9)=corners, $10=rotation
                    // Compute bounding box from the 4 corner coordinates
                    double rounding = modifiers[0];
                    double bminX = modifiers[1], bmaxX = modifiers[1];
                    double bminY = modifiers[2], bmaxY = modifiers[2];
                    for(int ci = 1; ci < 4; ci++)
                    {
                        double cx = modifiers[1 + ci*2];
                        double cy = modifiers[2 + ci*2];
                        if(cx < bminX) bminX = cx;
                        if(cx > bmaxX) bmaxX = cx;
                        if(cy < bminY) bminY = cy;
                        if(cy > bmaxY) bmaxY = cy;
                    }
                    double width  = (bmaxX - bminX) + rounding * 2;
                    double height = (bmaxY - bminY) + rounding * 2;
                    double angle  = (modifiers.size() >= 10) ? modifiers[9] : 0.0;

                    ADHash.insert(ADName+" Shape", SHAPE_R);
                    ADHash.insert(ADName+" Num",   2);
                    ADHash.insert(ADName+"0",      width);
                    ADHash.insert(ADName+"1",      height);
                    ADHash.insert(ADName+" Angle", angle);
                }
                else
                {
                    // Fewer modifiers — copy whatever macroToPad resolved
                    ADHash.insert(ADName+" Shape", ADHash.value(macroName+" Shape", SHAPE_M));
                    ADHash.insert(ADName+" Num",   ADHash.value(macroName+" Num",   0));
                    ADHash.insert(ADName+" Angle", ADHash.value(macroName+" Angle", 0.0));

                    int pNum = (int)ADHash.value(macroName+" Num", 0);
                    for(int pi = 0; pi < pNum; pi++)
                        ADHash.insert(ADName+QString::number(pi),
                                      ADHash.value(macroName+QString::number(pi), 0.0));
                }
            }

        }
        /*
         * Aperture Macro (AM) parameter block.
         * Each primitive occupies one comma-separated record terminated by '*'.
         * Multi-line macros: name on first line ending with '*', primitives follow,
         * block closed by a bare '%' line.
         * Single-line macros: name and primitives on the same '%AM...' line.
         *
         * Supported primitive types:
         *   1  - Circle
         *   4  - Outline
         *   5  - Polygon
         *   20 - Vector Line
         *   21 - Center Line
         *   22 - Lower-Left Line
         *
         * A good test case for AM is 'design1.gtl'
         * */
        else if(line.startsWith("%AM")||AMFlag==true)
        {
            // Bare '%' or '%\n' closes a multi-line macro block
            if(line.trimmed()=="%")
            {
                AMFlag=false;
                macroToPad(AMNum,AMName);
                return true;
            }

            // '%AM<name>*%' — header with no primitives on this line and closed immediately
            if(line.startsWith("%AM") && line.endsWith("%\n") && line.indexOf('*')==(int)(line.size()-3))
            {
                AMName = line.mid(3, line.indexOf('*')-3);
                AMNum  = 0;
                AMFlag = false;
                return true;
            }

            // '%AM<name>*\n' — multi-line macro: name only on this line
            if(line.startsWith("%AM") && line.indexOf('*')==line.size()-2)
            {
                AMFlag = true;
                AMNum  = 0;
                AMName = line.mid(3, line.indexOf('*')-3);
                return true;
            }

            // Determine where primitive data begins
            int position = 0;
            if(!AMFlag) // single-line: '%AM<name>*<primitive>,...*%'
            {
                int starPos = line.indexOf('*');
                AMName   = line.mid(3, starPos - 3);
                AMNum    = 0;
                position = starPos + 1;
            }

            // Parse every '*'-terminated primitive record on this line
            while(position < line.size())
            {
                // Skip closing '%', newlines
                while(position < line.size() &&
                      (line.at(position)=='%' || line.at(position)=='\n' || line.at(position)=='\r'))
                    position++;
                if(position >= line.size()) break;

                // Find the end of this primitive record (terminated by '*')
                int recordEnd = line.indexOf('*', position);
                if(recordEnd < 0) break;

                QString record = line.mid(position, recordEnd - position);
                position = recordEnd + 1;

                if(record.isEmpty()) continue;

                QStringList fields = record.split(',');
                if(fields.isEmpty()) continue;

                bool ok = false;
                int primType = fields.at(0).trimmed().toInt(&ok);
                if(!ok)
                {
                    // Could be an AM comment (type 0 text) or other non-numeric
                    continue;
                }

                // Primitive type 0 is a comment — skip it
                if(primType == 0)
                    continue;

                AMNum++;
                QString key = AMName + 'n' + QString::number(AMNum);
                ADHash.insert(key + " primType", primType);
                ADHash.insert(AMName + " Shape", SHAPE_M);

                bool hasExposure = (primType==1 || primType==4 || primType==5 ||
                                    primType==20 || primType==21 || primType==22);

                int storedCount = 0;
                bool skipPrimitive = false;
                for(int fi = 1; fi < fields.size(); fi++)
                {
                    QString f = fields.at(fi).trimmed();
                    if(f.isEmpty()) continue;

                    double val = 0.0;
                    if(f.startsWith('$'))
                    {
                        m_logger->error("AM: variable expression not supported: {}", f.toStdString());
                    }
                    else
                    {
                        val = f.toDouble();
                    }

                    if(hasExposure && fi==1)
                    {
                        // Exposure flag: skip the entire primitive if unexposed
                        if((int)val == 0)
                        {
                            skipPrimitive = true;
                            break;
                        }
                        continue; // don't store the exposure value as a parameter
                    }

                    ADHash.insert(key + 'p' + QString::number(storedCount), val);
                    storedCount++;
                }

                if(skipPrimitive)
                {
                    ADHash.remove(key + " primType");
                    AMNum--;
                    continue;
                }

                ADHash.insert(key + " pNum", storedCount);
                qDebug() << "AM" << AMName << "n" << AMNum
                         << "primType=" << primType << "pNum=" << storedCount;
            }

            // Check if line ends with '%' — closes the AM block
            if(AMFlag && line.trimmed().endsWith("%"))
            {
                AMFlag = false;
                macroToPad(AMNum, AMName);
            }

            // If this was a single-line AM, resolve it immediately
            if(!AMFlag && AMNum > 0)
                macroToPad(AMNum, AMName);
        }
        else if(line.at(1)=='M')
        {
            ModeofUnit=line.at(3);
            ModeofUnit+=line.at(4);
            ModeofUnit+='\0';
        }
        // Layer Polarity (%LPD*% or %LPC*%) and other % commands — skip gracefully
        else if(line.startsWith("%LP") || line.startsWith("%LN") ||
                line.startsWith("%SR") || line.startsWith("%IR") ||
                line.startsWith("%IP"))
        {
            return true;
        }

    }
    else if(line.contains("X")||line.contains("Y")||
                line.contains("G")||line.contains("M")||line.contains("D"))
    {
        if(!(line.endsWith("*\n")||line.endsWith("*")))
        {
            m_logger->error("Data error! Incomplete data block!");
            return false;
        }
        if(line.startsWith("D"))
            line.prepend("G54");
        line.chop(2);
        DataList.append(line);

    }
    else return false;
    return true;
}

qint64 Gerber::convertNumber(QString line,QString c,qint32 integerDigit,qint32 decimalDigit)
{
    int i=0;
    int start,end;
    start=line.indexOf(c)+1;

    // Handle negative sign
    bool negative = false;
    if(start < line.size() && line.at(start)=='-')
    {
        negative = true;
        start++;
    }

    for(end=start;end<line.size();end++)
        if(!(line.at(end)>='0'&&line.at(end)<='9'))
                break;
    int length=end-start; // digit count only, excluding sign
    QString temp=line.mid(start,length);
    qint64 number=temp.toLongLong();
    int scaleNum=decimalDigit+integerDigit-length;
    if(FormatStatement == FormatType::Trailing)
    {
        // Trailing zeros omitted: missing digits are on the right (low-order),
        // so we pad by multiplying.
        while(i<scaleNum)
        {
            number*=10;
            i++;
        }
    }
    // For 'L' (Leading zeros omitted): missing digits are on the left (high-order),
    // so no multiplication is needed — the number is already right-aligned.
    i=0;
    while(decimalDigit+i< PRECISION)
    {
        number*=10;
        i++;
    }

    if(negative)
        number = -number;

    if(ModeofUnit == "IN")
    {
        // Convert from inch-based internal units (1 unit = 1 microinch) to
        // mm-based internal units (1 unit = 1 nm) by multiplying by 25.4.
        double scaled = static_cast<double>(number) * 25.4;
        return static_cast<qint64>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
    }
    // Already in mm-based internal units (1 unit = 1 nm), no conversion needed.
    return number;
}

/*
 * This function finds the border of the PCB.Later on this border information will
 * be used to automatically scale the PCB on screen.
 * */
void Gerber::find_border(BoundingRect r)
{
    if(r.left<minX)
        minX=r.left;
    if(r.right>maxX)
        maxX=r.right;
    if(r.bottom<minY)
        minY=r.bottom;
    if(r.top>maxY)
        maxY=r.top;
}

/*
 * Interpret the data alfter process_line() function.
 * */
bool Gerber::transform_data()
{
    int i{0};
    QString line;
    QString currentParameter;
    //char currentShape;
    PadShape currentShape;
    double currentX{ 0.0 };
	double currentY{ 0.0 };
    double lastX{ 0.0 };
    double lastY{ 0.0 };
    int currentMode{ 0 };//D01,D02,D03;

    //Find the first XY data
    for(i=0;i<DataList.size();i++)
    {
        line=DataList.at(i);
        if(line.contains("X"))
            break;
    }

    /*
     * All the gerber data are point data,to draw a line we need to record previous
     * point.
     * */
    lastX=convertNumber(line,"X",XInteger,XDecimal);
    lastY=convertNumber(line,"Y",YInteger,YDecimal);

    maxX=lastX;
    minX=lastX;
    maxY=lastY;
    minY=lastY;


    for(i=0;i<DataList.size();i++)
    {
        line=DataList.at(i);
        if(line=="G91")
        {
            qDebug()<<"G91 command is not supported";
            return false;
        }

        /*
         * Turn on Polygon Area Fill
         * */
        if(line=="G36")
        {
            //qDebug()<<"G36 command is not supported";
            //return false;
            polygonFillMode=true;
        }

        /*
         * Turn off Polygon Area Fill
         * */
        if(line=="G37")
        {
            //qDebug()<<"G36 command is not supported";
            //return false;
            polygonFillMode=false;
        }

        /*
         * Tool prepare.Usually precedes an aperture D-code.
         * When this code appears,we need to change the aperture shape.
         * */
        if(line.startsWith("G54"))
        {
            currentParameter=line.mid(3,line.size()-3);
            if(ADHash.value(currentParameter + " Shape")==SHAPE_C)
                currentShape = PadShape::Circle;
            else if(ADHash.value(currentParameter + " Shape")==SHAPE_O)
                currentShape = PadShape::Oval;
            else if(ADHash.value(currentParameter + " Shape")==SHAPE_R)
                currentShape = PadShape::Rectangle;
            else if(ADHash.value(currentParameter + " Shape")==SHAPE_P)
                currentShape = PadShape::Polygon;
            else if(ADHash.value(currentParameter + " Shape")==SHAPE_M)
                currentShape = PadShape::Macro; // aperture macro — shape resolved at flash time
            else
                m_logger->error("G54: unknown shape for {}", currentParameter.toStdString());
            continue;
        }

        /*
         * D Codes definition:
         * D01(D2)  - Draw line,exposure on.
         * D02(D2)  - Exposure off.This is like G00 in GCode,which is rapid positioning.
         * D03(D3)  - Flash aperture.Used for exposuring a pad.
         * */
        if(line.endsWith("D01")) currentMode=1;
        else if(line.endsWith("D02")) currentMode=2;
        else if(line.endsWith("D03")) currentMode=3;

        if(!(line.contains("X")||line.contains("Y")))
            continue;

        if(currentMode==1)//draw tracks
        {
            if(line.contains("X"))
                currentX=convertNumber(line,"X",XInteger,XDecimal);
            if(line.contains("Y"))
                currentY=convertNumber(line,"Y",YInteger,YDecimal);

            Track newTrack;
            newTrack.pointstart.setX(lastX);
            newTrack.pointstart.setY(lastY);
            newTrack.pointend.setX(currentX);
            newTrack.pointend.setY(currentY);

            if(polygonFillMode==true)
                newTrack.width=12700; // 0.0127 mm minimum fill width
            else
                newTrack.width=ADHash.value(currentParameter + "0") * PRECISIONSCALE * (ModeofUnit == "IN" ? 25.4 : 1.0);

            newTrack.boundingRect=boundingRect(newTrack);
            tracksList.append(newTrack);
            lastX=currentX;
            lastY=currentY;
            trackNum++;

            find_border(newTrack.boundingRect);
        }
        else if(currentMode==2)//fast move
        {
            if(line.contains("X"))
                currentX=convertNumber(line,"X",XInteger,XDecimal);
            if(line.contains("Y"))
                currentY=convertNumber(line,"Y",YInteger,YDecimal);

            lastX=currentX;
            lastY=currentY;
        }
        else if (currentMode == 3)//draw pads
        {
            if (line.contains("X"))
                currentX = convertNumber(line, "X", XInteger, XDecimal);
            if (line.contains("Y"))
                currentY = convertNumber(line, "Y", YInteger, YDecimal);

            Pad newPad;
            newPad.point.setX(currentX);
            newPad.point.setY(currentY);
            newPad.shape = currentShape;
            newPad.hole = 0;

            newPad.parameterNum = ADHash.value(currentParameter + " Num");
            newPad.angle = ADHash.value(currentParameter + " Angle");
            for (int j = 0; j < newPad.parameterNum; j++)
            {
                newPad.parameter[j] = ADHash.value(currentParameter + QString::number(j)) * PRECISIONSCALE * (ModeofUnit == "IN" ? 25.4 : 1.0);
            }

            // Read hole diameter directly from aperture definition if present.
            // Gerber spec: C has optional 2nd param (hole), R/O have optional 3rd param (hole).
            if (newPad.shape == PadShape::Circle && newPad.parameterNum >= 2)
            {
                newPad.hole = newPad.parameter[1];
            }
            else if ((newPad.shape == PadShape::Rectangle || newPad.shape == PadShape::Oval)
                && newPad.parameterNum >= 3)
            {
                newPad.hole = newPad.parameter[2];
            }
            newPad.ADNum = currentParameter;
            newPad.boundingRect = boundingRect(newPad);
            padsList.append(newPad);
            lastX = currentX;
            lastY = currentY;
            padNum++;

            find_border(newPad.boundingRect);
        }
    }

    blockNum=block;

    borderRect.setX(minX);
    borderRect.setY(minY);
    borderRect.setWidth(maxX-minX);
    borderRect.setHeight(maxY-minY);

    blockCount();

    return true;
}

Gerber::~Gerber()
{

}

/*
 * This function will calculate the border of the element.No matter what
 * kind of shape the element is,it will generate a rectangle border for it.
 * This will largely decrease the time to check collision in later process.
 * */
BoundingRect Gerber::boundingRect(Pad pad)
{
    BoundingRect r;
    if(pad.angle==0)
    {
        if(pad.shape==PadShape::Circle)
        {
            r.bottom=pad.point.y()-pad.parameter[0]/2;
            r.top=pad.point.y()+pad.parameter[0]/2;
            r.left=pad.point.x()-pad.parameter[0]/2;
            r.right=pad.point.x()+pad.parameter[0]/2;
        }
        else if(pad.shape==PadShape::Rectangle||pad.shape==PadShape::Oval)
        {
            r.bottom=pad.point.y()-pad.parameter[1]/2;
            r.top=pad.point.y()+pad.parameter[1]/2;
            r.left=pad.point.x()-pad.parameter[0]/2;
            r.right=pad.point.x()+pad.parameter[0]/2;
        }
    }
    else
    {
        if(pad.shape==PadShape::Rectangle)
        {
            if(pad.angle==0)
            {
                r.bottom=pad.point.y()-pad.parameter[1]/2;
                r.top=pad.point.y()+pad.parameter[1]/2;
                r.left=pad.point.x()-pad.parameter[0]/2;
                r.right=pad.point.x()+pad.parameter[0]/2;
            }
            else
            {
                qint64 x,y;
                y=pad.parameter[0]*qSin(qAbs(pad.angle))/2
                        +pad.parameter[1]*qSin(3.1415926/2-qAbs(pad.angle))/2;
                x=pad.parameter[0]*qCos(qAbs(pad.angle))/2
                        +pad.parameter[1]*qSin(qAbs(pad.angle))/2;
                r.top=pad.point.y()+y;
                r.bottom=pad.point.y()-y;
                r.right=pad.point.x()+x;
                r.left=pad.point.x()-x;
            }
        }
        else if(pad.shape==PadShape::Oval)
        {
            if(pad.angle==0)
            {
                r.bottom=pad.point.y()-pad.parameter[1]/2;
                r.top=pad.point.y()+pad.parameter[1]/2;
                r.left=pad.point.x()-pad.parameter[0]/2;
                r.right=pad.point.x()+pad.parameter[0]/2;
            }
            else
            {
                qint64 x,y;
                y=pad.parameter[0]*qSin(qAbs(pad.angle))/2+pad.parameter[1]/2;
                x=pad.parameter[0]*qCos(qAbs(pad.angle))/2+pad.parameter[1]/2;
                r.top=pad.point.y()+y;
                r.bottom=pad.point.y()-y;
                r.right=pad.point.x()+x;
                r.left=pad.point.x()-x;
            }

        }
        else if(pad.shape==PadShape::Macro)
        {
            // Macro pad — use parameters like a rectangle if available
            if(pad.parameterNum >= 2)
            {
                qint64 x,y;
                y=pad.parameter[1]/2;
                x=pad.parameter[0]/2;
                r.top=pad.point.y()+y;
                r.bottom=pad.point.y()-y;
                r.right=pad.point.x()+x;
                r.left=pad.point.x()-x;
            }
            else if(pad.parameterNum >= 1)
            {
                r.bottom=pad.point.y()-pad.parameter[0]/2;
                r.top=pad.point.y()+pad.parameter[0]/2;
                r.left=pad.point.x()-pad.parameter[0]/2;
                r.right=pad.point.x()+pad.parameter[0]/2;
            }
            else
            {
                // Fallback: zero-size bounding rect at the pad center
                r.bottom=pad.point.y();
                r.top=pad.point.y();
                r.left=pad.point.x();
                r.right=pad.point.x();
            }
        }
    }
    r.area=(r.top-r.bottom)*(r.right-r.left);
    return r;
}

/*
 * This function will calculate the border of the element.No matter what
 * kind of shape the element is,it will generate a rectangle border for it.
 * This will largely decrease the time to check collision in later process.
 * */
BoundingRect Gerber::boundingRect(Track t)
{
    BoundingRect r;
    if(t.pointend.x()>t.pointstart.x())
    {
        if(t.pointend.y()>t.pointstart.y())
        {
            r.right=t.pointend.x()+t.width/2;
            r.top=t.pointend.y()+t.width/2;
            r.left=t.pointstart.x()-t.width/2;
            r.bottom=t.pointstart.y()-t.width/2;
        }
        else
        {
            r.right=t.pointend.x()+t.width/2;
            r.top=t.pointstart.y()+t.width/2;
            r.left=t.pointstart.x()-t.width/2;
            r.bottom=t.pointend.y()-t.width/2;
        }

    }
    else
    {
        if(t.pointend.y()>t.pointstart.y())
        {
            r.right=t.pointstart.x()+t.width/2;
            r.top=t.pointend.y()+t.width/2;
            r.left=t.pointend.x()-t.width/2;
            r.bottom=t.pointstart.y()-t.width/2;
        }
        else
        {
            r.right=t.pointstart.x()+t.width/2;
            r.top=t.pointstart.y()+t.width/2;
            r.left=t.pointend.x()-t.width/2;
            r.bottom=t.pointend.y()-t.width/2;
        }

    }
    r.area=(r.top-r.bottom)*(r.right-r.left);
    return r;
}

/*

bool toolpath::bondingRecIntersct(QRectF r1, QRectF r2)
{
    //BE CAREFUL OF the DECIMAL 0.001!!
    if(r1.right()+0.001<r2.left()) return false;
    if(r1.top()+0.001<r2.bottom()) return false;
    if(r1.bottom()-r2.top()>0.001) return false;
    if(r1.left()-r2.right()>0.001) return false;
    return true;
}

 * */
/*
QRectF gerber::boundingRect(struct pad pad)
{
    QRectF r;
    if(pad.shape=='C')
    {
        r.setBottom(pad.point.y()-pad.parameter[0]/2.0);
        r.setTop(pad.point.y()+pad.parameter[0]/2.0);
        r.setLeft(pad.point.x()-pad.parameter[0]/2.0);
        r.setRight(pad.point.x()+pad.parameter[0]/2.0);
    }
    else if(pad.shape=='R'||pad.shape=='O')
    {
        r.setBottom(pad.point.y()-pad.parameter[1]/2.0);
        r.setTop(pad.point.y()+pad.parameter[1]/2.0);
        r.setLeft(pad.point.x()-pad.parameter[0]/2.0);
        r.setRight(pad.point.x()+pad.parameter[0]/2.0);
    }

    return r;
}

QRectF gerber::boundingRect(struct track t)
{
    QRectF r;
    if(t.pointend.x()>t.pointstart.x())
    {
        if(t.pointend.y()>t.pointstart.y())
        {
            r.setRight(t.pointend.x()+t.width/2.0);
            r.setTop(t.pointend.y()+t.width/2.0);
            r.setLeft(t.pointstart.x()-t.width/2.0);
            r.setBottom(t.pointstart.y()-t.width/2.0);
        }
        else
        {
            r.setRight(t.pointend.x()+t.width/2.0);
            r.setTop(t.pointstart.y()+t.width/2.0);
            r.setLeft(t.pointstart.x()-t.width/2.0);
            r.setBottom(t.pointend.y()-t.width/2.0);
        }

    }
    else
    {
        if(t.pointend.y()>t.pointstart.y())
        {
            r.setRight(t.pointstart.x()+t.width/2.0);
            r.setTop(t.pointend.y()+t.width/2.0);
            r.setLeft(t.pointend.x()-t.width/2.0);
            r.setBottom(t.pointstart.y()-t.width/2.0);
        }
        else
        {
            r.setRight(t.pointstart.x()+t.width/2.0);
            r.setTop(t.pointstart.y()+t.width/2.0);
            r.setLeft(t.pointend.x()-t.width/2.0);
            r.setBottom(t.pointend.y()-t.width/2.0);
        }

    }
    return r;
}
*/

/*
 * Exchange the start point and end point of the track.
 * */
void Gerber::inverseTrack(Track &t)
{
    QPoint p;
    p=t.pointend;
    t.pointend=t.pointstart;
    t.pointstart=p;
}

//
/*
 * Simple net categorizing.In PCB layout,net means a set of pads or tracks which are
 * physically connected.E.g.,GND is a net and lots of tracks and componet pads are
 * connected with GND.Block is the term I chose to describe a net,more precisely a
 * part of a net.It means all the pads and tracks in this set are connected through
 * certain points,like a chain,instead of 'collision'.E.g,an arc in gerber could be
 * decribed as a chain of small straight segments,or a track may connects two pads
 * toghter.These elements share joints so they can be easily recognized as a net.So
 * there's no need to use collision detect algorithm to recognize them,which saves
 * lots of time.
 * */
void Gerber::blockCount()
{
    int i,j;
    bool sameBlock=false;
    Track t;
    Pad p;

    if(trackNum==0 && padNum==0)
        return;

    if(trackNum!=0)
    {
        t=tracksList.at(0);
        t.block=1;
        tracksList.replace(0,t);
    }
    else
    {
        p=padsList.at(0);
        p.block=1;
        padsList.replace(0,p);
    }
    blockNum=1;
    for(i=0;i<trackNum-1;i++)
    {
        /*
         * There are four possible ways of two tracks connecting together:
         *  t1.end   & t2.start
         *  t1.end   & t2.end
         *  t1.start & t2.end
         *  t1.start & t2.start
         * All of these need to be checked.If two tracks connects,then assign
         * the block number to the second track.Otherwise,creat a new block number.
         * */
        if(tracksList.at(i).pointend==tracksList.at(i+1).pointend||
                tracksList.at(i).pointend==tracksList.at(i+1).pointstart||
                    tracksList.at(i).pointstart==tracksList.at(i+1).pointend||
                        tracksList.at(i).pointstart==tracksList.at(i+1).pointstart)
        {

            t=tracksList.at(i+1);
            t.block=tracksList.at(i).block;
            if(tracksList.at(i).pointend==tracksList.at(i+1).pointend||
                    tracksList.at(i).pointstart==tracksList.at(i+1).pointstart)
                inverseTrack(t);
            tracksList.replace(i+1,t);
        }
        else
        {
            blockNum++;
            t=tracksList.at(i+1);
            t.block=blockNum;
            tracksList.replace(i+1,t);
        }
    }

    for(i=0;i<padNum;i++)
    {
        sameBlock=false;
        for(j=0;j<trackNum;j++)
        {
            /*
             * Check the connection of a track and a pad.
             * */
            if(padsList.at(i).point==tracksList.at(j).pointend||
                    padsList.at(i).point==tracksList.at(j).pointstart)
            {
                p=padsList.at(i);
                p.block=tracksList.at(j).block;
                padsList.replace(i,p);
                sameBlock=true;
                break;
            }
        }
        if(sameBlock==false)
        {
            blockNum++;
            p=padsList.at(i);
            p.block=blockNum;
            padsList.replace(i,p);
        }
    }
    for(i=0;i<trackNum;i++)
    {
        t=tracksList.at(i);
        t.totalBlock=blockNum;
        tracksList.replace(i,t);
    }
    for(i=0;i<padNum;i++)
    {
        p=padsList.at(i);
        p.totalBlock=blockNum;
        padsList.replace(i,p);
    }
}

