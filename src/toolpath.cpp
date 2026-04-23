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

#define _USE_MATH_DEFINES

#include "toolpath.h"
#include <math.h>

#include "scale.h"

#include "config.h"

MyRect Toolpath::trackToMyRect(Track t, qint64 offset)
{
    MyRect r;
    QPoint p1;
    double k,ki;
    double angle;

    t.width += offset;

    if((t.pointend.x()-t.pointstart.x())==0)//vertical track
    {
        p1.setX(t.width/2);
        p1.setY(0);
        if(t.pointend.y()>t.pointstart.y())
        {
            r.p1=t.pointstart-p1;
            r.p2=t.pointend-p1;
            r.p3=t.pointend+p1;
            r.p4=t.pointstart+p1;
        }
        else
        {
            r.p1=t.pointstart+p1;
            r.p2=t.pointend+p1;
            r.p3=t.pointend-p1;
            r.p4=t.pointstart-p1;
        }
    }
    else if((t.pointend.y()-t.pointstart.y())==0)//horizontal track
    {
        p1.setX(0);
        p1.setY(t.width/2);
        if(t.pointend.x()>t.pointstart.x())
        {
            r.p1=t.pointstart+p1;
            r.p2=t.pointend+p1;
            r.p3=t.pointend-p1;
            r.p4=t.pointstart-p1;
        }
        else
        {
            r.p1=t.pointstart-p1;
            r.p2=t.pointend-p1;
            r.p3=t.pointend+p1;
            r.p4=t.pointstart+p1;
        }
    }
    else
    {
        k=(t.pointend.y()-t.pointstart.y())*1.0/(t.pointend.x()-t.pointstart.x());
        ki=-1/k;
        angle=qAtan(ki);
        //if(angle<0)
            //angle+=M_PI;//M_PI
        p1.setX(t.width/2*qCos(angle));
        p1.setY(t.width/2*qSin(angle));

        if(t.pointstart.y()>t.pointend.y())
        {
            r.p1=t.pointstart+p1;
            r.p2=t.pointend+p1;
            r.p3=t.pointend-p1;
            r.p4=t.pointstart-p1;
        }
        else
        {
            r.p1=t.pointstart-p1;
            r.p2=t.pointend-p1;
            r.p3=t.pointend+p1;
            r.p4=t.pointstart+p1;
        }
    }
    return r;
}

BoundingRect Toolpath::expandBoundingRect(BoundingRect r,qint64 offset)
{
    r.top+=offset;
    r.right+=offset;
    r.bottom-=offset;
    r.left-=offset;
    return r;
}

MyRect Toolpath::rectToMyRect(Pad p1,qint64 offset)
{
    MyRect r1;
    QPoint point;
    //offset/=2;

    if(p1.angle==0)
    {
        point.setX(p1.point.x()-p1.parameter[0]/2-offset/2);
        point.setY(p1.point.y()+p1.parameter[1]/2+offset/2);
        r1.p1=point;

        point.setX(p1.point.x()+p1.parameter[0]/2+offset/2);
        point.setY(p1.point.y()+p1.parameter[1]/2+offset/2);
        r1.p2=point;

        point.setX(p1.point.x()+p1.parameter[0]/2+offset/2);
        point.setY(p1.point.y()-p1.parameter[1]/2-offset/2);
        r1.p3=point;

        point.setX(p1.point.x()-p1.parameter[0]/2-offset/2);
        point.setY(p1.point.y()-p1.parameter[1]/2-offset/2);
        r1.p4=point;
    }
    else
    {
        p1.parameter[0]+=offset;
        p1.parameter[1]+=offset;
        if(p1.angle>0)
        {
            qint64 x,y;
            y=p1.parameter[1]*qCos(qAbs(p1.angle))/2+p1.parameter[0]*qSin(qAbs(p1.angle))/2;
            x=p1.parameter[1]*qSin(qAbs(p1.angle))/2-p1.parameter[0]*qCos(qAbs(p1.angle))/2;
            r1.p1.setX(p1.point.x()-x);
            r1.p1.setY(p1.point.y()+y);

            r1.p3.setX(p1.point.x()+x);
            r1.p3.setY(p1.point.y()-y);

            y=p1.parameter[1]*qCos(qAbs(p1.angle))/2-p1.parameter[0]*qSin(qAbs(p1.angle))/2;
            x=p1.parameter[1]*qSin(qAbs(p1.angle))/2+p1.parameter[0]*qCos(qAbs(p1.angle))/2;
            r1.p2.setX(p1.point.x()-x);
            r1.p2.setY(p1.point.y()+y);

            r1.p4.setX(p1.point.x()+x);
            r1.p4.setY(p1.point.y()-y);
        }
        else
        {
            qint64 x,y;
            y=p1.parameter[1]*qCos(qAbs(p1.angle))/2+p1.parameter[0]*qSin(qAbs(p1.angle))/2;
            x=p1.parameter[1]*qSin(qAbs(p1.angle))/2-p1.parameter[0]*qCos(qAbs(p1.angle))/2;
            r1.p1.setX(p1.point.x()+x);
            r1.p1.setY(p1.point.y()+y);

            r1.p3.setX(p1.point.x()-x);
            r1.p3.setY(p1.point.y()-y);

            y=p1.parameter[1]*qCos(qAbs(p1.angle))/2-p1.parameter[0]*qSin(qAbs(p1.angle))/2;
            x=p1.parameter[1]*qSin(qAbs(p1.angle))/2+p1.parameter[0]*qCos(qAbs(p1.angle))/2;
            r1.p2.setX(p1.point.x()+x);
            r1.p2.setY(p1.point.y()+y);

            r1.p4.setX(p1.point.x()-x);
            r1.p4.setY(p1.point.y()-y);
        }
    }

    return r1;
}

Path Toolpath::elementToPath(Element const& e, qint64 offset)
{
    Path path;
    if (e.elementType == ElementType::Track)
    {
        MyRect r = trackToMyRect(e.track, offset);
        IntPoint pt;
        pt.X = r.p1.x(); pt.Y = r.p1.y(); path << pt;
        arcToSegments(r.p2, r.p3, path);
        pt.X = r.p4.x(); pt.Y = r.p4.y(); path << pt;
        arcToSegments(r.p4, r.p1, path);
    }
    else
    {
        if (e.pad.shape == PadShape::Circle)
        {
            QPoint pt1, pt2;
            pt1.setX(e.pad.point.x());
            pt1.setY(e.pad.point.y() + e.pad.parameter[0] / 2 + offset / 2);
            pt2.setX(e.pad.point.x());
            pt2.setY(e.pad.point.y() - e.pad.parameter[0] / 2 - offset / 2);
            arcToSegments(pt1, pt2, path);
            arcToSegments(pt2, pt1, path);
        }
        else if (e.pad.shape == PadShape::Rectangle)
        {
            MyRect r = rectToMyRect(e.pad, offset);
            IntPoint pt;
            pt.X = r.p1.x(); pt.Y = r.p1.y(); path << pt;
            pt.X = r.p2.x(); pt.Y = r.p2.y(); path << pt;
            pt.X = r.p3.x(); pt.Y = r.p3.y(); path << pt;
            pt.X = r.p4.x(); pt.Y = r.p4.y(); path << pt;
        }
        else if (e.pad.shape == PadShape::Oval)
        {
            Track t = obroundToTrack(e.pad);
            MyRect r = trackToMyRect(t, offset);
            IntPoint pt;
            pt.X = r.p1.x(); pt.Y = r.p1.y(); path << pt;
            arcToSegments(r.p2, r.p3, path);
            pt.X = r.p4.x(); pt.Y = r.p4.y(); path << pt;
            arcToSegments(r.p4, r.p1, path);
        }
    }
    return path;
}

Track Toolpath::obroundToTrack(Pad const& o1)
{
    Track t1;
    QPoint p1,p2;
    if(o1.angle==0)
    {
        if(o1.parameter[0]>=o1.parameter[1])//horizontal
        {
            t1.width=o1.parameter[1];

            p1.setX(o1.point.x()-(o1.parameter[0]-o1.parameter[1])/2);
            p1.setY(o1.point.y());
            p2.setX(o1.point.x()+(o1.parameter[0]-o1.parameter[1])/2);
            p2.setY(o1.point.y());
        }
        else//vertical
        {
            t1.width=o1.parameter[0];

            p1.setX(o1.point.x());
            p1.setY(o1.point.y()-(o1.parameter[1]-o1.parameter[0])/2);
            p2.setX(o1.point.x());
            p2.setY(o1.point.y()+(o1.parameter[1]-o1.parameter[0])/2);
        }
        t1.pointstart=p1;
        t1.pointend=p2;
    }
    else
    {
        qint64 x,y;
        y=(o1.parameter[0]-o1.parameter[1])*qSin(o1.angle)/2;
        x=(o1.parameter[0]-o1.parameter[1])*qCos(o1.angle)/2;

        t1.pointstart.setX(o1.point.x()+x);
        t1.pointstart.setY(o1.point.y()+y);
        t1.pointend.setX(o1.point.x()-x);
        t1.pointend.setY(o1.point.y()-y);
        t1.width=o1.parameter[1];
    }
    return t1;
}

void Toolpath::arcToSegments(QPoint const& p1, QPoint const& p2,Path &path)
{
    double angle = 0.0f;

    if((p1.x()-p2.x())==0)
    {
        if(p1.y()>p2.y())
            angle=M_PI_2;
        else
            angle=M_PI_2*3;
    }
    else if((p1.y()-p2.y())==0)
    {
        if ((p1.x()>p2.x()) == false )
            angle=M_PI;
    }
    else
    {
        angle=qAtan((p1.y()-p2.y())*1.0/(p1.x()-p2.x()));

        if(angle<0)
        {
            if(p1.y()>p2.y())
                angle=angle+M_PI;
        }
        else
        {
            if( (p2.y()<p1.y()) == false )
                angle=angle+M_PI;
        }
    }
    double startAngle = angle;
    double spanAngle =M_PI;

    QPoint c=(p1+p2)/2;
    double r=qSqrt(double(p2.x()-p1.x())*double(p2.x()-p1.x())+double(p2.y()-p1.y())*double(p2.y()-p1.y()))/2;
    double temp=(r-arcError)*1.0/r;//for debug
    double stepAngle=2*acos(temp);
    if(stepAngle==0)stepAngle=0.12566;//50 steps
    for(double a=startAngle;a>startAngle-spanAngle;a-=stepAngle)
    {
        IntPoint t;
        t.X=r*cos(a)+c.x();
        t.Y=r*sin(a)+c.y();
        path<<t;
    }

}

bool Toolpath::bondingRecIntersect(BoundingRect const& r1, BoundingRect const& r2)
{
    //BE CAREFUL OF the DECIMAL 0.001!!
    //Be consit:5-100000-100!!
    //or 6-1000000-1000
    //int precision=5;
    //int precisionScale=100000;
    //int precisionError=100;
    if(r1.right<r2.left- PRECISIONERROR) return false;
    if(r1.top<r2.bottom- PRECISIONERROR) return false;
    if(r1.bottom-r2.top> PRECISIONERROR) return false;
    if(r1.left-r2.right> PRECISIONERROR) return false;
    return true;
}

bool Toolpath::segmentCollision(Element const& e1, Element const& e2)
{
    if(!bondingRecIntersect(e1.boundingRect,e2.boundingRect))
        return false;
    /*
    if(e1.elementType=='T'&&e2.elementType=='P')
        return trackPadCollision(e1.track,e2.pad);
    if(e1.elementType=='T'&&e2.elementType=='T')
        return trackCollision(e1.track,e2.track);
    if(e1.elementType=='P'&&e2.elementType=='T')
        return trackPadCollision(e2.track,e1.pad);
    if(e1.elementType=='P'&&e2.elementType=='P')
        return padCollision(e1.pad,e2.pad);
        */
    return true;
}

bool Toolpath::toolpathIntersects(QList<NetPath> const& nPList,QList<CollisionToolpath> &cTList)
{
    //TODO: fix return statement
    Q_UNUSED(cTList)
    int i,j;
    for(i=0;i<nPList.size();i++)
    {
        NetPath tNetPath=nPList.at(i);
        for(j=i+1;j<nPList.size();j++)//netpath level compare
        {
          NetPath tNetPath1=nPList.at(j);
          if(bondingRecIntersect(tNetPath.boundingRect,tNetPath1.boundingRect)==false)
              continue;
          for(int k=0;k<tNetPath1.pathList.size();k++)//myPath level compare
          {
              MyPath tMyPath1=tNetPath1.pathList.at(k);
              if(bondingRecIntersect(tMyPath1.boundingRect,tNetPath.boundingRect)==false)
                  continue;
              for(int l=0;l<tNetPath.pathList.size();l++)//segment level compare
              {
                  MyPath tMyPath=tNetPath.pathList.at(l);
                  if(bondingRecIntersect(tMyPath1.boundingRect,tMyPath.boundingRect)==false)
                      continue;
                  for(int m=0;m<tMyPath1.segmentList.size();m++)
                  {
                      Segment tSegment1=tMyPath1.segmentList.at(m);

                      for(int n=0;n<tMyPath.segmentList.size();n++)
                      {
                          Segment tSegment=tMyPath.segmentList.at(n);
                      }
                  }
              }
          }
        }
    }
	return false;
}
bool Toolpath::cToolpathIntersects(QList<NetPath> const& nPList,QList<CollisionToolpath> &cTList)
{
    for(int i=0;i<nPList.size();i++)
    {
        NetPath tNetPath=nPList.at(i);
        if(tNetPath.toolpath.empty()) continue;
        Path tPath=tNetPath.toolpath.at(0);
        CollisionToolpath tcTList;
        tcTList.list.append(i);
        for(int j=i+1;j<nPList.size();j++)
        {
            NetPath tNetPath1=nPList.at(j);
            if(tNetPath1.toolpath.empty()) continue;
            Path tPath1=tNetPath1.toolpath.at(0);
            Paths tPaths;
            Clipper c;
            c.AddPath(tPath,ptSubject , true);
            c.AddPath(tPath1,ptClip , true);
            c.Execute(ctIntersection,tPaths,pftNonZero,pftNonZero);

            if(tPaths.size()>0)//collided
            {
                tcTList.list.append(j);
                CollisionPair p;
                p.p1=i;
                p.p2=j;
                tcTList.pair.append(p);
            }
        }
        if(tcTList.list.size()>1)
        {
            bool collisionFlag=false;
            int toolpathNum=0;
            for(int l=0;l<cTList.size();l++)
            {
                CollisionToolpath tcTList1=cTList.at(l);
                for(int k=0;k<tcTList.list.size();k++)
                {
                    int toopathIndex=tcTList.list.at(k);
                    for(int m=0;m<tcTList1.list.size();m++)
                    {
                        int toopathIndex1=tcTList1.list.at(m);
                        if(toopathIndex==toopathIndex1)
                        {
                            collisionFlag=true;
                            break;
                        }
                    }
                    if(collisionFlag==true)
                    {
                        toolpathNum=l;
                        break;
                    }
                }

                if(collisionFlag==true)//collided with existed toolpath,merge
                {
                    //merge two list
                    CollisionToolpath tcTList1=cTList.at(toolpathNum);
                    for(int n=0;n<tcTList1.list.size();n++)
                    {
                        bool sameFlag=false;
                        int num=0;
                        for(int o=0;o<tcTList.list.size();o++)
                        {
                            if(tcTList.list.at(o)==tcTList1.list.at(n))
                            {
                                sameFlag=true;
                                num=o;
                                break;
                            }
                        }
                        if(sameFlag==true)
                            continue;
                        tcTList1.list.append(tcTList.list.at(num));
                    }
                    //and save
                    cTList.replace(l,tcTList1);
                    break;
                }
            }
            if(collisionFlag==false)
            {
                cTList.append(tcTList);
            }
        }
    }
    // just an assumption
    return cTList.isEmpty() == false;
}

Toolpath::Toolpath(Preprocess* p, Setting* s, CuttingParm const& parm) : m_logger(spdlog::get(PROJECT_NAME))
{
    QElapsedTimer timer;
    timer.start();

    auto tool = s->getTool(parm.toolName);
    if (tool && tool->width > 0.0)
    {
        toolDiameter = static_cast<qint64>(tool->calculateWidth(parm.depth) * PRECISIONSCALE);
    }

    m_logger->debug("Toolpath: toolDiameter={} ({} mm)", toolDiameter, toolDiameter / PRECISIONSCALE);

    //int i,j;
    //Net tempPreprocessNetPath;

    //produce toolpath for every element
    //method:offset the shape with the radius of the bit
    //then link the breaking points
    for (Net const& net: p->netList)
    {
        NetPath tempToolPathNetPath;
        //tempPreprocessNetPath = net;
        Paths tempCPaths;
        for (Element const& e : net.elements)
        {
            MyPath tempPath;
            Segment s;
            tempPath.element = e;
            if (e.elementType == ElementType::Track)//track
            {
                MyRect r = trackToMyRect(e.track, toolDiameter);

                s.point = r.p1;
                s.type = 'L';
                tempPath.segmentList.append(s);
                s.point = r.p2;
                s.type = 'C';
                tempPath.segmentList.append(s);
                s.point = r.p3;
                s.type = 'L';
                tempPath.segmentList.append(s);
                s.point = r.p4;
                s.type = 'C';
                tempPath.segmentList.append(s);

                IntPoint point;
                point.X = r.p1.x(); point.Y = r.p1.y();
                tempPath.toolpath << point;
                arcToSegments(r.p2, r.p3, tempPath.toolpath);
                point.X = r.p4.x(); point.Y = r.p4.y();
                tempPath.toolpath << point;
                arcToSegments(r.p4, r.p1, tempPath.toolpath);
            }
            else//pad
            {
                if (e.pad.shape == PadShape::Circle)
                {
                    QPoint point;
                    QPoint point1, point2;
                    point.setX(e.pad.point.x());
                    point.setY(e.pad.point.y() + e.pad.parameter[0] / 2 + toolDiameter / 2);
                    point1 = point;
                    s.point = point;
                    s.type = 'C';
                    tempPath.segmentList.append(s);

                    point.setY(e.pad.point.y() - e.pad.parameter[0] / 2 - toolDiameter / 2);
                    point2 = point;
                    s.point = point;
                    s.type = 'C';
                    tempPath.segmentList.append(s);

                    arcToSegments(point1, point2, tempPath.toolpath);
                    arcToSegments(point2, point1, tempPath.toolpath);
                }
                else if (e.pad.shape == PadShape::Rectangle)
                {
                    MyRect r = rectToMyRect(e.pad, toolDiameter);
                    s.point = r.p1;
                    s.type = 'L';
                    tempPath.segmentList.append(s);
                    s.point = r.p2;
                    s.type = 'L';
                    tempPath.segmentList.append(s);
                    s.point = r.p3;
                    s.type = 'L';
                    tempPath.segmentList.append(s);
                    s.point = r.p4;
                    s.type = 'L';
                    tempPath.segmentList.append(s);

                    IntPoint point;
                    point.X = r.p1.x(); point.Y = r.p1.y();
                    tempPath.toolpath << point;
                    point.X = r.p2.x(); point.Y = r.p2.y();
                    tempPath.toolpath << point;
                    point.X = r.p3.x(); point.Y = r.p3.y();
                    tempPath.toolpath << point;
                    point.X = r.p4.x(); point.Y = r.p4.y();
                    tempPath.toolpath << point;
                }
                else if (e.pad.shape == PadShape::Oval)
                {
                    Track t = obroundToTrack(e.pad);
                    MyRect r = trackToMyRect(t, toolDiameter);
                    s.point = r.p1;
                    s.type = 'L';
                    tempPath.segmentList.append(s);
                    s.point = r.p2;
                    s.type = 'C';
                    tempPath.segmentList.append(s);
                    s.point = r.p3;
                    s.type = 'L';
                    tempPath.segmentList.append(s);
                    s.point = r.p4;
                    s.type = 'C';
                    tempPath.segmentList.append(s);

                    IntPoint point;
                    point.X = r.p1.x(); point.Y = r.p1.y();
                    tempPath.toolpath << point;
                    arcToSegments(r.p2, r.p3, tempPath.toolpath);
                    point.X = r.p4.x(); point.Y = r.p4.y();
                    tempPath.toolpath << point;
                    arcToSegments(r.p4, r.p1, tempPath.toolpath);
                }
            }
            tempCPaths.push_back(tempPath.toolpath);
            tempPath.boundingRect = expandBoundingRect(e.boundingRect, toolDiameter);
            tempToolPathNetPath.pathList.append(tempPath);

        }
        SimplifyPolygons(tempCPaths, tempToolPathNetPath.toolpath, pftNonZero);
        //if (j < 5) // log first 5 nets to avoid log spam
        {
            m_logger->debug("net {} elements={} input paths={} output paths={}", net.netNum, net.elements.size(), tempCPaths.size(), tempToolPathNetPath.toolpath.size());
        }
        netPathList.append(tempToolPathNetPath);
    }
    // Clear all collision flags before re-evaluating so fixed collisions turn blue.
    for (int i = 0; i < p->netList.size(); ++i)
    {
        Net n = p->netList.at(i);
        n.collisionFlag = false;
        p->netList.replace(i, n);
    }

    cToolpathIntersects(netPathList, tpCollisionNum);
    int sum = 0;

    for (auto const& temp : tpCollisionNum) {
        sum += temp.pair.size();
        for (auto const& cp: temp.pair)
        {
            Net n = p->netList.at(cp.p1);
            n.collisionFlag = true;
            p->netList.replace(cp.p1, n);
            n = p->netList.at(cp.p2);
            n.collisionFlag = true;
            p->netList.replace(cp.p2, n);
        }
    }
    m_logger->debug("toolpath collision num={}", sum);
    collisionSum = sum;

    Paths tempPath;
    for (auto const& nnetpath : netPathList)
    {
        if (!nnetpath.toolpath.empty())
        {
            tempPath.push_back(nnetpath.toolpath.at(0));
        }
    }

    SimplifyPolygons(tempPath, totalToolpath, pftNonZero);

    m_logger->debug("totalToolpath paths after SimplifyPolygons: {} (from {} nets, {} non-empty)", totalToolpath.size(), netPathList.size(), tempPath.size());

    for (auto const& nnetpath : netPathList) {
        if (nnetpath.toolpath.size() > 1)
        {
            for (Path const& tp: nnetpath.toolpath)
            {
                totalToolpath.push_back(tp);
            }
        }
    }
    m_logger->debug("totalToolpath ring 0 size: {}", totalToolpath.size());

    // Additional isolation rings: each ring cuts a concentric path further from the traces.
    // Ring N offset = toolDiameter + N * ringStep
    // ringStep = 2 * toolDiameter * (1 - overlap) so adjacent rings are tangent when overlap=0.
    int numRings = std::max(1, parm.isolationRings);
    if (numRings > 1)
    {
        double overlap = tool ? std::min(0.99, std::max(0.0, parm.overlap)) : 0.0;
        qint64 ringStep = static_cast<qint64>(2.0 * toolDiameter * (1.0 - overlap));
        if (ringStep < 1) ringStep = toolDiameter;

        for (int ring = 1; ring < numRings; ++ring)
        {
            qint64 ringOffset = toolDiameter + ring * ringStep;

            Paths ringFirstPaths;
            for (auto const& netPath : netPathList)
            {
                Paths netPaths;
                for (auto const& myPath : netPath.pathList)
                {
                    Path p = elementToPath(myPath.element, ringOffset);
                    if (!p.empty()) netPaths.push_back(p);
                }

                Paths netSimplified;
                SimplifyPolygons(netPaths, netSimplified, pftNonZero);

                if (!netSimplified.empty())
                {
                    ringFirstPaths.push_back(netSimplified.at(0));
                    for (size_t k = 1; k < netSimplified.size(); ++k)
                        totalToolpath.push_back(netSimplified.at(k));
                }
            }

            Paths ringMerged;
            SimplifyPolygons(ringFirstPaths, ringMerged, pftNonZero);
            for (auto& rp : ringMerged)
                totalToolpath.push_back(rp);

            m_logger->debug("ring {} offset={} added {} paths", ring, ringOffset, ringMerged.size());
        }
    }

    m_logger->debug("totalToolpath final size: {}", totalToolpath.size());

    time = timer.elapsed();
}

void Toolpath::generateClearingPaths(CuttingParm const& parm, Paths const& boardBoundary)
{
    if (totalToolpath.empty()) return;
    clearingPaths.clear();

    // Determine board polygon: use supplied boundary if valid, else fall back to
    // isolation-path bounding box + 5 mm margin on each side.
    Paths boardPaths;
    if (!boardBoundary.empty())
    {
        boardPaths = boardBoundary;
    }
    else
    {
        qint64 xMin = totalToolpath[0][0].X, xMax = xMin;
        qint64 yMin = totalToolpath[0][0].Y, yMax = yMin;
        for (const auto& path : totalToolpath)
            for (const auto& pt : path) {
                xMin = std::min(xMin, pt.X); xMax = std::max(xMax, pt.X);
                yMin = std::min(yMin, pt.Y); yMax = std::max(yMax, pt.Y);
            }
        // 5 mm fallback margin
        const qint64 margin = 5000000;
        xMin -= margin; xMax += margin;
        yMin -= margin; yMax += margin;

        Path boardPath;
        boardPath << IntPoint(xMin, yMin) << IntPoint(xMax, yMin)
                  << IntPoint(xMax, yMax) << IntPoint(xMin, yMax);
        boardPaths.push_back(boardPath);
    }

    // Scan bounds from the board polygon
    qint64 xMin = boardPaths[0][0].X, xMax = xMin;
    qint64 yMin = boardPaths[0][0].Y, yMax = yMin;
    for (const auto& bp : boardPaths)
        for (const auto& pt : bp) {
            xMin = std::min(xMin, pt.X); xMax = std::max(xMax, pt.X);
            yMin = std::min(yMin, pt.Y); yMax = std::max(yMax, pt.Y);
        }

    // Use first board path as the board polygon for Clipper
    Path boardPath = boardPaths[0];

    // Expand isolation paths outward by half tool width to get the already-cleared boundary
    Paths expandedPaths;
    try {
        ClipperOffset co;
        for (const auto& ip : totalToolpath)
            co.AddPath(ip, jtRound, etClosedPolygon);
        co.Execute(expandedPaths, static_cast<double>(toolDiameter) / 2.0);
    } catch (...) {
        m_logger->warn("generateClearingPaths: ClipperOffset failed");
        return;
    }

    // clearRegion = board rectangle minus the already-cleared area
    Paths clearRegion;
    try {
        Clipper clipper;
        clipper.AddPath(boardPath, ptSubject, true);
        for (const auto& ep : expandedPaths)
            clipper.AddPath(ep, ptClip, true);
        clipper.Execute(ctDifference, clearRegion, pftNonZero, pftNonZero);
    } catch (...) {
        m_logger->warn("generateClearingPaths: Clipper difference failed");
        return;
    }

    m_logger->debug("clearingPaths: expandedPaths={} clearRegion={}", expandedPaths.size(), clearRegion.size());
    if (clearRegion.empty()) return;

    // Raster fill via scanline edge intersection — no Clipper open-path clipping needed.
    // For each horizontal scan line y, find all X coords where polygon edges cross it,
    // sort them, and take pairs as line segments (even-odd fill rule handles holes).
    double ov = std::min(0.99, std::max(0.0, parm.overlap));
    qint64 step = std::max(qint64(1), static_cast<qint64>(toolDiameter * (1.0 - ov)));

    bool leftToRight = true;
    for (qint64 y = yMin; y <= yMax; y += step)
    {
        std::vector<qint64> xs;
        for (const auto& poly : clearRegion)
        {
            int n = static_cast<int>(poly.size());
            for (int i = 0; i < n; ++i)
            {
                const IntPoint& a = poly[i];
                const IntPoint& b = poly[(i + 1) % n];
                // Edge must strictly cross y (one endpoint below, one above)
                if ((a.Y < y && b.Y >= y) || (b.Y < y && a.Y >= y))
                {
                    double t = static_cast<double>(y - a.Y) / (b.Y - a.Y);
                    xs.push_back(static_cast<qint64>(a.X + t * (b.X - a.X)));
                }
            }
        }

        if (xs.size() < 2) { leftToRight = !leftToRight; continue; }
        std::sort(xs.begin(), xs.end());

        // Reverse pairs for right-to-left passes (boustrophedon)
        if (!leftToRight) std::reverse(xs.begin(), xs.end());

        for (size_t i = 0; i + 1 < xs.size(); i += 2)
        {
            Path seg;
            seg << IntPoint(xs[i], y) << IntPoint(xs[i + 1], y);
            clearingPaths.push_back(seg);
        }

        leftToRight = !leftToRight;
    }

    m_logger->debug("clearingPaths: {} segments", clearingPaths.size());
}

Toolpath::~Toolpath()
{
    //Track t;

}

