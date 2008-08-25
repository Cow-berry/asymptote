// approximate acceleration
triple accel(path3 g, real t)
{
  triple a;
  real deltaT=0.1;
  if(t < deltaT) t=deltaT;
  if(t > 1-deltaT) t=1-deltaT;
  a=dir(g,t+deltaT)-dir(g,t-deltaT);
  return a/deltaT/2;
}

// transformation that bends points along a path
// assumes that p.z is in [0,scale]
triple bend(triple p, path3 g, real scale=1, real endtime=g.length())
{
  real time=arctime(g,arclength(subpath(g,0,endtime))+(p.z-scale));
  triple dir=dir(g,time);

  triple w=point(g,time);

  triple dir2=unit(cross(dir(g,0),dir(g,1)));
  if(abs(dir2) < 1000*realEpsilon) {
    if(abs(dir-X) < 1000*realEpsilon || abs(dir+X) < 1000*realEpsilon)
      dir2=Y;
    else dir2=X;
  }
  dir2=unit(dir2-dot(dir2,dir)*dir);

  triple w=cross(dir,dir2);
  triple q=point(g,time);
  transform3 t=new real[][] {
    {w.x,dir2.x,dir.x,0},
    {w.y,dir2.y,dir.y,0},
    {w.z,dir2.z,dir.z,0},
    {0,0,0,1}
  };
  return shift(q-t*(0,0,p.z))*t*p;
}

real takeStep(path3 s, real endtime, real width)
{
  real a=abs(accel(s,endtime));

  // tweak this:
  real K=1.25/width; // a different constant perhaps ?
  real minStep=1/50; // at most 1/minStep segments for a curve
  real step=max(1/(K*a+1),minStep); // or a different model
  a=abs(accel(s,(min(endtime+step,length(s))+endtime)/2));
  step=max(1/(K*a+1),minStep);
  if(step > 0.25) step=0.25; // guarantee at least 4 segments per curve
  return min(endtime+step,length(s));
}


// return true iff segment i of path3 g is close to being straight
bool checkStraight(path3 g, int i, real straightEpsilon)
{
  triple a = point(g,i);
  triple b = postcontrol(g,i)-a;
  triple c = precontrol(g,i+1);
  triple d = point(g,i+1);
  c = d-c;
  d = d-a;
  return abs(b-project(b,d)) < straightEpsilon &&
    abs(c-project(c,d)) < straightEpsilon;
}

surface tube(path3 g, real width)
{
  surface tube;
  real r=0.5*width;

  transform3 t=scale3(r);

  for(int i=0; i < length(g); ++i) {
    if(straight(g,i) || checkStraight(g,i,r)) {
      triple v=point(g,i);
      triple u=point(g,i+1)-v;
      tube.append(shift(v)*transform3(unit(u))*scale(r,r,abs(u))*unitcylinder);
    } else {
      path3 s=subpath(g,i,i+1);
      real endtime=0;
      while(endtime < 1) {
        real newend=takeStep(s,endtime,r);

        path3 si=subpath(s,endtime,newend);

        real L=arclength(si);
        surface segment=scale(r,r,L)*unitcylinder;
        path3 circle=t*unitcircle3;
        if(endtime == 0)
          segment.push(circle);
        if(newend == 1)
          segment.push(shift((0,0,L))*circle);
        for(patch p : segment.s) {
          for(int i=0; i < 4; ++i) {
            for(int j=0; j < 4; ++j) {
              p.P[i][j]=bend(p.P[i][j],si,L,1);
            }
          }
        }
        tube.s.append(segment.s);

        endtime=newend;
      }
    }

    static real epsilon=sqrt(realEpsilon);

    if(i > 0 && abs(dir(g,i,1)-dir(g,i,-1)) > epsilon)
      tube.append(shift(point(g,i))*t*unitsphere);
  }
  return tube;
}

struct arrowhead3
{
  surface head(path3 g, position position, pen p=currentpen,
               real size=0, real angle=arrowangle);
  real size(pen p)=arrowsize;
}

arrowhead3 DefaultHead3;

DefaultHead3.head=new surface(path3 g, position position=EndPoint,
			      pen p=currentpen, real size=0,
			      real angle=arrowangle) {
  if(size == 0) size=DefaultHead3.size(p);
  bool relative=position.relative;
  real position=position.position.x;
  if(relative) position=reltime(g,position);

  path3 r=subpath(g,position,0);
  real t=arctime(r,size);
  path3 s=subpath(r,t,0);
  real L=arclength(s);
  real wl=Tan(angle);
  real width=L*wl; // make sure this is not zero
  real remainL=L;

  surface head;

  for(int i=0; i < length(s); ++i) {
    path3 s=subpath(s,i,i+1);

    real endtime=0;
    while(endtime < 1) {
      real newend=takeStep(s,endtime,width);

      path3 si=subpath(s,endtime,newend);

      real l=arclength(si);
      real w=remainL*wl;
      surface segment=scale(w,w,l)*unitcylinder;
      if(endtime == 0) // add base
        segment.append(scale(w,w,1)*unitdisk);
      for(patch p : segment.s) {
        for(int i=0; i < 4; ++i) {
          for(int j=0; j < 4; ++j) {
            real k=1-p.P[i][j].z/remainL;
            p.P[i][j]=(k*p.P[i][j].x,k*p.P[i][j].y,p.P[i][j].z);
            p.P[i][j]=bend(p.P[i][j],si,l,1);
          }
        }
      }
      head.append(segment);

      endtime=newend;
      remainL -= l;
    }
  }

  return head;
};

private real position(position position, real size, path3 g, bool center)
{
  bool relative=position.relative;
  real position=position.position.x;
  if(relative) {
    position *= arclength(g);
    if(center) position += 0.5*size;
    position=arctime(g,position);
  } else if(center) 
    position=arctime(g,arclength(subpath(g,0,position))+0.5*size);
  return position;
}

void drawarrow(picture pic, arrowhead3 arrowhead=DefaultHead3,
	       path3 g, pen p=currentpen, pen arrowheadpen=p, real size=0,
	       real angle=arrowangle, position position=EndPoint,
	       bool forwards=true, bool center=false)
{
  if(arrowheadpen == nullpen) arrowheadpen=p;
  if(size == 0) size=arrowhead.size(p);
  size=min(arrowsizelimit*arclength(g),size);
  real position=position(position,size,g,center);

  int L=length(g);
  if(!forwards) {
    g=reverse(g);
    position=L-position;
  }
  path3 r=subpath(g,position,0);
  size=min(arrowsizelimit*arclength(r),size);
  if(position == L)
      draw(pic,subpath(r,arctime(r,size),length(r)),p);
  else draw(pic,g,p);
  draw(pic,arrowhead.head(g,position,p,size,angle),arrowheadpen);
 }

void drawarrow2(picture pic, arrowhead3 arrowhead=DefaultHead3,
		path3 g, pen p=currentpen, pen arrowheadpen=p,
		real size=0, real angle=arrowangle)
{
  if(arrowheadpen == nullpen) arrowheadpen=p;
  if(size == 0) size=arrowhead.size(p);
  size=min(arrow2sizelimit*arclength(g),size);

  path3 r=reverse(g);
  int L=length(g);
  draw(pic,subpath(r,arctime(r,size),L-arctime(g,size)),p);
  draw(pic,arrowhead.head(g,L,p,size,angle),arrowheadpen);
  draw(pic,arrowhead.head(r,L,p,size,angle),arrowheadpen);
}

// Add to picture an estimate of the bounding box contribution of arrowhead
// using the local slope at endpoint.
void addArrow(picture pic, arrowhead3 arrowhead, path3 g, pen p, real size,
	      real angle, real position)
{
  triple v=point(g,position);
  path3 g=v-(size+linewidth(p))*dir(g,position)--v;
  surface s=arrowhead.head(g,position,p,size,angle);
  pic.addPoint(v,min(s)-v);
  pic.addPoint(v,max(s)-v);
}

picture arrow(arrowhead3 arrowhead=DefaultHead3,
              path3 g, pen p=currentpen, pen arrowheadpen=p, real size=0,
              real angle=arrowangle, position position=EndPoint,
              bool forwards=true, bool center=false)
{
  if(size == 0) size=DefaultHead3.size(p);
  picture pic;
  pic.add(new void(picture f, transform3 t) {
      drawarrow(f,arrowhead,t*g,p,arrowheadpen,size,angle,position,
		forwards,center);
    });

  addPath(pic,g,p);

  real position=position(position,size,g,center);
  path3 G;
  if(!forwards) {
    G=reverse(g);
    position=length(g)-position;
  } else G=g;
  addArrow(pic,arrowhead,G,p,size,angle,position);

  return pic;
}

picture arrow2(arrowhead3 arrowhead=DefaultHead3,
               path3 g, pen p=currentpen, pen arrowheadpen=p,
	       real size=0, real angle=arrowangle)
{
  if(size == 0) size=DefaultHead3.size(p);
  picture pic;
  pic.add(new void(picture f, transform3 t) {
      drawarrow2(f,arrowhead,t*g,p,arrowheadpen,size,angle);
    });
  
  addPath(pic,g,p);

  int L=length(g);
  addArrow(pic,arrowhead,g,p,size,angle,L);
  addArrow(pic,arrowhead,reverse(g),p,size,angle,L);

  return pic;
}

typedef bool arrowbar3(picture, path3, pen);

bool Blank(picture pic, path3 g, pen p) {
  return false;
}

bool None(picture pic, path3 g, pen p) {
  return true;
}

arrowbar3 BeginArrow3(arrowhead3 arrowhead=DefaultHead3,
		      real size=0, real angle=arrowangle,
		      position position=BeginPoint, pen arrowheadpen=nullpen)
{
  return new bool(picture pic, path3 g, pen p) {
    add(pic,arrow(arrowhead,g,p,arrowheadpen,size,angle,position,
		  forwards=false));
    return false;
  };
}

arrowbar3 Arrow3(arrowhead3 arrowhead=DefaultHead3,
		 real size=0, real angle=arrowangle,
		 position position=EndPoint, pen arrowheadpen=nullpen)

{
  return new bool(picture pic, path3 g, pen p) {
    add(pic,arrow(arrowhead,g,p,arrowheadpen,size,angle,position));
    return false;
  };
}

arrowbar3 EndArrow3(arrowhead3 arrowhead=DefaultHead3,
		    real size=0, real angle=arrowangle,
		    position position=EndPoint,
		    pen arrowheadpen=nullpen)=Arrow3;

arrowbar3 MidArrow3(arrowhead3 arrowhead=DefaultHead3,
		    real size=0, real angle=arrowangle,
		    pen arrowheadpen=nullpen)
{
  return new bool(picture pic, path3 g, pen p) {
    add(pic,arrow(arrowhead,g,p,arrowheadpen,size,angle,MidPoint,center=true));
    return false;
  };
}

arrowbar3 Arrows3(arrowhead3 arrowhead=DefaultHead3,
		  real size=0, real angle=arrowangle, pen arrowheadpen=nullpen)

{
  return new bool(picture pic, path3 g, pen p) {
    add(pic,arrow2(arrowhead,g,p,arrowheadpen,size,angle));
    return false;
  };
}

arrowbar3 BeginArrow3=BeginArrow3(),
MidArrow3=MidArrow3(),
Arrow3=Arrow3(),
EndArrow3=Arrow3(),
Arrows3=Arrows3();