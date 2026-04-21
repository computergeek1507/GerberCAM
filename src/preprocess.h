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

#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "gerber.h"
#include<QPoint>
#include<QtMath>
#include <QElapsedTimer>

#include "spdlog/spdlog.h"
#include "clipper.hpp"
#include "setting.h"
using namespace ClipperLib;

struct MyRect
{
    QPoint p1,p2,p3,p4;//clockwise vertex
};


struct Element
{
    Pad pad;
    Track track;
    qint32 block;
    qint32 netNum=0;
    QString ADNum;
    char elementType;//P T C
    BoundingRect boundingRect;
    qint32 elementNum;
};

struct PadManage
{
    QString ADNum;
    double angle;
    char shape;
    QList<int> index;
    int parameterNum;
    int parameter[4];
};

struct Net
{
   QList<Element> elements;
   qint32 netNum;
   BoundingRect boundingRect;
   bool collisionFlag;
};

struct ContourSegment
{
    QPoint point1,point2;
    int block;
    bool flag;
    int pointer;
};

class Preprocess
{

public:
    ~Preprocess();

    QList<Net> netList;
    QList<Element> elementList;
    QList<Net> contourList;
    QList<PadManage> padList;
    qint64 time;
    int padNum;

    Preprocess(Gerber &g, const Setting *s);
    void clearEccentricHole(QList<Pad> pads);
protected:

    bool trackCollision(Track tt1, Track tt2);
    bool rectCollision(MyRect r1, MyRect r2);
    void checkSameNet(Gerber g);
    MyRect trackToMyRect(Track t);

    MyRect rectToMyRect(Pad p1);
    bool trackPadCollision(Track t1, Pad p1);
    Track obroundToTrack(Pad o1);
    bool padCollision(Pad p1, Pad p2);
    bool elementCollision(Element e1, Element e2);


    bool elementCollision1(Element e1, Element e2);
    BoundingRect boundingRect(Pad pad);
    BoundingRect boundingRect(Track t);


    QList<ContourSegment> searchList;


    bool bondingRecIntersect(BoundingRect r1, BoundingRect r2);
    bool pointInCircle(QPoint c, qint64 diameter, QPoint p);
    bool lineIntersection(QPoint p1Start, QPoint p1End, QPoint p2Start, QPoint p2End);
    bool lineIntersection(QPoint p1Start, QPoint p1End, QPoint p2Start, QPoint p2End, QPoint *qint32ersect);
    qint32 judgeDirection(QPoint p1, QPoint p2, QPoint p);
    bool pointInRect(MyRect r1, QPoint p);
    BoundingRect mergeRect(BoundingRect r1,BoundingRect r2);
    void findContour();
    qint64 contourThreshold=9000000000000LL;//3x3 mm² at 1e6 units/mm precision
    //qint64 contourThreshold=5629921259842;//13x11mm,10e6 precision
    bool searchLoop(int n, QPoint originPoint, QPoint currentPoint, int deepth);
    bool searchContour(Net &n);
    void padPreprocess(Gerber &g, const Setting *s);
private:
    QList<Net> nets;

    std::shared_ptr<spdlog::logger> m_logger{ nullptr };
};

#endif // PREPROCESS_H
