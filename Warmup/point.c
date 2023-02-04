#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
    point_set(p,point_X(p)+x,point_Y(p)+y);
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    double distance,p1x,p1y,p2x,p2y;
    p1x=point_X(p1);
    p1y=point_Y(p1);
    p2x=point_X(p2);
    p2y=point_Y(p2);
    distance=sqrt(pow((p2y-p1y),2)+pow((p2x-p1x),2));
    return distance;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    double length1,length2;
    length1=sqrt(pow(point_X(p1),2)+pow(point_Y(p1),2));
    length2=sqrt(pow(point_X(p2),2)+pow(point_Y(p2),2));
    int compare;
    if(length1<length2){
        compare=-1;
    }
    else if(length1==length2){
        compare=0;
    }
    else{
        compare=1;
    }
    return compare;
}
