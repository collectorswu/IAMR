
//
// $Id: CoordSys.cpp,v 1.6 1997-10-01 01:03:03 car Exp $
//

#ifdef BL_USE_NEW_HFILES
#include <cmath>
#else
#include <math.h>
#endif

#include <Misc.H>
#include <Utility.H>
#include <CoordSys.H>
#include <ParmParse.H>
#include <COORDSYS_F.H>

const double RZFACTOR = 2*4*atan(1.0);

CoordSys::CoordType  CoordSys::c_sys = CoordSys::undef;
REAL CoordSys::offset[BL_SPACEDIM];

#define DEF_LIMITS(fab,fabdat,fablo,fabhi)   \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
REAL* fabdat = (fab).dataPtr();

#define DEF_CLIMITS(fab,fabdat,fablo,fabhi)  \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
const REAL* fabdat = (fab).dataPtr();

// ---------------------------------------------------------------
// Static functions
// ---------------------------------------------------------------
void
CoordSys::SetCoord(CoordType coord)
{
    c_sys = coord;
}
// ---------------------------------------------------------------
void
CoordSys::SetOffset( const REAL* x_lo )
{
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	offset[k] = x_lo[k];
    }
}
// ---------------------------------------------------------------
int
CoordSys::IsRZ()
{
    assert(c_sys != undef);
#if (BL_SPACEDIM == 2)
    return (c_sys == RZ);
#endif    
#if (BL_SPACEDIM == 3)
    return 0;
#endif    
}
// ---------------------------------------------------------------
int
CoordSys::IsCartesian()
{
    assert(c_sys != undef);
    return (c_sys == cartesian);
}



// ---------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------
CoordSys::CoordSys()
{
    ok = 0;
}
// ---------------------------------------------------------------
CoordSys::CoordSys( const REAL* cell_dx)
{
    define(cell_dx);
}

// ---------------------------------------------------------------
void
CoordSys::define( const REAL* cell_dx)
{
    assert(c_sys != undef);
    ok = 1;
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	dx[k] = cell_dx[k];
    }
}

// ---------------------------------------------------------------
void
CoordSys::CellCenter(const IntVect& point, Array<REAL>& loc) const
{
    assert(ok);
    loc.resize(BL_SPACEDIM);
    CellCenter(point,&(loc[0]));
}

// ---------------------------------------------------------------
void
CoordSys::CellCenter(const IntVect& point, REAL *loc) const
{
    assert(ok);
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	loc[k] = offset[k] + dx[k]*(0.5+ (REAL)point[k]);
    }
}

// ---------------------------------------------------------------
REAL
CoordSys::CellCenter(int point, int dir) const
{
    assert(ok);
    return offset[dir] + dx[dir]*(0.5+ (REAL)point);
}

// ---------------------------------------------------------------
REAL
CoordSys::LoEdge(int point, int dir) const
{
    assert(ok);
    return offset[dir] + dx[dir]*point;
}

// ---------------------------------------------------------------
REAL
CoordSys::LoEdge(const IntVect& point, int dir) const
{
    assert(ok);
    return offset[dir] + dx[dir]*point[dir];
}

// ---------------------------------------------------------------
REAL
CoordSys::HiEdge(int point, int dir) const
{
    assert(ok);
    return offset[dir] + dx[dir]*(point + 1);
}

// ---------------------------------------------------------------
REAL
CoordSys::HiEdge(const IntVect& point, int dir) const
{
    assert(ok);
    return offset[dir] + dx[dir]*(point[dir] + 1);
}

// ---------------------------------------------------------------
void
CoordSys::LoFace(const IntVect& point, int dir, Array<REAL>& loc) const
{
    loc.resize(BL_SPACEDIM);
    LoFace(point,dir,&(loc[0]));
}

// ---------------------------------------------------------------
void
CoordSys::LoFace(const IntVect& point, int dir, REAL *loc) const
{
    assert(ok);
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	REAL off = 0.5;
	if (k == dir) off = 0.0;
	loc[k] = offset[k] + dx[k]*(off + (REAL)point[k]);
    }
}

// ---------------------------------------------------------------
void
CoordSys::HiFace(const IntVect& point, int dir, Array<REAL>& loc) const
{
    loc.resize(BL_SPACEDIM);
    HiFace(point,dir,&(loc[0]));
}

// ---------------------------------------------------------------
void
CoordSys::HiFace(const IntVect& point, int dir, REAL *loc) const
{
    assert(ok);
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	REAL off = 0.5;
	if (k == dir) off = 1.0;
	loc[k] = offset[k] + dx[k]*(off + (REAL)point[k]);
    }
}

// ---------------------------------------------------------------
void
CoordSys::LoNode(const IntVect& point, Array<REAL>& loc) const
{
    loc.resize(BL_SPACEDIM);
    LoNode(point,&(loc[0]));
}

// ---------------------------------------------------------------
void
CoordSys::LoNode(const IntVect& point, REAL *loc) const
{
    assert(ok);
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	loc[k] = offset[k] + dx[k]*point[k];
    }
}

// ---------------------------------------------------------------
void
CoordSys::HiNode(const IntVect& point, Array<REAL>& loc) const
{
    loc.resize(BL_SPACEDIM);
    HiNode(point,&(loc[0]));
}

// ---------------------------------------------------------------
void
CoordSys::HiNode(const IntVect& point, REAL *loc) const
{
    assert(ok);
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	loc[k] = offset[k] + dx[k]*(point[k] + 1);
    }
}

// ---------------------------------------------------------------
IntVect
CoordSys::CellIndex(const REAL* point) const
{
    assert(ok);
    IntVect ix;
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	ix[k] = (int) ((point[k]-offset[k])/dx[k]);
    }
    return ix;
}

// ---------------------------------------------------------------
IntVect
CoordSys::LowerIndex(const REAL* point) const
{
    assert(ok);
    IntVect ix;
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	ix[k] = (int) ((point[k]-offset[k])/dx[k]);
    }
    return ix;
}

// ---------------------------------------------------------------
IntVect
CoordSys::UpperIndex(const REAL* point) const
{
    assert(ok);
    IntVect ix;
    int k;
    for (k = 0; k < BL_SPACEDIM; k++) {
	ix[k] = (int) ((point[k]-offset[k])/dx[k]);
    }
    return ix;
}

    
// ---------------------------------------------------------------
FArrayBox*
CoordSys::GetVolume (const BOX& region) const 
{
    FArrayBox *vol = new FArrayBox();
    GetVolume(*vol,region);
    return vol;
}

// ---------------------------------------------------------------
void
CoordSys::GetVolume (FArrayBox& vol, const BOX& region) const 
{
    assert(ok);
    assert(region.cellCentered());
    
    vol.resize(region,1);
    DEF_LIMITS(vol,vol_dat,vlo,vhi);
    int coord = (int) c_sys;
    FORT_SETVOL(vol_dat,ARLIM(vlo),ARLIM(vhi),offset,dx,&coord);
}

// ---------------------------------------------------------------
#if (BL_SPACEDIM == 2)
FArrayBox*
CoordSys::GetDLogA (const BOX& region, int dir) const
{
    FArrayBox *dloga = new FArrayBox();
    GetDLogA(*dloga,region,dir);
    return dloga;
}

// ---------------------------------------------------------------
void
CoordSys::GetDLogA (FArrayBox& dloga, const BOX& region, int dir) const
{
    assert(ok);
    assert(region.cellCentered());

    dloga.resize(region,1);
    DEF_LIMITS(dloga,dloga_dat,dlo,dhi);
    int coord = (int) c_sys;
    FORT_SETDLOGA(dloga_dat,ARLIM(dlo),ARLIM(dhi),offset,dx,&dir,&coord);
}
#endif

// ---------------------------------------------------------------
FArrayBox*
CoordSys::GetFaceArea (const BOX& region, int dir) const 
{
    FArrayBox *area = new FArrayBox();
    GetFaceArea(*area,region,dir);
    return area;
}

// ---------------------------------------------------------------
void
CoordSys::GetFaceArea (FArrayBox& area, 
		       const BOX& region, int dir) const
{
    assert(ok);
    assert(region.cellCentered());
    
    BOX reg(region);
    reg.surroundingNodes(dir);

    area.resize(reg,1);
    DEF_LIMITS(area,area_dat,lo,hi)
    int coord = (int) c_sys;
    FORT_SETAREA(area_dat,ARLIM(lo),ARLIM(hi),offset,dx,&dir,&coord);
}

// ---------------------------------------------------------------
void
CoordSys::GetEdgeLoc( Array<REAL>& loc, 
		      const BOX& region, int dir) const 
{
    assert(ok);
    assert(region.cellCentered());

    const int* lo = region.loVect();
    const int* hi = region.hiVect();
    int len = hi[dir] - lo[dir] + 2;
    loc.resize(len);
    REAL off = offset[dir] + dx[dir]*lo[dir];
    int i;
    for (i = 0; i < len; i++) {
	loc[i] = off + dx[dir]*i;
    }
}

// ---------------------------------------------------------------
void
CoordSys::GetCellLoc( Array<REAL>& loc, 
		      const BOX& region, int dir) const
{
    assert(ok);
    assert(region.cellCentered());

    const int* lo = region.loVect();
    const int* hi = region.hiVect();
    int len = hi[dir] - lo[dir] + 1;
    loc.resize(len);
    REAL off = offset[dir] + dx[dir]*(0.5 + (REAL)lo[dir]);
    int i;
    for (i = 0; i < len; i++) {
	loc[i] = off + dx[dir]*i;
    }
}

// ---------------------------------------------------------------
void
CoordSys::GetEdgeVolCoord( Array<REAL>& vc,
			   const BOX& region, int dir) const
{
      // in cartesian and Z direction of RZ volume coordinates
      // are idential to physical distance from axis
    GetEdgeLoc(vc,region,dir);

      // in R direction of RZ, vol coord = (r^2)/2
#if (BL_SPACEDIM == 2)
    if (dir == 0 && c_sys == RZ) {
	int len = vc.length();
        int i;
	for (i = 0; i < len; i++) {
	    REAL r = vc[i];
	    vc[i] = 0.5*r*r;
	}
    }
#endif    
}

// ---------------------------------------------------------------
void
CoordSys::GetCellVolCoord( Array<REAL>& vc,
			   const BOX& region, int dir) const
{
      // in cartesian and Z direction of RZ volume coordinates
      // are idential to physical distance from axis
    GetCellLoc(vc,region,dir);

      // in R direction of RZ, vol coord = (r^2)/2
#if (BL_SPACEDIM == 2)
    if (dir == 0 && c_sys == RZ) {
	int len = vc.length();
        int i;
	for (i = 0; i < len; i++) {
	    REAL r = vc[i];
	    vc[i] = 0.5*r*r;
	}
    }
#endif    
}

// --------------------------------------------------------------
ostream& operator << (ostream& os, const CoordSys& c)
{
    os << '(' << (int) c.c_sys << ' ';

    os << D_TERM( '(' << c.offset[0] , <<
                  ',' << c.offset[1] , <<
                  ',' << c.offset[2])  << ')';
    os << D_TERM( '(' << c.dx[0] , <<
                  ',' << c.dx[1] , <<
                  ',' << c.dx[2])  << ')';
    os << ' ' << c.ok << ")\n";

    return os;
}

// --------------------------------------------------------------
istream& operator >> (istream& is, CoordSys& c)
{
    int coord;
    is.ignore(BL_IGNORE_MAX, '(') >> coord;
    c.c_sys = (CoordSys::CoordType) coord;
    D_EXPR(is.ignore(BL_IGNORE_MAX, '(') >> c.offset[0],
	   is.ignore(BL_IGNORE_MAX, ',') >> c.offset[1],
	   is.ignore(BL_IGNORE_MAX, ',') >> c.offset[2]);
    is.ignore(BL_IGNORE_MAX, ')');
    D_EXPR(is.ignore(BL_IGNORE_MAX, '(') >> c.dx[0],
	   is.ignore(BL_IGNORE_MAX, ',') >> c.dx[1],
	   is.ignore(BL_IGNORE_MAX, ',') >> c.dx[2]);
    is.ignore(BL_IGNORE_MAX, ')');
    is >> c.ok;
    is.ignore(BL_IGNORE_MAX, '\n');
    return is;
}

// functions to return geometric information about a single cell
REAL CoordSys::Volume(const IntVect& point) const
{
    REAL xhi[BL_SPACEDIM];
    REAL xlo[BL_SPACEDIM];
    HiNode(point,xhi);
    LoNode(point,xlo);
    return Volume(xlo,xhi);
}

REAL 
CoordSys::Volume(const REAL xlo[BL_SPACEDIM], 
		 const REAL xhi[BL_SPACEDIM]) const
{
    switch(c_sys){
    case cartesian:
        return (xhi[0]-xlo[0])
#if (BL_SPACEDIM>=2)                       
                       *(xhi[1]-xlo[1])
#endif
#if (BL_SPACEDIM>=3)                       
                       *(xhi[2]-xlo[2])
#endif
                       ;
#if (BL_SPACEDIM==2)
    case RZ:
        return (0.5*RZFACTOR)*(xhi[1]-xlo[1])*(xhi[0]*xhi[0]-xlo[0]*xlo[0]);
#endif
    default:
        assert(0);
    }
    return 0;
}                      

REAL CoordSys::AreaLo(const IntVect& point, int dir) const
{
#if (BL_SPACEDIM==2)
    REAL xlo[BL_SPACEDIM];
    switch( c_sys ){
    case cartesian:
        switch(dir){
        case 0:
            return dx[1];
        case 1:
            return dx[0];
        }
    case RZ:
        LoNode(point,xlo);
        switch(dir){
        case 0:
            return RZFACTOR*dx[1]*xlo[0];
        case 1:
            return ((xlo[0]+dx[0])*(xlo[0]+dx[0])-xlo[0]*xlo[0])*
                   (0.5*RZFACTOR);
        }
    default:
        assert(0);
    }
#endif
#if (BL_SPACEDIM==3)
    switch(dir){
    case 0:
        return dx[1]*dx[2];
    case 1:
        return dx[0]*dx[2];
    case 2:
        return dx[1]*dx[0];
    }
#endif
    return 0;
}


REAL CoordSys::AreaHi(const IntVect& point, int dir) const
{
#if (BL_SPACEDIM==2)
    REAL xhi[BL_SPACEDIM];
    switch( c_sys ){
    case cartesian:
        switch(dir){
        case 0:
            return dx[1];
        case 1:
            return dx[0];
        }
    case RZ:
        HiNode(point,xhi);
        switch(dir){
        case 0:
            return RZFACTOR*dx[1]*xhi[0];
        case 1:
            return (xhi[0]*xhi[0]-(xhi[0]-dx[0])*(xhi[0]-dx[0]))*
                   (RZFACTOR*0.5);
        }
    default:
        assert(0);
    }
#endif
#if (BL_SPACEDIM==3)
    switch(dir){
    case 0:
        return dx[1]*dx[2];
    case 1:
        return dx[0]*dx[2];
    case 2:
        return dx[1]*dx[0];
    }
#endif
    return 0;
}
