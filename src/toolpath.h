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

#ifndef TOOLPATH_H
#define TOOLPATH_H
#include "gerber.h"
#include "preprocess.h"
#include<QPoint>
#include<QtMath>
#include <QElapsedTimer>

#include "clipper.hpp"
using namespace ClipperLib;
struct Segment{
    QPoint point;
    char type;//L(line) C(circle)
};

//the path data of a element,ie,track,rect/obround/circle pad
struct MyPath{//right now all the path are generated in clockwise direction
    Element element;
    QList<Segment> segmentList;
    BoundingRect boundingRect;
    Path toolpath;//all the arc are turn into tiny lines and save in here
};

//the path data of a net
struct NetPath{
    QList<MyPath> pathList;
    BoundingRect boundingRect;
    Paths toolpath;
};
struct CollisionPair{
    int p1,p2;
};

struct CollisionToolpath{
    QList<int> list;
    QList<struct CollisionPair> pair;
};

class Toolpath//:public preprocess
{

public:

    Toolpath(Preprocess &p);
    ~Toolpath();

    QList<NetPath> netPathList;
    Paths totalToolpath;
    //consit with the precision 10e6!!!!
    qint64 toolDiameter=15748;//0.4mm bit
    //qint64 toolDiameter=7874;//0.2mm bit
    int collisionSum=0;
    qint64 time;

    QList<CollisionToolpath> tpCollisionNum;
protected:

    MyRect trackToMyRect(Track t, qint64 offset);
    BoundingRect expandBoundingRect(BoundingRect r, qint64 offset);
    MyRect rectToMyRect(Pad p1, qint64 offset);

    Track obroundToTrack(Pad o1);
    void arcToSegments(QPoint p1, QPoint p2,Path &path);
    bool bondingRecIntersect(BoundingRect r1, BoundingRect r2);
    bool cToolpathIntersects(QList<NetPath> nPList, QList<CollisionToolpath> &cTList);
    bool toolpathIntersects(QList<NetPath> nPList, QList<CollisionToolpath> &cTList);
    bool segmentCollision(Element e1, Element e2);
private:
      //the precision must be 10e6!!
     qint64 arcError=394;//0.01mm



};

#endif // TOOLPATH_H
