/*  
    fgens.c:

    Copyright (C) 1991, 1994, 1995, 1998, 2000 
                  Barry Vercoe, John ffitch, Paris Smaragdis, Gabriel Maldonado,
                  Richard Karpen, Greg Sullivan, Pete Moss, Istvan Varga

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "cs.h"                 /*                              FGENS.C         */
#include <ctype.h>
#include "soundio.h"
#include "cwindow.h"
#include <math.h>
#include "cmath.h"
#include "ftgen.h"
#include "fft.h"

#define GENMAX  42

extern OPARMS  O;
/* New function in FILOPEN.C to look around for (text) files */
extern FILE *fopenin(char* filnam);
extern double besseli(double);
/* extern void vco2_tables_destroy(void); */

typedef void    (*GEN)(void);

static void   gen01(void), gen01raw(void), gen02(void), gen03(void), gen04(void),
              gen05(void);
static void   gen06(void), gen07(void), gen08(void), gen09(void), gen10(void);
static void   gen11(void), gen12(void), gen13(void), gen14(void), gen15(void);
static void  gn1314(void), gen17(void), gen18(void), gen19(void), gen20(void);
static void   gen21(void), gen23(void), gen24(void), gen16(void);
static void   gen25(void), gen27(void), gen28(void), gen30(void), gen31(void);
static void   gen32(void), gen33(void), gen34(void), gen40(void), gen41(void);
static void   gen42(void);

static void   GENUL(void);

FUNC    **flist=NULL /*[MAXFNUM+1]*/, *ftp;
static  int     maxfnum = 0;
static  GEN     gensub[GENMAX+1] = { GENUL,
                                     gen01, gen02, gen03, gen04, gen05,
                                     gen06, gen07, gen08, gen09, gen10,
                                     gen11, gen12, gen13, gen14, gen15,
                                     gen16, gen17, gen18, gen19, gen20,
                                     gen21, GENUL, gen23, gen24, gen25,
                                     GENUL, gen27, gen28, GENUL, gen30,
                                     gen31, gen32, gen33, gen34, GENUL,
                                     GENUL, GENUL, GENUL, GENUL, gen40,
                                     gen41, gen42 };
static  EVTBLK  *e;

static  double  tpdlen, tpd360 = 0.017453293;
static  int     fno, guardreq, nargs, fterrcnt;
static  long    flen, flenp1, lenmask;
static  void    fterror(char *), ftresdisp(void), ftalloc(void);

#define FTERR(s)        {fterror(s);  return;}

#define FTPMAX  (150)
static EVTBLK *ftevt = NULL;

void ftRESET(void)
{
    int i;
    ftp     = NULL;
    e       = NULL;
    if (flist) {
      for (i = 1; i <= maxfnum; i++)
		  mfree(flist[i]);   /* Check this */
      mfree(flist);
      flist   = NULL;
    }
    maxfnum = 0;
    if (ftevt) {
      mfree(ftevt);
      ftevt = NULL;
    }
/*     vco2_tables_destroy(); */
}

static void   GENUL(void)
{
    FTERR(Str(X_513,"unknown GEN number"));
}

void fgens(EVTBLK *evtblkp)     /* create ftable using evtblk data */
{
    long        ltest, lobits, lomod, genum;

    e = evtblkp;
    fterrcnt = 0;
    if ((fno = (int)e->p[1]) < 0) {                     /* fno < 0: remove */
      if ((fno = -fno) > maxfnum)
        FTERR(Str(X_849,"illegal ftable number"))
      if ((ftp = flist[fno]) == NULL)
        FTERR(Str(X_783,"ftable does not exist"))
      flist[fno] = NULL;
      mfree((char *)ftp);
      printf(Str(X_779,"ftable %d now deleted\n"),fno);
      return;
    }
    if (!fno)                           /* fno = 0, return      */
      return;
    if (fno > maxfnum) {
      int size = maxfnum;
      FUNC **nn;
      int i;
      while (fno >= size)
        size += MAXFNUM;
      size++;
/*       if (maxfnum!=0) */
/*         printf(Str(X_307,"Increasing number of tables from %d to %d\n"), */
/*                maxfnum, size-1); */
      nn = (FUNC**)mrealloc(flist, size*sizeof(FUNC*));
      flist = nn;
      for (i=maxfnum+1; i<size; i++) flist[i] = NULL; /* Clear new section */
      maxfnum = size-1;
/*           FTERR(X_849,"illegal ftable number") */
    }
    if ((nargs = e->pcnt - 4) <= 0)             /* chk minimum arg count */
      FTERR(Str(X_941,"insufficient gen arguments"))
      if ((genum = (long)e->p[4]) < 0)
        genum = -genum;
    if (!genum || genum > GENMAX)               /*   & legal gen number */
      FTERR(Str(X_850,"illegal gen number"))
    if ((flen = (long)(e->p[3]+0.5))) {         /* if user flen given       */
      if (flen < 0 ) { /* gab for non-pow-of-two-length */
        flen = labs(flen);
        lenmask = 0xFFFFFFFF;
        lobits = 0;    /* Hope this is not needed! */
      }
      else {
        guardreq = flen & 01;                   /*   set guard request flg  */
        flen &= -2;                             /*   flen now w/o guardpt   */
        flenp1 = flen + 1;                      /*   & flenp1 with guardpt  */
        if (flen <= 0 || flen > MAXLEN)
          FTERR(Str(X_889,"illegal table length"))
        for (ltest=flen,lobits=0; (ltest & MAXLEN)==0; lobits++,ltest<<=1);
        if (ltest != MAXLEN)              /*   flen must be power-of-2 */
          FTERR(Str(X_889,"illegal table length"))
        lenmask = flen-1;
      }
      ftalloc();                                /*   alloc ftable space now */
      ftp->flen = flen;
      ftp->lenmask = lenmask;                   /*   init hdr w powof2 data */
      ftp->lobits = lobits;
      lomod = MAXLEN / flen;
      ftp->lomask = lomod - 1;
      ftp->lodiv = FL(1.0)/((MYFLT)lomod);      /*    & other useful vals    */
      tpdlen = TWOPI_F / flen;
      ftp->nchanls = 1;                         /*    presume mono for now   */
      ftp->flenfrms = flen;
    }
    else if (genum != 1 && genum != 23 && genum != 28)
      /* else defer alloc to gen01|gen23|gen28 */
      FTERR(Str(X_684,"deferred size for GEN1 only"))
    else ftp = NULL;           /* Ensure a null pointer */
    printf(Str(X_782,"ftable %d:\n"), fno);
    (*gensub[genum])();                 /* call gen subroutine  */
    if (!fterrcnt)
      ftresdisp();                      /* rescale and display */
}

static void gen02(void)             /* read ftable values directly from p-args */
{
    MYFLT   *fp = ftp->ftable, *pp = &e->p[5];
    int     nvals = nargs;

    if (nvals > flenp1)
      nvals = flenp1;                 /* for all vals up to flen+1 */
    do  *fp++ = *pp++;                /*   copy into ftable   */
    while (--nvals);
}

static void gen03(void)
{
    int         ncoefs;
    MYFLT       xintvl, xscale;
    int         xloc, nlocs;
    MYFLT       *fp = ftp->ftable, x, sum, *coefp, *coef0, *coeflim;

    if ((ncoefs = nargs - 2) <= 0)
      FTERR(Str(X_1027,"no coefs present"));
    coef0 = &e->p[7];
    coeflim = coef0 + ncoefs;
    if ((xintvl = e->p[6] - e->p[5]) <= 0)
      FTERR(Str(X_892,"illegal x interval"));
    xscale = xintvl / (MYFLT)flen;
    xloc = (int)(e->p[5] / xscale);             /* initial xloc */
    nlocs = flenp1;
    do {                                        /* for each loc:        */
      x = xloc++ * xscale;
      coefp = coeflim;
      sum = *--coefp;                           /* init sum to coef(n)  */
      while (coefp > coef0) {
        sum *= x;                               /*  & accum by Horner's rule */
        sum += *--coefp;
      }
      *fp++ = sum;
    } while (--nlocs);
}

static void gen04(void)
{
    MYFLT   *valp, *rvalp, *fp = ftp->ftable;
    int     n, r;
    FUNC    *srcftp;
    MYFLT   val, max, maxinv;
    int     srcno, srcpts, ptratio;

    if (nargs < 2)
      FTERR(Str(X_939,"insufficient arguments"))
    if ((srcno = (int)e->p[5]) <= 0 || srcno > maxfnum ||
        (srcftp = flist[srcno]) == NULL)
      FTERR(Str(X_1344,"unknown srctable number"))
    if (!e->p[6]) {
      srcpts = srcftp->flen;
      valp = &srcftp->ftable[0];
      rvalp = NULL;
    }
    else {
      srcpts = srcftp->flen >>1;
      valp = &srcftp->ftable[srcpts];
      rvalp = valp - 1;
    }
    if ((ptratio = srcpts / flen) < 1)
      FTERR(Str(X_1263,"table size too large"))
    if ((val = *valp++)) {
      if (val < FL(0.0))      val = -val;
      max = val;
      maxinv = FL(1.0) / max;
    }
    else {
      max = FL(0.0);
      maxinv = FL(1.0);
    }
    *fp++ = maxinv;
    for (n = flen; n--; ) {
      for (r = ptratio; r--; ) {
        if ((val = *valp++)) {
          if (val < FL(0.0))      val = -val;
          if (val > max) {
            max = val;
            maxinv = FL(1.0) / max;
          }
        }
        if (rvalp != NULL && (val = *rvalp--)) {
          if (val < 0.)   val = -val;
          if (val > max) {
            max = val;
            maxinv = FL(1.0) / max;
          }
        }
      }
      *fp++ = maxinv;
    }
    guardreq = 1;                   /* disable new guard point */
    e->p[4] = -FL(4.0);             /*   and rescaling         */
}

static void gen05(void)
{
    int     nsegs, seglen;
    MYFLT   *valp, *fp, *finp;
    MYFLT   amp1, mult;

    if ((nsegs = (nargs - 1) >> 1) <= 0)         /* nsegs = nargs-1 /2 */
      return;
    valp = &e->p[5];
    fp = ftp->ftable;
    finp = fp + flen;
    if (*valp == 0) goto gn5er2;
    do {
      amp1 = *valp++;
      if (!(seglen = (int)*valp++)) continue;
      if (seglen < 0) goto gn5er1;
      if ((mult = *valp/amp1) <= 0) goto gn5er2;
      mult = (MYFLT)pow( (double)mult, 1.0/(double)seglen );
      while (seglen--) {
        *fp++ = amp1;
        amp1 *= mult;
        if (fp > finp) return;
      }
    } while (--nsegs);
    if (fp == finp)                 /* if 2**n pnts, add guardpt */
      *fp = amp1;
    return;

 gn5er1:
    fterror(Str(X_795,"gen call has negative segment size:"));
    return;
 gn5er2:
    fterror(Str(X_859,"illegal input vals for gen call, beginning:"));
}

static void gen07(void)
{
    int     nsegs, seglen;
    MYFLT   *valp, *fp, *finp;
    MYFLT   amp1, incr;

    if ((nsegs = (nargs - 1) >> 1) <= 0)         /* nsegs = nargs-1 /2 */
      return;
    valp = &e->p[5];
    fp = ftp->ftable;
    finp = fp + flen;
    do {
      amp1 = *valp++;
      if (!(seglen = (int)*valp++)) continue;
      if (seglen < 0) goto gn7err;
      incr = (*valp - amp1) / seglen;
      while (seglen--) {
        *fp++ = amp1;
        amp1 += incr;
        if (fp > finp) return;
      }
    } while (--nsegs);
    if (fp == finp)                 /* if 2**n pnts, add guardpt */
      *fp = amp1;
    return;

 gn7err:
    fterror(Str(X_795,"gen call has negative segment size:"));
}

static void gen06(void)
{
    MYFLT   *segp, *extremp, *inflexp, *segptsp, *fp, *finp;
    MYFLT   y, diff2;
    int     pntno, pntinc, nsegs, npts;

    if ((nsegs = (nargs - 1) >>1) < 1)
      FTERR(Str(X_939,"insufficient arguments"))
    fp = ftp->ftable;
    finp = fp + flen;
    pntinc = 1;
    for (segp = &e->p[3]; nsegs > 0; nsegs--) {
      segp += 2;
      segptsp = segp + 1;
      if ((npts = (int)*segptsp) < 0)
        FTERR(Str(X_1011,"negative segsiz"))
          if (pntinc > 0) {
            pntno = 0;
            inflexp = segp + 2;
            extremp = segp;
          }
          else {
            pntno = npts;
            inflexp = segp;
            extremp = segp + 2;
          }
      diff2 = (*inflexp - *extremp) * FL(0.5);
      for ( ; npts > 0 && fp < finp; pntno += pntinc, npts--) {
        y = (MYFLT)pntno / *segptsp;
        *fp++ = (FL(3.0)-y) * y * y * diff2 + *extremp;
      }
      pntinc = -pntinc;
    }
    *fp = *(segp + 2);                      /* write last target point */
}

static void gen08(void)
{
    MYFLT   R, x, c3, c2, c1, c0, *fp, *fplim, *valp;
    MYFLT   f2 = FL(0.0), f1, f0, df1, df0, dx01, dx12 = FL(0.0), curx;
    MYFLT   slope, resd1, resd0;
    int     nsegs, npts;

    if ((nsegs = (nargs - 1) >>1) <= 0)
      FTERR(Str(X_939,"insufficient arguments"));
    valp = &e->p[5];
    fp = ftp->ftable;
    fplim = fp + flen;
    f0 = *valp++;                   /* 1st 3 params give vals at x0, x1  */
    if ((dx01 = *valp++) <= FL(0.0))        /*      and dist between     */
      FTERR(Str(X_892,"illegal x interval"));
    f1 = *valp++;
    curx = df0 = FL(0.0);           /* init x to origin; slope at x0 = 0 */
    do {                            /* for each spline segmnt (x0 to x1) */
      if (nsegs > 1) {                    /* if another seg to follow  */
        MYFLT dx02;
        if ((dx12 = *valp++) <= FL(0.0))  /*    read its distance      */
          FTERR(Str(X_892,"illegal x interval"));
        f2 = *valp++;                   /*    and the value at x2    */
        dx02 = dx01 + dx12;
        df1 = ( f2*dx01*dx01 + f1*(dx12-dx01)*dx02 - f0*dx12*dx12 )
          / (dx01*dx02*dx12);
      }                              /* df1 is slope of parabola at x1 */
      else df1 = FL(0.0);
      if ((npts = (int)(dx01 - curx)) > fplim - fp)
        npts = fplim - fp;
      if (npts > 0) {                     /* for non-trivial segment: */
        slope = (f1 - f0) / dx01;       /*   get slope x0 to x1     */
        resd0 = df0 - slope;            /*   then residual slope    */
        resd1 = df1 - slope;            /*     at x0 and x1         */
        c3 = (resd0 + resd1) / (dx01*dx01);
        c2 = - (resd1 + FL(2.0)*resd0) / dx01;
        c1 = df0;                       /*   and calc cubic coefs   */
        c0 = f0;
        for (x = curx; npts>0; --npts, x += FL(1.0)) {
          R = c3;
          R *= x;
          R += c2;         /* f(x) = ((c3 x + c2) x + c1) x + c0  */
          R *= x;
          R += c1;
          R *= x;
          R += c0;
          *fp++ = R;                  /* store n pts for this seg */
        }
        curx = x;
      }
      curx -= dx01;               /* back up x by length last segment */
      dx01 = dx12;                /* relocate to the next segment */
      f0 = f1;                    /*   by assuming its parameters */
      f1 = f2;
      df0 = df1;
    }
    while (--nsegs && fp<fplim);    /* loop for remaining segments  */
    while (fp <= fplim)
      *fp++ = f0;                 /* & repeat the last value      */
}

static void gen09(void)
{
    int     hcnt;
    MYFLT   *valp, *fp, *finp;
    double  phs, inc, amp;

    if ((hcnt = nargs / 3) <= 0)            /* hcnt = nargs / 3 */
      return;
    valp = &e->p[5];
    finp = &ftp->ftable[flen];
    do {
      for (inc=(*valp++)*tpdlen, amp=(*valp++),
             phs=(*valp++)*tpd360, fp=ftp->ftable; fp<=finp; fp++) {
        *fp += (MYFLT)(sin(phs) * amp);
        if ((phs += inc) >= TWOPI)
          phs -= TWOPI;
      }
    } while (--hcnt);
}

static void gen10(void)
{
    long    phs, hcnt;
    MYFLT   amp, *fp, *finp;

    if ((hcnt = nargs) <= 0)                        /* hcnt is nargs   */
      return;
    finp = &ftp->ftable[flen];
    do {
      if ((amp = e->p[hcnt+4]) != 0)               /* for non-0 amps,  */
      for (phs=0, fp=ftp->ftable; fp<=finp; fp++) {
        *fp += (MYFLT)sin(phs*tpdlen) * amp;    /* accum sin pts  */
        phs += hcnt;                    /* phsinc is hno   */
        phs %= flen;                    /* phs &= lenmask; */
      }
    }
    while (--hcnt);
}

static void gen11(void)
{
    MYFLT  *fp, *finp;
    long   phs;
    double  x;
    MYFLT   denom, r, scale;
    int     n, k;

    if (nargs < 1)
      FTERR (Str(X_939,"insufficient arguments"));
    if ((n = (int)e->p[5]) < 1)
      FTERR (Str(X_1014,"nh partials < 1"));
    k = 1;
    r = FL(1.0);
    if (nargs > 1)
      k = (int)e->p[6];
    if (nargs > 2)
      r = e->p[7];
    fp = ftp->ftable;
    finp = fp + flen;
    if (nargs == 1 || (k == 1 && r == FL(1.0))) {     /* simple "buzz" case */
      int tnp1;
      MYFLT pdlen;

      tnp1 = (n << 1) + 1;
      scale = FL(0.5) / n;
      pdlen = (MYFLT)tpdlen * FL(0.5);
      for (phs = 0; fp <= finp; phs++) {
        x = phs * pdlen;
        if (!(denom = (MYFLT)sin(x)))
          *fp++ = FL(1.0);
        else *fp++ = ((MYFLT)sin(tnp1 * x) / denom - FL(1.0)) * scale;
      }
    }
    else {                                   /* complex "gbuzz" case */
      MYFLT numer, twor, rsqp1, rtn, rtnp1, absr;
      int   km1, kpn, kpnm1;

      km1   = k - 1;
      kpn   = k + n;
      kpnm1 = kpn - 1;
      twor  = r * FL(2.0);
      rsqp1 = r * r + FL(1.0);
      rtn   = intpow(r, (long) n);
      rtnp1 = rtn * r;
      if ((absr = (MYFLT)fabs(r)) > FL(0.999) && absr < FL(1.001))
        scale = FL(1.0) / n;
      else scale = (FL(1.0) - absr) / (FL(1.0) - (MYFLT)fabs(rtn));
      for (phs=0; fp <= finp; phs++) {
        x = phs * tpdlen;
        numer = (MYFLT)cos(x*k) - r * (MYFLT)cos(x*km1) - rtn * (MYFLT)cos(x*kpn)
          + rtnp1 * (MYFLT)cos(x*kpnm1);
        if ((denom = rsqp1 - twor*(MYFLT)cos(x)) > FL(0.0001)
            || denom < -FL(0.0001))
          *fp++ = numer / denom * scale;
        else *fp++ = FL(1.0);
      }
    }
}

static void gen12(void)
{
    static double coefs[] = { 3.5156229, 3.0899424, 1.2067492,
                              0.2659732, 0.0360768, 0.0045813 };
    double *coefp, sum, tsquare, evenpowr, *cplim = coefs + 6;
    int    n;
    MYFLT   *fp;
    double xscale;

    if (nargs < 1)
      FTERR (Str(X_939,"insufficient arguments"));
    xscale = (double) e->p[5] / flen / 3.75;
    for (n=0,fp=ftp->ftable; n<=flen; n++) {
      tsquare = (double) n * xscale;
      tsquare *= tsquare;
      for (sum=evenpowr=1.0, coefp=coefs; coefp<cplim; coefp++) {
        evenpowr *= tsquare;
        sum += *coefp * evenpowr;
      }
      *fp++ = (MYFLT) log(sum);
    }
}

static  MYFLT   mxval, mxscal;
static  void   gn1314(void), gen03(void);

static void gen13(void)
{
    mxval = FL(2.0);
    mxscal = FL(0.5);
    gn1314();
}

static void gen14(void)
{
    mxval = FL(1.0);
    mxscal = FL(1.0);
    gn1314();
}

static void gn1314(void)
{
    long        nh, nn;
    MYFLT       *mp, *mspace, *hp, *oddhp;
    MYFLT       xamp, xintvl, scalfac, sum, prvm;

    if ((nh = nargs - 2) <= 0)
      FTERR(Str(X_939,"insufficient arguments"))
    if ((xintvl = e->p[5]) <= 0)
      FTERR(Str(X_894,"illegal xint value"))
    if ((xamp = e->p[6]) <= 0)
      FTERR(Str(X_893,"illegal xamp value"))
    e->p[5] = -xintvl;
    e->p[6] = xintvl;
    nn = nh * sizeof(MYFLT) / 2;                /* alloc spc for terms 3,5,7,..*/
    mp = mspace = (MYFLT *)mcalloc(nn);         /* of 1st row of matrix, and */
    for (nn = (nh + 1) >>1; --nn; )             /* form array of non-0 terms */
      *mp++ = mxval = -mxval;                   /*  -val, val, -val, val ... */
    scalfac = 2 / xamp;
    hp = &e->p[7];                              /* beginning with given h0,  */
    do {
      mp = mspace;
      oddhp = hp;
      sum = *oddhp++;                           /* sum = diag(=1) * this h   */
      for (nn = (nh+1) >>1; --nn; ) {
        oddhp++;                                /*  + odd terms * h+2,h+4,.. */
        sum += *mp++ * *oddhp++;
      }
      *hp++ = sum * mxscal;                     /* repl this h w. coef (sum) */
      mp = mspace;
      prvm = FL(1.0);
      for (nn = nh>>1; --nn > 0; mp++)          /* calc nxt row matrix terms */
        *mp = prvm = *mp - prvm;
      mxscal *= scalfac;
    } while (--nh);                             /* loop til all h's replaced */
    mfree((char *)mspace);
    gen03();                                    /* then call gen03 to write */
}

static void gen15(void)
{
    MYFLT       xint, xamp, hsin[PMAX/2], h, angle;
    MYFLT       *fp, *cosp, *sinp;
    int n, nh;
    long        *lp, *lp13;

    if (nargs & 01)
      FTERR(Str(X_1320,"uneven number of args"));
    nh = (nargs - 2) >>1;
    fp = &e->p[5];                                      /* save p5, p6  */
    xint = *fp++;
    xamp = *fp++;
    for (n = nh, cosp = fp, sinp = hsin; n > 0; n--) {
      h = *fp++;                                        /* rpl h,angle pairs */
      angle = (MYFLT)(*fp++ * tpd360);
      *cosp++ = h * (MYFLT)cos((double)angle);          /*  with h cos angle */
      *sinp++ = h * (MYFLT)sin((double)angle);          /* and save the sine */
    }
    nargs -= nh;
    gen13();                                      /* call gen13   */
    if (fterrcnt) return;
    ftresdisp();                                  /* and display fno   */
    lp13 = (long *)ftp;
    fno++;                                        /* alloc eq. space for fno+1 */
    ftalloc();
    for (lp = (long *)ftp; lp < (long *)ftp->ftable; )  /* & copy header */
      *lp++ = *lp13++;
    fp = &e->p[5];
    *fp++ = xint;                                 /* restore p5, p6,   */
    *fp++ = xamp;
    for (n = nh-1, sinp = hsin+1; n > 0; n--)     /* then skip h0*sin  */
      *fp++ = *sinp++;                            /* & copy rem hn*sin */
    nargs--;
    gen14();                                      /* now draw ftable   */
}

static void gen16(void)
{
    MYFLT *fp, *valp, val;
    int nseg = nargs/3;

    fp = ftp->ftable;
    valp = &e->p[5];
    *fp++ = val = *valp++;
    while (nseg-- > 0) {
      MYFLT dur = *valp++;
      MYFLT alpha = *valp++;
      MYFLT nxtval = *valp++;
      long cnt = (long)(dur + FL(0.5));
      if (alpha == FL(0.0)) {
        MYFLT c1 = (nxtval-val)/dur;
        while (cnt-->0) {
          *fp++ = val = val + c1;
        }
      }
      else {
        MYFLT c1 = (nxtval - val)/(FL(1.0) - (MYFLT)exp((double)alpha));
        MYFLT x;
        alpha /= dur;
        x = alpha;
        while (cnt-->0) {
          *fp++ = val + c1 * (FL(1.0) - (MYFLT)exp((double)(x)));
          x += alpha;
        }
        val = *(fp-1);
      }
    }
}

static void gen17(void)
{
    int     nsegs, ndx, nxtndx;
    MYFLT   *valp, *fp, *finp;
    MYFLT   val;

    if ((nsegs = nargs >> 1) <= 0)       /* nsegs = nargs /2 */
      goto gn17err;
    valp = &e->p[5];
    fp = ftp->ftable;
    finp = fp + flen;
    if ((ndx = (int)*valp++) != 0)
      goto gn17err;
    while (--nsegs) {
      val = *valp++;
      if ((nxtndx = (int)*valp++) <= ndx)
        goto gn17err;
      do {
        *fp++ = val;
        if (fp > finp) return;
      } while (++ndx < nxtndx);
    }
    val = *valp;
    while (fp <= finp)                      /* include 2**n + 1 guardpt */
      *fp++ = val;
    return;

 gn17err:
    fterror(Str(X_794,"gen call has illegal x-ordinate values:"));
}

static void gen18(void)  /* by pete moss (petemoss@petemoss.org), jan 2002 */
{
    int cnt, start, finish, fnlen, j;
    MYFLT *pp = &e->p[5], fn, amp, *fp, *fp18 = ftp->ftable, range, f;
    double i;
    FUNC *fnp;

    if ((cnt = nargs >> 2) <= 0) {
      fterror(Str(X_652,"wrong number of args"));
      return;
    }
    while (cnt--) {
      fn=*pp++, amp=*pp++, start=(int)*pp++, finish=(int)*pp++;

      if ((start>flen) || (finish>flen)) { /* make sure start and finish < flen */
        fterror(Str(X_1683,"a range given exceeds table length"));
        return;
      }

      if ((fnp = ftfind(&fn)) != NULL) {               /* make sure fn exists */
        fp = fnp->ftable, fnlen = fnp->flen-1;  /* and set it up */
      }
      else {
        fterror(Str(X_1684,"an input function does not exist"));
        return;
      }

      range = (MYFLT)(finish - start), j = start;
      while (j++ <= finish) {                           /* write the table */
        f = (MYFLT)modf((fnlen*(j-start)/range), &i);
        *(fp18 + j) += amp * ((f * (*(fp + (int)(i+1)) -
                                    *(fp + (int)i))) +
                              *(fp + (int)i));
      }
    }
}

static void gen19(void)
{
    int     hcnt;
    MYFLT   *valp, *fp, *finp;
    double  phs, inc, amp, dc;

    if ((hcnt = nargs / 4) <= 0)            /* hcnt = nargs / 4 */
      return;
    valp = &e->p[5];
    finp = &ftp->ftable[flen];
    do {
      for (inc=(*valp++)*tpdlen, amp=(*valp++),
             phs=(*valp++)*tpd360, dc=(*valp++),
             fp=ftp->ftable; fp<=finp; fp++) {
        *fp += (MYFLT)(sin(phs) * amp + dc);   /* dc after str scale */
        if ((phs += inc) >= TWOPI)
          phs -= TWOPI;
      }
    } while (--hcnt);
}

/*  GEN20 and GEN21 by Paris Smaragdis 1994 B.C.M. Csound development team  */
static void gen20(void)
{
    MYFLT cf[4], *ft;
    double arg, i, xarg, beta = 0.0;

    ft = ftp->ftable;
    xarg = 1.0;

    if (e->p[4] < FL(0.0)) {
      xarg = e->p[6];
      if ( nargs < 2 ) xarg = 1.0;
    }

    if (nargs > 2)
      beta = (double)e->p[7];

    switch( (int)e->p[5])  {
    case 1:                     /* Hamming */
        cf[0] = FL(0.54);
        cf[1] = FL(0.46);
        cf[2] = cf[3] = FL(0.0);
        break;
    case 2:                     /* Hanning */
        cf[0] = cf[1] = FL(0.5);
        cf[2] = cf[3] = FL(0.0);
        break;
    case 3:                     /* Bartlett */
        arg = 2.0/flen;
        for (i = 0.0 ; i < flen/2.0 ; i++)
            *ft++ = (MYFLT)(i*arg*xarg);
        for (; i < flen ; i++)
            *ft++ = (MYFLT)((2.0 - i*arg)*xarg);
        return;
    case 4:                     /* Blackman */
        cf[0] = FL(0.42);
        cf[1] = FL(0.5);
        cf[2] = FL(0.08);
        cf[3] = FL(0.0);
        break;
    case 5:                     /* Blackman-Harris */
        cf[0] = FL(0.35878);
        cf[1] = FL(0.48829);
        cf[2] = FL(0.14128);
        cf[3] = FL(0.01168);
        break;
    case 6:                     /* Gaussian */
        arg = 12.0/flen;
        for (i = -6.0 ; i < 0.0 ; i += arg)
          *ft++ = (MYFLT)(xarg * (pow( 2.71828, -(i*i)/2.0)));
        for (i = arg ; i < 6.0 ; i += arg)
          *ft++ = (MYFLT)(xarg * (pow( 2.71828, -(i*i)/2.0)));
        return;
    case 7:                     /* Kaiser */
      {
        double flen2 = (double)flen/2.0;
        double flenm12 = (double)(flen-1)*(flen-1);
        double besbeta = besseli( beta);
        for (i = -flen2 + 0.1 ; i < flen2 ; i++)
          *ft++ = (MYFLT)(xarg * besseli((beta*sqrt(1.0-i*i/flenm12)))/besbeta);
        return;
      }
    case 8:                     /* Rectangular */
        for (i = 0 ; i < flen ; i++)
          *ft++ = FL(1.0);
        return;
    case 9:                     /* Sinc */
        arg = TWOPI / flen;
        for (i = -PI ; i < 0 ; i += arg)
          *ft++ = (MYFLT)(xarg * sin(i) / i);
        *ft++ = FL(1.0);
        for (i = arg ; i < PI ; i += arg)
          *ft++ = (MYFLT)(xarg * sin(i) / i);
        return;
    default:
        fterror(Str(X_372,"No such window!"));
        return;
    }

    arg = TWOPI/flen;

    for (i = 0.0 ; i < TWOPI ; i += arg)
      *ft++ = (MYFLT)(xarg * (cf[0] - cf[1]*cos(i) +
                              cf[2]*cos(2.0*i) - cf[3]*cos(3.0*i)));
}

static void gen21(void)
{
    long i;
    MYFLT *ft;
    MYFLT scale;

    ft = ftp->ftable;

    if (nargs < 1)  {           /* All need at least 1 argument */
      fterror(Str(X_541,"Wrong number of input arguments\n"));
      return;
    }
    if (nargs ==1) scale = FL(1.0);
    else scale = e->p[6];

    switch ((int)e->p[5])  {
    case 1:                     /* Uniform distribution */
        for (i = 0 ; i < flen ; i++)
            *ft++ = unifrand(scale);
        break;
    case 2:                     /* Linear distribution */
        for (i = 0 ; i < flen ; i++)
            *ft++ = linrand(  scale);
        break;
    case 3:                     /* Triangular about 0.5 */
        for (i = 0 ; i < flen ; i++)
            *ft++ = trirand(  scale);
        break;
    case 4:                     /* Exponential */
        for (i = 0 ; i < flen ; i++)
            *ft++ = exprand(  scale);
        break;
    case 5:                     /* Bilateral exponential */
        for (i = 0 ; i < flen ; i++)
            *ft++ = biexprand(  scale);
        break;
    case 6:                     /* Gaussian distribution */
        for (i = 0 ; i < flen ; i++)
            *ft++ = gaussrand(  scale);
        break;
    case 7:                     /* Cauchy distribution */
        for (i = 0 ; i < flen ; i++)
            *ft++ = cauchrand(  scale);
        break;
    case 8:                     /* Positive Cauchy */
        for (i = 0 ; i < flen ; i++)
            *ft++ = pcauchrand(  scale);
        break;
    case 9:                     /* Beta distribution */
        if (nargs < 3)  {
            fterror(Str(X_541,"Wrong number of input arguments\n"));
            return;
        }
        for (i = 0 ; i < flen ; i++)
            *ft++ = betarand(scale,(MYFLT)e->p[7],(MYFLT)e->p[8]);
        break;
    case 10:                    /* Weibull Distribution */
        if (nargs < 2)  {
            fterror(Str(X_541,"Wrong number of input arguments\n"));
            return;
        }
        for (i = 0 ; i < flen ; i++)
            *ft++ = weibrand(  scale, (MYFLT) e->p[7]);
        break;
    case 11:                    /* Poisson Distribution */
        for (i = 0 ; i < flen ; i++)
            *ft++ = poissrand(  scale);
        break;
    default:
        fterror(Str(X_508,"unknown distribution\n"));
    }
}

static void gen23(void)         /* ASCII file table read Gab 17-feb-98*/
                                /* Modified after Paris Smaragdis by JPff */
{
    int         c = 0, j = 0;
    char        buf[512], *p;
    MYFLT       *fp;
    FILE        *infile;

    if (!(infile=fopenin(e->strarg)))
      FTERR (Str(X_725,"error opening ASCII file"));
    p = buf;
    if (ftp==NULL) {
        /* Start counting elements */
      flen = 0;
      while ((c= getc(infile)) != EOF) {
        if (!isspace(c)) {
          if (c == ';') {
            while ((c= getc(infile)) != '\n') ;
          }
          else *p++ = c;
        }
        else {
          char pp;
          *p = '\0';
          for (p = buf; (pp = *p) != '\0'; p++) {
            if (!isdigit(pp) && pp != '-' && pp != '.' && pp != '\0')
              goto nxt;
          }
          flen++;
        nxt:
          while (isspace(c=getc(infile))) ;
          ungetc(c,infile);
          p = buf;
        }
      }
      printf("%d elements in %s\n", flen, e->strarg);
      rewind(infile);
      /* Allocate memory and read them in now */
      flen    = flen+2;
      lenmask = flen;
      flenp1  = flen+2;
      ftp = (FUNC *) mcalloc((long)sizeof(FUNC) + flen*sizeof(MYFLT));
      flist[fno]   = ftp;
      ftp->flen    = flen;
      ftp->lenmask = flen;
    }
    fp = ftp->ftable;
    p = buf;
    while ((c= getc(infile)) != EOF && j < flen) {
      if (!isspace(c)) {
        if (c == ';') {
          while ((c= getc(infile)) != '\n') ;
        }
        else *p++ = c;
      }
      else {
        char pp;                /* To save value */
        *p = '\0';
        for (p = buf; (pp = *p) != '\0'; p++) {
          if (!isdigit(pp) && pp != '-' && pp != '.' && pp != '\0')
            goto next ;
        }
        *fp++ = (MYFLT) atof (buf);
        j++;
      next:
        while (isspace(c=getc(infile))) ;
        ungetc(c,infile);
        p = buf;
      }
    }
    fclose(infile);
}


static void gen24(void)
{
    MYFLT       *fp = ftp->ftable, *fp_source;
    FUNC        *srcftp;
    int srcno, srcpts, j;
    MYFLT max, min, new_max, new_min, source_amp, target_amp, amp_ratio;

    if (nargs < 3)
      FTERR(Str(X_939, "insufficient arguments"))
    if ((srcno = (int)e->p[5]) <= 0 ||
        srcno > MAXFNUM             ||
        (srcftp = flist[srcno]) == NULL)
      FTERR(Str(X_1344,"unknown srctable number"))
    fp_source = srcftp->ftable;

    new_min = e->p[6];
    new_max = e->p[7];
    srcpts = srcftp->flen;
    if (srcpts!= flen)
      FTERR(Str(X_938,"table size must be the same of source table"))
    max = min = fp_source[0];
    for (j = 0; j < srcpts; j++) {
      if (fp_source[j] > max ) max = fp_source[j];
      if (fp_source[j] < min ) min = fp_source[j];
    }

    source_amp = max - min;
    target_amp = new_max - new_min;
    amp_ratio = target_amp/source_amp;

    for (j = 0; j < srcpts; j++) {
      fp[j] = (fp_source[j]-min) * amp_ratio + new_min;
    }
    fp[j] = fp[j-1];
}


static void gen25(void)
{
    int nsegs,  seglen;
    MYFLT       *valp, *fp, *finp;
    MYFLT       x1, x2, y1, y2, mult;

    if ((nsegs = ((nargs / 2) - 1) )<= 0) return;
    valp = &e->p[5];
    fp = ftp->ftable;
    finp = fp + flen;
    do {
      x1 = *valp++;
      y1 =  *valp++;
      x2 = *valp++;
      if (nsegs > 1)
        y2 =  *valp++;
      else
        y2 = *valp;
      if (x2 < x1) goto gn25err;
      if (x1 > flen || x2 > flen) goto gn25err2;
      seglen = (int)(x2-x1);
      if (y1 <= 0 || y2 <= 0) goto gn25err3;
      mult = y2/y1;
      mult = (MYFLT)pow( (double)mult, 1.0/(double)seglen);
      while (seglen--) {
        *fp++ = y1;
        y1 *= mult;
        if (fp > finp) return;
      }
      valp -= 2;
    } while (--nsegs);
    if (fp == finp)                     /* if 2**n pnts, add guardpt */
      *fp = y1;
    return;

 gn25err:
    fterror(Str(X_1385,"x coordindates must all be in increasing order:"));
    return;

 gn25err2:
    fterror(Str(X_1384,"x coordindate greater than function size:"));
    return;

 gn25err3:
    fterror(Str(X_858,"illegal input val (y <= 0) for gen call, beginning:"));
    return;
}

static void gen27(void)
{
    int nsegs;
    MYFLT       *valp, *fp, *finp;
    MYFLT       x1, x2, y1, y2, seglen, incr;

    if ((nsegs = ((nargs / 2) - 1) )<= 0) return;
    valp = &e->p[5];
    fp = ftp->ftable;
    finp = fp + flen;
    do {
      x1 = *valp++;
      y1 = *valp++;
      x2 = *valp++;
      if (nsegs > 1)
        y2 =  *valp++;
      else
        y2 = *valp;
      if (x2 < x1) goto gn27err;
      if (x1 > flen || x2 > flen) goto gn27err2;
      seglen = x2-x1;
      incr = (y2 - y1) / seglen;
      while (seglen--) {
        *fp++ = y1;
        y1 += incr;
        if (fp > finp) return;
      }
      valp -= 2;
    } while (--nsegs);
    if (fp == finp)                     /* if 2**n pnts, add guardpt */
      *fp = y1;
    return;

 gn27err:
    fterror(Str(X_1385,"x coordindates must all be in increasing order:"));
    return;
 gn27err2:
    fterror(Str(X_1384,"x coordindate greater than function size:"));
    return;
}

static void gen28(void) /* read X Y values directly from ascii file */
{
    MYFLT       *fp = ftp->ftable, *finp;
    int         seglen, resolution = 100;
    FILE        *filp;
    char        filename[256];
    int         i=0, j=0;
    MYFLT       *x, *y, *z;
    int         arraysize = 1000;
    MYFLT       x1, y1, z1, x2, y2, z2, incrx, incry;

    finp = fp + flen;
    strcpy(filename,e->strarg);
    if ((filp = fopenin(filename)) == NULL) goto gen28err1;

    x = (MYFLT*)mmalloc(arraysize*sizeof(MYFLT));
    y = (MYFLT*)mmalloc(arraysize*sizeof(MYFLT));
    z = (MYFLT*)mmalloc(arraysize*sizeof(MYFLT));
    while (fscanf( filp, "%f%f%f", &z[i], &x[i], &y[i])!= EOF) {
      i++;
      if (i>=arraysize) {
        arraysize += 1000;
        x = (MYFLT*)mrealloc(x, arraysize*sizeof(MYFLT));
        y = (MYFLT*)mrealloc(y, arraysize*sizeof(MYFLT));
        z = (MYFLT*)mrealloc(z, arraysize*sizeof(MYFLT));
      }
    }
    --i;

    flen = (long)(z[i]*resolution*2);
    flen = flen+2;
    lenmask=flen;
    flenp1=flen+2;
    ftp=NULL;
    mfree((char *)ftp);         /*   release old space   */
    ftp = (FUNC *) mcalloc((long)sizeof(FUNC) + flen*sizeof(MYFLT));
    flist[fno] = ftp;
    ftp->flen = flen;
    ftp->lenmask=flen;
    fp = ftp->ftable;
    finp = fp + flen;

    do {
      x1 = x[j];
      y1 = y[j];
      x2 = x[j+1];
      y2 = y[j+1];
      z1 = z[j];
      z2 = z[j+1];

      if (z2 < z1) goto gen28err2;
      seglen = (int)((z2-z1) * resolution); /* printf("seglen= %f\n", seglen); */
      incrx = (x2 - x1) / (MYFLT)seglen;
      incry = (y2 - y1) / (MYFLT)seglen;
      while (seglen--) {
        *fp++ = x1; /* printf("x= %f  ", x1); fflush(stdout); */
        x1 += incrx;
        *fp++ = y1; /* printf("y= %f\n", y1); fflush(stdout); */
        y1 += incry;
      }

      j++;
    } while (--i);
    do {
      *fp++ = x[j];
      *fp++ = y[j+1];
    } while (fp < finp);

    mfree(x); mfree(y); mfree(z);
    fclose(filp);

    return;

 gen28err1:
    fterror(Str(X_672,"could not open space file"));
    return;
 gen28err2:
    fterror(Str(X_492,"Time values must be in increasing order"));
    return;
}

/* gen30: extract harmonics from source table */

static void gen30 (void)
{
    complex *ex, *x;
    long    l1, l2, minh = 0, maxh = 0, i;
    MYFLT   xsr, minfrac, maxfrac;
    FUNC    *f;

    if (nargs < 3) {
      fterror(Str(X_941, "insufficient gen arguments"));
      return;
    }
    xsr = FL(1.0);
    if ((nargs > 3) && (e->p[8] > FL(0.0))) xsr = esr_ / e->p[8];
    f = ftfind (&(e->p[5])); if (f == NULL) return;
    l1 = ftp->flen; l2 = f->flen;
    minfrac = e->p[6];          /* lowest harmonic number */
    maxfrac = e->p[7] * xsr;    /* highest harmonic number */
    i = (l1 < l2 ? l1 : l2) >> 1;       /* sr/2 limit */
    /* limit to 0 - sr/2 range */
    if ((maxfrac < FL(0.0)) || (minfrac > (MYFLT) i)) return;
    if (maxfrac > (MYFLT) i) maxfrac = (MYFLT) i;
    if (minfrac < FL(0.0)) minfrac = FL(0.0);
    if ((nargs > 4) && (e->p[9] != FL(0.0))) {
      minh = (long) minfrac;            /* "interpolation" mode */
      minfrac -= (MYFLT) minh;
      minfrac = FL(1.0) - minfrac;
      maxh = (long) maxfrac;
      maxfrac -= (MYFLT) maxh++;
      if (maxh > i) {
        maxh = i; maxfrac = FL(1.0);
      }
    } else {
      minh = (long) ((double) minfrac + (i < 10000L ? 0.99 : 0.9));
      maxh = (long) ((double) maxfrac + (i < 10000L ? 0.01 : 0.1));
      minfrac = maxfrac = FL(1.0);
    }
    if (minh > maxh) return;
    i = ((l1 > l2 ? l1 : l2) >> 1) + 1L;
    x = (complex*) mmalloc (sizeof (complex) * i);
    /* read src table */
    for (i = 0; i < l2; i++) {
      x[i >> 1].re = f->ftable[i]; i++;
      x[i >> 1].im = f->ftable[i];
    }
    /* filter */
    ex = AssignBasis (NULL, l2);
    FFT2realpacked (x, l2, ex);
    i = -1; while (++i < minh) x[i].re = x[i].im = FL(0.0);
    i = maxh; while (++i <= (l1 >> 1)) x[i].re = x[i].im = FL(0.0);
    x[minh].re *= minfrac; x[minh].im *= minfrac;
    x[maxh].re *= maxfrac; x[maxh].im *= maxfrac;
    ex = AssignBasis (NULL, l1);
    FFT2torlpacked (x, l1, FL(1.0) / (float) l2, ex);
    /* write dest. table */
    for (i = 0; i < l1; i++) {
      ftp->ftable[i] = x[i >> 1].re; i++;
      ftp->ftable[i] = x[i >> 1].im;
    }
    ftp->ftable[l1] = ftp->ftable[0];   /* write guard point */
    mfree (x);
}

/* gen31: transpose, phase shift, and mix source table */

static void gen31 (void)
{
    complex *ex, *x, *y;
    MYFLT   a, p;
    double  d_re, d_im, p_re, p_im, ptmp;
    long    i, j, k, n, l1, l2;
    FUNC    *f;

    if (nargs < 4) {
      fterror(Str(X_941, "insufficient gen arguments"));
      return;
    }
    f = ftfind (&(e->p[5])); if (f == NULL) return;
    l1 = ftp->flen; l2 = f->flen;

    x = (complex*) mcalloc (sizeof (complex) * ((l2 >> 1) + 1));
    y = (complex*) mcalloc (sizeof (complex) * ((l1 >> 1) + 1));
    /* read and analyze src table */
    for (i = 0; i < l2; i++) {
      x[i >> 1].re = f->ftable[i]; i++;
      x[i >> 1].im = f->ftable[i];
    }
    ex = AssignBasis (NULL, l2);
    FFT2realpacked (x, l2, ex);

    j = 6; while (j < (nargs + 3)) {
      n = (long) (FL(0.5) + e->p[j++]); if (n < 1) n = 1;       /* frequency */
      a = e->p[j++];                                    /* amplitude */
      p = e->p[j++];                                    /* phase */
      p -= (MYFLT) ((long) p); if (p < FL(0.0)) p += FL(1.0); p *= TWOPI_F;
      d_re = cos ((double) p); d_im = sin ((double) p);
      p_re = 1.0; p_im = 0.0;   /* init. phase */
      i = k = 0; do {
        /* mix to table */
        y[i].re += a * (x[k].re * (MYFLT) p_re - x[k].im * (MYFLT) p_im);
        y[i].im += a * (x[k].im * (MYFLT) p_re + x[k].re * (MYFLT) p_im);
        /* update phase */
        ptmp = p_re * d_re - p_im * d_im;
        p_im = p_im * d_re + p_re * d_im;
        p_re = ptmp;
        i += n; k++;
      } while ((i <= (l1 >> 1)) && (k <= (l2 >> 1)));
    }

    /* write dest. table */
    ex = AssignBasis (NULL, l1);
    FFT2torlpacked (y, l1, FL(1.0) / (MYFLT) l2, ex);
    for (i = 0; i < l1; i++) {
      ftp->ftable[i] = y[i >> 1].re; i++;
      ftp->ftable[i] = y[i >> 1].im;
    }
    ftp->ftable[l1] = ftp->ftable[0];   /* write guard point */

    mfree (x); mfree (y);
}


/* gen32: transpose, phase shift, and mix source tables */

static void gen32 (void)
{
    complex *ex, *x, *y;
    MYFLT   a, p;
    double  d_re, d_im, p_re, p_im, ptmp;
    long    i, j, k, n, l1, l2, ntabl, *pnum, ft;
    FUNC    *f;

    if (nargs < 4) {
      fterror(Str(X_941, "insufficient gen arguments"));
      return;
    }

    ntabl = nargs >> 2;         /* number of waves to mix */
    pnum  = (long*) mmalloc (sizeof (long) * ntabl);
    for (i = 0; i < ntabl; i++)
      pnum[i] = (i << 2) + 5;   /* p-field numbers */
    do {
      i = k = 0;                        /* sort by table number */
      while (i < (ntabl - 1)) {
        if (e->p[pnum[i]] > e->p[pnum[i + 1]]) {
          j = pnum[i]; pnum[i] = pnum[i + 1]; pnum[i + 1] = j;
          k = 1;
        }
        i++;
      }
    } while (k);

    l1 = ftp->flen;             /* dest. table length */
    for (i = 0; i <= l1; i++) ftp->ftable[i] = FL(0.0);
    x = y = NULL;

    ft = 0x7fffffffL;           /* last table number */
    j  = -1L;                   /* current wave number */

    while (++j < ntabl) {
      p = e->p[pnum[j]];                /* table number */
      i = (long) (p + (p < FL(0.0) ? FL(-0.5) : FL(0.5)));
      p = (MYFLT) (abs (i));
      if ((f = ftfind (&p)) == NULL) return;    /* source table */
      l2 = f->flen;             /* src table length */
      if (i < 0) {              /* use linear interpolation */
        ft = i;
        p_re  = (double) e->p[pnum[j] + 3];     /* start phase */
        p_re -= (double) ((long) p_re); if (p_re < 0.0) p_re++;
        p_re *= (double) l2;
        d_re  = (double) e->p[pnum[j] + 1];     /* frequency */
        d_re *= (double) l2 / (double) l1;
        a     = e->p[pnum[j] + 2];                      /* amplitude */
        for (i = 0; i <= l1; i++) {
          k = (long) p_re; p = (MYFLT) (p_re - (double) k);
          if (k >= l2) k -= l2;
          ftp->ftable[i] += f->ftable[k++] * a * (FL(1.0) - p);
          ftp->ftable[i] += f->ftable[k] * a * p;
          p_re += d_re;
          while (p_re < 0.0) p_re += (double) l2;
          while (p_re >= (double) l2) p_re -= (double) l2;
        }
      }
      else {                    /* use FFT */
        if (i != ft) {
          ft = i;               /* new table */
          if (y == NULL)
            y = (complex*) mcalloc (sizeof (complex) * ((l1 >> 1) + 1));
          if (x != NULL) mfree (x);
          x = (complex*) mcalloc (sizeof (complex) * ((l2 >> 1) + 1));
          /* read and analyze src table */
          for (i = 0; i < l2; i++) {
            x[i >> 1].re = f->ftable[i]; i++;
            x[i >> 1].im = f->ftable[i];
          }
          ex = AssignBasis (NULL, l2);
          FFT2realpacked (x, l2, ex);
        }
        n = (long) (FL(0.5) + e->p[pnum[j] + 1]);               /* frequency */
        if (n < 1) n = 1;
        a = e->p[pnum[j] + 2] / (MYFLT) l2;             /* amplitude */
        p = e->p[pnum[j] + 3];                          /* phase */
        p -= (MYFLT) ((long) p); if (p < FL(0.0)) p += FL(1.0); p *= TWOPI_F;
        d_re = cos ((double) p); d_im = sin ((double) p);
        p_re = 1.0; p_im = 0.0;         /* init. phase */
        i = k = 0; do {
          /* mix to table */
          y[i].re += a * (x[k].re * (MYFLT) p_re - x[k].im * (MYFLT) p_im);
          y[i].im += a * (x[k].im * (MYFLT) p_re + x[k].re * (MYFLT) p_im);
          /* update phase */
          ptmp = p_re * d_re - p_im * d_im;
          p_im = p_im * d_re + p_re * d_im;
          p_re = ptmp;
          i += n; k++;
        } while ((i <= (l1 >> 1)) && (k <= (l2 >> 1)));
      }
    }
    /* write dest. table */
    if (y != NULL) {
      ex = AssignBasis (NULL, l1);
      FFT2torlpacked (y, l1, FL(1.0), ex);
      for (i = 0; i < l1; i++) {
        ftp->ftable[i] += y[i >> 1].re; i++;
        ftp->ftable[i] += y[i >> 1].im;
      }
      ftp->ftable[l1] += y[0].re;               /* write guard point */
      mfree (x);                                /* free tmp memory */
      mfree (y);
    }
    mfree (pnum);
}


/* GEN33 by Istvan Varga */

static void gen33 (void)
{
    MYFLT   fmode, *ft, *srcft, scl, amp, phs;
    complex *ex, *x;
    long    nh, flen, srclen, i, pnum, maxp;
    FUNC    *src;

    if (nargs < 3) {
      fterror(Str(X_941, "insufficient gen arguments"));
      return;
    }
    if (nargs > 3)      /* check optional argument */
      fmode = e->p[8];
    else
      fmode = FL(0.0);
    /* table length and data */
    ft = ftp->ftable; flen = (long) ftp->flen;
    /* source table */
    if ((src = ftfind (&(e->p[5]))) == NULL) return;
    srcft = src->ftable; srclen = (long) src->flen;
    /* number of partials */
    nh = (long) (e->p[6] + FL(0.5));
    if (nh > (srclen / 3L)) nh = srclen / 3L;
    if (nh < 0L) nh = 0L;
    /* amplitude scale */
    scl = e->p[7];
    /* frequency mode */
    if (fmode < FL(0.0)) {
      fmode = (MYFLT) flen / (esr_ * -fmode);    /* frequency in Hz */
    }
    else if (fmode > FL(0.0)) {
      fmode = (MYFLT) flen / fmode;             /* ref. sample rate */
    }
    else {
      fmode = FL(1.0);                          /* partial number */
    }

    /* allocate memory for tmp data */
    x = (complex*) mcalloc(sizeof (complex) * ((flen >> 1) + 1L));
    ex = AssignBasis(NULL, flen);

    maxp = flen >> 1;           /* max. partial number */
    i = nh;
    while (i--) {
      /* amplitude */
      amp = scl * *(srcft++);
      /* partial number */
      pnum = (long) (fmode * *srcft + (*srcft < FL(0.0) ? FL(-0.5) : FL(0.5)));
      srcft++;
      if (pnum < (-maxp) || pnum > maxp) {
        srcft++; continue;      /* skip partial with too high frequency */
      }
      /* initial phase */
      phs = TWOPI_F * *(srcft++);
      if (pnum < 0L) {
        phs = PI_F - phs; pnum = -pnum;         /* negative frequency */
      }
      /* mix to FFT data */
      x[pnum].re += amp * (MYFLT) sin((double) phs);
      x[pnum].im -= amp * (MYFLT) cos((double) phs);
    }

    FFT2torlpacked(x, flen, FL(0.5), ex);      /* iFFT */

    i = flen >> 1;              /* copy to output table */
    pnum = 0L;
    while (i--) {
      *(ft++) = x[pnum].re;
      *(ft++) = x[pnum++].im;
    }
    *ft = x[0].re;              /* write guard point */

    /* free tmp memory */
    mfree(x);
}

/* GEN34 by Istvan Varga */

static void gen34 (void)
{
    MYFLT   fmode, *ft, *srcft, scl;
    double  y0, y1, x, c, v, *xn, *cn, *vn, *tmp, amp, frq, phs;
    long    nh, flen, srclen, i, j, k, l, bs;
    FUNC    *src;

    if (nargs < 3) {
      fterror(Str(X_941, "insufficient gen arguments"));
      return;
    }
    if (nargs > 3)      /* check optional argument */
      fmode = e->p[8];
    else
      fmode = FL(0.0);
    /* table length and data */
    ft = ftp->ftable; flen = (long) ftp->flen;
    /* source table */
    if ((src = ftfind (&(e->p[5]))) == NULL) return;
    srcft = src->ftable; srclen = (long) src->flen;
    /* number of partials */
    nh = (long) (e->p[6] + FL(0.5));
    if (nh > (srclen / 3L)) nh = srclen / 3L;
    if (nh < 0L) nh = 0L;
    /* amplitude scale */
    scl = e->p[7];
    /* frequency mode */
    if (fmode < FL(0.0)) {
      fmode = TWOPI_F / (esr_ * -fmode); /* frequency in Hz */
    }
    else if (fmode > FL(0.0)) {
      fmode = TWOPI_F / fmode;          /* ref. sample rate */
    }
    else {
      fmode = TWOPI_F / (MYFLT) flen;   /* partial number */
    }

    /* use blocks of 256 samples (2048 bytes) for speed */
    bs = 256L;
    /* allocate memory for tmp data */
    tmp = (double*) mmalloc(sizeof (double) * bs);
    xn  = (double*) mmalloc(sizeof (double) * (nh + 1L));
    cn  = (double*) mmalloc(sizeof (double) * (nh + 1L));
    vn  = (double*) mmalloc(sizeof (double) * (nh + 1L));
    /* initialise oscillators */
    i = -1L;
    while (++i < nh) {
      amp = (double) scl * (double) *(srcft++);         /* amplitude */
      frq = (double) fmode * (double) *(srcft++);       /* frequency */
      if (fabs (frq) > PI) {
        xn[i] = cn[i] = vn[i] = 0.0;
        srcft++; continue;      /* skip partial with too high frequency */
      }
      phs = TWOPI * (double) *(srcft++);                /* phase */
      /* calculate coeffs for fast sine oscillator */
      y0 = sin(phs);           /* sample 0 */
      y1 = sin(phs + frq);     /* sample 1 */
      xn[i] = y0;
      cn[i] = 2.0 * cos(frq) - 2.0;
      vn[i] = y1 - cn[i] * y0 - y0;
      /* amp. scale */
      xn[i] *= amp; vn[i] *= amp;
    }

    /* render output */
    j = flen + 1L;      /* include guard point */
    do {
      k = (j > bs ? bs : j);    /* block size */
      /* clear buffer */
      for (i = 0L; i < k; i++) tmp[i] = 0.0;
      /* fast sine oscillator */
      i = -1L;
      while (++i < nh) {
        x = xn[i]; c = cn[i]; v = vn[i];
        l = k;
        do {
          *(tmp++) += x;
          v += c * x;
          x += v;
        } while (--l);
        tmp -= k;               /* restore pointer */
        xn[i] = x; vn[i] = v;   /* save oscillator state */
      }
      /* write to output table */
      for (i = 0L; i < k; i++) *(ft++) = (MYFLT) tmp[i];
      j -= k;
    } while (j);

    /* free tmp buffers */
    mfree(tmp); mfree(xn); mfree(cn); mfree(vn);
}



static void gen40(void)              /*gab d5*/
{
    MYFLT       *fp = ftp->ftable, *fp_source, *fp_temp;
    FUNC        *srcftp;
    int         srcno, srcpts, j,k;
    MYFLT       last_value = FL(0.0), lenratio;

    if ((srcno = (int)e->p[5]) <= 0 ||
        srcno > MAXFNUM             ||
        (srcftp = flist[srcno]) == NULL)
      FTERR(Str(X_1667,"unknown source table number"))
    fp_source = srcftp->ftable;
    srcpts = srcftp->flen;
    fp_temp = (MYFLT *) calloc(srcpts, sizeof(MYFLT));
    for (j = 0; j < srcpts; j++) {
      last_value += fp_source[j];
      fp_temp[j] = last_value;
    }
    lenratio = (flen-1)/last_value;

    for (j = 0; j < flen; j++) {
      k=0;
      while ( k++ < srcpts && fp_temp[k] * lenratio < j) ;
      k--;
      fp[j] = (MYFLT) k;
    }
    fp[j] = fp[j-1];
    free(fp_temp);
}

static void gen41(void)  /*gab d5*/
{
    MYFLT       *fp = ftp->ftable, *pp = &e->p[5];
    int         j, k, width;
    long        tot_prob=0;

    for (j=0; j < nargs; j+=2) {
      tot_prob += (long) pp[j+1];
    }
    for (j=0; j< nargs; j+=2) {
      width = (int) ((pp[j+1]/tot_prob) * flen +.5);
      for ( k=0; k < width; k++) {
        *fp++ = pp[j];
      }
    }
    *fp = pp[j-1];
}


static void gen42(void) /*gab d5*/
{
    MYFLT       *fp = ftp->ftable, *pp = &e->p[5], inc;
    int         j, k, width;
    long        tot_prob=0;

    for (j=0; j < nargs; j+=3) {
      tot_prob += (long) pp[j+2];
    }
    for (j=0; j< nargs; j+=3) {
      width = (int) ((pp[j+2]/tot_prob) * flen +FL(0.5));
      inc = (pp[j+1]-pp[j]) / (MYFLT) (width-1);
      for ( k=0; k < width; k++) {
        *fp++ = pp[j]+(inc*k);
      }
    }
    *fp = *(fp-1);
}

static void fterror(char *s)
{
    printf(Str(X_268,"FTERROR, ftable %d: %s\n"),fno,s);
    printf(Str(X_752,"f%3.0f%8.2f%8.2f%8.2f"),
           e->p[1],e->p2orig,e->p3orig,e->p[4]);
    if (e->p[5] == SSTRCOD)
      printf("  \"%s\" ...\n",e->strarg);
    else printf("%8.2f ...\n",e->p[5]);
    fterrcnt++;
}

static void ftresdisp(void)
  /* set guardpt, rescale the function, and display it */
{
    MYFLT       *fp, *finp = &ftp->ftable[flen];
    MYFLT       abs, maxval;
    static      WINDAT  dwindow;

    if (!guardreq)                              /* if no guardpt yet, do it */
      ftp->ftable[flen] = ftp->ftable[0];
    if (e->p[4] > FL(0.0)) {            /* if genum positve, rescale */
      for (fp=ftp->ftable, maxval = FL(0.0); fp<=finp; ) {
        if ((abs = *fp++) < FL(0.0))
          abs = -abs;
        if (abs > maxval)
          maxval = abs;
      }
      if (maxval != FL(0.0) && maxval != FL(1.0))
        for (fp=ftp->ftable; fp<=finp; fp++)
          *fp /= maxval;
    }
    sprintf(strmsg,Str(X_781,"ftable %d:"),fno);
    dispset(&dwindow,ftp->ftable,(long)(flen+guardreq),strmsg,0,"ftable");
    display(&dwindow);
}

static void ftalloc(void)   /* alloc ftable space for fno (or replace one)  */
{                           /*  set ftp to point to that structure      */
    if ((ftp = flist[fno]) != NULL) {
      printf(Str(X_1161,"replacing previous ftable %d\n"),fno);
      if (flen != ftp->flen) {          /* if redraw & diff len, */
        mfree((char *)ftp);             /*   release old space   */
        flist[fno] = NULL;
        if (actanchor.nxtact != NULL) {
          if (O.msglevel & WARNMSG) { /*   & chk for danger    */
            printf(Str(X_785,"WARNING: ftable %d relocating due to size change\n"
                       "currently active instruments may find this "
                       "disturbing\n"), fno);
          }
        }
      }
      else {                            /* else clear it to zero */
        MYFLT   *fp = ftp->ftable;
        MYFLT   *finp = &ftp->ftable[flen];
        while (fp <= finp)
          *fp++ = FL(0.0);
      }
    }
    if ((ftp = flist[fno]) == NULL) {   /*   alloc space as reqd */
      ftp = (FUNC *) mcalloc((long)sizeof(FUNC) + flen*sizeof(MYFLT));
      flist[fno] = ftp;
    }
}

FUNC *
ftfind(MYFLT *argp)     /* find the ptr to an existing ftable structure */
                        /*   called by oscils, etc at init time         */
{
    int fno;
    FUNC        *ftp;

    if ((fno = (int)*argp) <= 0 ||
        fno > maxfnum           ||
        (ftp = flist[fno]) == NULL) {
      sprintf(errmsg, Str(X_315,"Invalid ftable no. %f"), *argp);
      initerror(errmsg);
      return NULL;
    }
    else if (!ftp->lenmask) {
      sprintf(errmsg, Str(X_686,"deferred-size ftable %f illegal here"), *argp);
      initerror(errmsg);
      return NULL;
    }
    else return(ftp);
}

/*************************************/
/* ftfindp()
 *
 * New function to find a function table at performance time.  Based
 * on ftfind() which is intended to run at init time only.
 *
 * This function can be called from other modules - such as ugrw1.c.
 *
 * It returns a pointer to a FUNC data structure which contains all
 * the details of the desired table.  0 is returned if it cannot be
 * found.
 *
 * This does not handle deferred function table loads (gen01).
 *
 * Maybe this could be achieved, but some exploration would be
 * required to see that this is feasible at performance time.
 *  */
FUNC * ftfindp(MYFLT *argp)
{
    int fno;
    FUNC        *ftp;
    /* Check limits, and then index  directly into the flist[] which
     * contains pointers to FUNC data structures for each table.
     */
    if ((fno = (int)*argp) <= 0 ||
        fno > maxfnum           ||
        (ftp = flist[fno]) == NULL) {
      sprintf(errmsg, Str(X_315,"Invalid ftable no. %f"), *argp);
      perferror(errmsg);
      return NULL;
    }
    else if (!ftp->lenmask) {
      /* Now check that the table has a length > 0.  This should only
       * occur for tables which have not been loaded yet.  */
      sprintf(errmsg,
              Str(X_241,
                  "Deferred-size ftable %f load not available at perf time."),
              *argp);
      perferror(errmsg);
      return NULL;
    }
    else return(ftp);
}

 FUNC *
ftnp2find(MYFLT *argp)  /* find ptr to a deferred-size ftable structure */
                        /*   called by loscil at init time, and ftlen   */
{
    EVTBLK evt;
    char strarg[SSTRSIZ];

    if ((fno = (int)*argp) <= 0 ||
        fno > maxfnum           ||
        (ftp = flist[fno]) == NULL) {
      sprintf(errmsg, Str(X_315,"Invalid ftable no. %f"), *argp);
      initerror(errmsg);
      return NULL;
    }
    else {
      if (ftp->flen == 0) {
        /* The soundfile hasn't been loaded yet, so call GEN01 */
        flen = 0;
        e = &evt;
        e->p[4] = ftp->gen01args.gen01;
        e->p[5] = ftp->gen01args.ifilno;
        e->p[6] = ftp->gen01args.iskptim;
        e->p[7] = ftp->gen01args.iformat;
        e->p[8] = ftp->gen01args.channel;
        strcpy(strarg,ftp->gen01args.strarg);
        e->strarg = strarg;
        gen01raw();
      }
      return (ftp);
    }
}

static void gen01(void)         /* read ftable values from a sound file */
{                               /* stops reading when table is full     */
    if (nargs < 4)
      FTERR(Str(X_939,"insufficient arguments"))
    if (O.gen01defer) {
      /* We're deferring the soundfile load until performance time,
         so allocate the function table descriptor, save the arguments,
         and get out */
      ftalloc();
      ftp->gen01args.gen01 = e->p[4];
      ftp->gen01args.ifilno = e->p[5];
      ftp->gen01args.iskptim = e->p[6];
      ftp->gen01args.iformat = e->p[7];
      ftp->gen01args.channel = e->p[8];
      strcpy(ftp->gen01args.strarg,e->strarg);
      ftp->flen = flen;
      return;
    }
    gen01raw();
}

static void needsiz(long maxend)
{
    long nxtpow;
    maxend -= 1; nxtpow = 2;
    while (maxend >>= 1)
      nxtpow <<= 1;
    printf(Str(X_1073,"non-deferred ftable %d needs size %ld\n"),
           (int)fno, nxtpow);
}

static void gen01raw(void)      /* read ftable values from a sound file */
{                               /* stops reading when table is full     */
    extern  int     close(int);
    static  ARGOFFS argoffs = {0};  /* OUTOCOUNT-not applicable yet */
    static  OPTXT   optxt;          /* create dummy optext  */
    SOUNDIN *p;                     /*   for sndgetset      */
    AIFFDAT *adp;
    extern  int     sndgetset(SOUNDIN *);
    extern  long    getsndin(int, MYFLT *, long, SOUNDIN *);
    SOUNDIN tmpspace;               /* create temporary opds */
    int     fd, truncmsg = 0;
    long    inlocs = 0;

    optxt.t.outoffs = &argoffs;      /* point to dummy OUTOCOUNT */
    p = &tmpspace;
    p->h.optext = &optxt;
    p->ifilno   = &e->p[5];
    p->iskptim  = &e->p[6];
    p->iformat  = &e->p[7];
    p->channel  = (short)e->p[8];
    p->bytrev   = 0;
    if (p->channel < 0 /* || p->channel > ALLCHNLS-1 */) {
      sprintf(errmsg,Str(X_654,"channel %d illegal"),(int)p->channel);
      FTERR(errmsg);
    }
    if (p->channel == 0)                    /* snd is chan 1,2,..8 or all */
      p->channel = ALLCHNLS;
    p->analonly = 0;
    p->STRARG = e->strarg;
    if (!flen)
      printf(Str(X_683,"deferred alloc\n"));
    if ((fd = sndgetset(p))<0)              /* sndinset to open the file */
      FTERR (errmsg)
        if (p->endfile) {
          printf(Str(X_285,"GEN1 early end-of-file\n"));
          goto gn1rtn;
        }
    if (!flen) {                            /* deferred ftalloc requestd: */
      if ((flen = p->framesrem) <= 0)       /*   get minsize from soundin */
        FTERR (Str(X_685,"deferred size, but filesize unknown"))
      if (p->channel == ALLCHNLS)
        flen *= p->nchanls;
      guardreq = 1;
      flenp1 = flen;                      /* presum this includes guard */
      flen -= 1;
      ftalloc();                          /*   alloc now, and           */
      ftp->flen     = flen;
      ftp->lenmask  = 0;                  /*   mark hdr partly filled   */
      ftp->nchanls  = p->nchanls;
      ftp->flenfrms = flen / p->nchanls;  /* ?????????? */
    }
    ftp->gen01args.sample_rate = curr_func_sr;
    ftp->cvtbas = LOFACT * p->sr * onedsr;
    if ((adp = p->aiffdata) != NULL) {            /* if file was aiff,    */
      /* set up some necessary header stuff if not in aiff file */
      if (adp->natcps == 0)                      /* from Jeff Fried */
        adp->natcps = ftp->cvtbas;
      if (adp->gainfac == 0)
        adp->gainfac = FL(1.0);
      ftp->cpscvt = ftp->cvtbas / adp->natcps;  /*    copy data to FUNC */
      ftp->loopmode1 = adp->loopmode1;          /* (getsndin does gain) */
      ftp->loopmode2 = adp->loopmode2;
      ftp->begin1 = adp->begin1;
      ftp->begin2 = adp->begin2;
      if (ftp->loopmode1) /* Greg Sullivan */
        ftp->end1 = adp->end1;
      else
        ftp->end1 = ftp->flenfrms;
      ftp->end1 = adp->end1;
      ftp->end2 = adp->end2;
      if (ftp->end1 > flen || ftp->end2 > flen) {
        long maxend;
        if (O.msglevel & WARNMSG)
          printf(Str(X_288,"WARNING: GEN1: input file truncated by ftable size\n"));
        if ((maxend = ftp->end1) < ftp->end2)
          maxend = ftp->end2;
        printf(Str(X_578,"\tlooping endpoint %ld exceeds ftsize %ld\n"),
               maxend,flen);
        needsiz(maxend);
        truncmsg = 1;
      }
    }
    else {
      ftp->cpscvt = FL(0.0);      /* else no looping possible   */
      ftp->loopmode1 = 0;
      ftp->loopmode2 = 0;
      ftp->end1 = ftp->flenfrms; /* Greg Sullivan */
    }
    if ((inlocs = getsndin(fd, ftp->ftable, flenp1, p)) < 0) { /* read sound */
      fterror(Str(X_286,"GEN1 read error"));                /* with opt gain */
      return;
    }
 gn1rtn:
    if (p->audrem > 0 && !truncmsg && p->framesrem > flen) { /* Reduce msg */
      if (O.msglevel & WARNMSG)
        printf(Str(X_287,"WARNING: GEN1: aiff file truncated by ftable size\n"));
      printf(Str(X_573,"\taudio samps %ld exceeds ftsize %ld\n"),
             p->framesrem, flen);
      needsiz(p->framesrem);     /* ????????????  */
    }
    ftp->soundend = inlocs / ftp->nchanls;   /* record end of sound samps */
    close(fd);
}

static int ftldno = 100;        /* Count table number */

#define FTPLERR(s)     {fterror(s); \
                        die(Str(X_784,"ftable load error"));\
                        return(NULL);}

FUNC *hfgens(EVTBLK *evtblkp)               /* create ftable using evtblk data */
{
    long    ltest, lobits, lomod, genum;

    e = evtblkp;
    fterrcnt = 0;
    if ((fno = (int)e->p[1]) < 0) {         /* fno < 0: remove */
      if ((fno = -fno) > maxfnum) {
        int size = maxfnum+1;
        FUNC **nn;
        int i;
        while (fno >= size) size += MAXFNUM;
        nn = (FUNC**)mrealloc(flist, size*sizeof(FUNC*));
        flist = nn;
        for (i=maxfnum+1; i<size; i++) flist[i] = NULL; /* Clear new section */
        maxfnum = size-1;
      }
      if ((ftp = flist[fno]) == NULL)
        FTPLERR(Str(X_783,"ftable does not exist"))
      flist[fno] = NULL;
      mfree((char *)ftp);
      printf(Str(X_779,"ftable %d now deleted\n"),fno);
      return(NULL);                       /**************/
    }
    if (!fno) {                               /* fno = 0, automatic number */
      do {
        fno = ++ftldno;
      } while (fno<maxfnum || flist[fno]==NULL);
      if (fno==maxfnum) {
        int size = maxfnum+1;
        FUNC **nn;
        int i;
        size += MAXFNUM;
        nn = (FUNC**)mrealloc(flist, size*sizeof(FUNC*));
        flist = nn;
        for (i=maxfnum+1; i<size; i++) flist[i] = NULL; /* Clear new section */
        maxfnum = size-1;
      }
      e->p[1] = (MYFLT)(fno);
    }
    if (fno > maxfnum) {
        int size = maxfnum+1;
        FUNC **nn;
        int i;
        while (fno >= size) size += MAXFNUM;
        nn = (FUNC**)mrealloc(flist, size*sizeof(FUNC*));
        flist = nn;
        for (i=maxfnum+1; i<size; i++) flist[i] = NULL; /* Clear new section */
        maxfnum = size-1;
    }
    if ((nargs = e->pcnt - 4) <= 0)         /* chk minimum arg count */
      FTPLERR(Str(X_941,"insufficient gen arguments"))
    if ((genum = (long)e->p[4]) < 0)
      genum = -genum;
    if (!genum || genum > GENMAX)           /*   & legal gen number */
      FTPLERR(Str(X_850,"illegal gen number"))
    if ((flen = (long)e->p[3]) != 0) {      /* if user flen given       */
      guardreq = flen & 01;                 /*   set guard request flg  */
      flen &= -2;                           /*   flen now w/o guardpt   */
      flenp1 = flen + 1;                    /*   & flenp1 with guardpt  */
      if (flen <= 0 || flen > MAXLEN)
        FTPLERR(Str(X_889,"illegal table length"))
      for (ltest=flen,lobits=0; (ltest & MAXLEN) == 0; lobits++,ltest<<=1);
      if (ltest != MAXLEN)                  /*   flen must be power-of-2 */
        FTPLERR(Str(X_889,"illegal table length"))
      lenmask = flen-1;
      ftalloc();                            /*   alloc ftable space now */
      ftp->fno = fno;
      ftp->flen = flen;
      ftp->lenmask = lenmask;               /*   init hdr w powof2 data */
      ftp->lobits = lobits;
      lomod = MAXLEN / flen;
      ftp->lomask = lomod - 1;
      ftp->lodiv = FL(1.0)/((MYFLT)lomod);       /*    & other useful vals    */
      tpdlen = TWOPI / flen;
      ftp->nchanls = 1;                      /*    presume mono for now   */
      ftp->flenfrms = flen;     /* Is this necessary?? */
    }
    else if (genum != 1 && genum != 23 && genum != 28)
      /* else defer alloc to gen01|gen23|gen28 */
      FTPLERR(Str(X_684,"deferred size for GEN1 only"))
    printf(Str(X_782,"ftable %d:\n"), fno);
    (*gensub[genum])();                     /* call gen subroutine  */
    if (!fterrcnt)
      ftresdisp();                          /* rescale and display */
    return(ftp);
}

int ftgen(FTGEN *p)                    /* set up and call any GEN routine */
{
    int nargs;
    MYFLT *fp;
    FUNC *ftp;

    if (ftevt == NULL) {
      ftevt = (EVTBLK *)mcalloc((long)sizeof(EVTBLK) + FTPMAX * sizeof(MYFLT));
      ftevt->opcod = 'f';
    }
    fp = &ftevt->p[1];
    *fp++ = *p->p1;                               /* copy p1 - p5 */
    *fp++ = FL(0.0);                              /* force time 0    */
    *fp++ = *p->p3;
    *fp++ = *p->p4;
    *fp++ = *p->p5;
    if ((nargs = p->INOCOUNT - 5) > 0) {
      MYFLT **argp = p->argums;
      while (nargs--)                             /* copy rem arglist */
        *fp++ = **argp++;
    }
    if (ftevt->p[5] == SSTRCOD) {                 /* if string p5    */
      int n = (int)ftevt->p[4];
      if (n<0) n = -n;
      if (n == 1  ||
          n == 23 ||
          n == 28) {              /*   must be Gen01, 23 or 28 */
        ftevt->strarg = p->STRARG;
      }
      else {
        return initerror(Str(X_788,"ftgen string arg not allowed"));
      }
    }
    else ftevt->strarg = NULL;                    /* else no string */
    ftevt->pcnt = p->INOCOUNT;
    if ((ftp = hfgens(ftevt)) != NULL)            /* call the fgen  */
      *p->ifno = (MYFLT)ftp->fno;                 /* record the fno */
    else if (ftevt->p[1] >=0)
      return initerror(Str(X_787,"ftgen error"));
    }
    return OK;
}

int ftload(FTLOAD *p)
{
    MYFLT **argp = p->argums;
    char filename[MAXNAME];
    int nargs;
    FILE *file;

    if ((nargs = p->INOCOUNT - 2) <= 0) goto err2;

    if (*p->ifilno == SSTRCOD) { /* if char string name given */
      if (p->STRARG == NULL) strcpy(filename,unquote(currevent->strarg));
      else strcpy(filename,unquote(p->STRARG));    /* unquote it,  else use */
    }
    if (*p->iflag <= 0) {
      if (!(file = fopen(filename, "rb"))) goto err3;
      while (nargs--)  {
        FUNC header;

        fno = (int) **argp;
        fread(&header, sizeof(FUNC)-sizeof(MYFLT)-SSTRSIZ, 1, file);
        /* ***** Need to do byte order here ***** */
        flen = header.flen;
        header.fno = fno;
        if ((ftp = ftfind(*argp)) != NULL) {
          MYFLT *table = ftp->ftable;
          memcpy(ftp, &header, sizeof(FUNC)-sizeof(MYFLT)-SSTRSIZ);
          ftalloc();
          fread(table, sizeof(float), flen, file);
        /* ***** Need to do byte order here ***** */
        }
        else goto err;
        argp++;
      }
    }
    else {
      if (!(file = fopen(filename, "r"))) goto err3;
      while (nargs--) {
        FUNC header;
        char s[64], *s1;
        fno = (int) **argp;
        /* IMPORTANT!! If FUNC structure and/or GEN01ARGS structure
           will be modified, the following code has to be modified too */
        fgets(s, 64, file);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.flen = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.lenmask = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.lobits = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.lomask = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.lodiv = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.cvtbas = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.cpscvt = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.loopmode1 = (short) atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.loopmode2 = (short) atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.begin1 = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.end1 = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.begin2 = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.end2 = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.soundend = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.flenfrms = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.nchanls = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.fno = atol(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.gen01 = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.ifilno = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.iskptim = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.iformat = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.channel = (MYFLT)atof(s1);
        fgets(s, 64, file);  s1 = strchr(s, ' ')+1;
        header.gen01args.sample_rate = (MYFLT)atof(s1);
        fgets(s, 64, file);
        /* WARNING! skips header.gen01args.strarg from saving/loading
           in text format */
        flen = header.flen;
        header.fno = fno;
        if ((ftp = ftfind(*argp)) != NULL) {
          long j;
          MYFLT *table = ftp->ftable;
          memcpy(ftp, &header, sizeof(FUNC)-sizeof(MYFLT));
          ftalloc();
          for (j=0; j < flen; j++) {
            fgets(s, 64, file);
            table[j] = (MYFLT)atof(s);
          }
          fgets(s, 64, file);
        }
        else goto err;
        argp++;
      }
    }
    fclose(file);
    return OK;
 err:
    return initerror(Str(X_1772,"ftload: Bad table number. Loading is possible "
              "only into existing tables."));
 err2:
    return initerror(Str(X_1773,"ftload: no table numbers"));
 err3:
    return initerror(Str(X_1774,"ftload: unable to open file"));
}

int ftload_k (FTLOAD_K *p)
{
    FTLOAD *pp = &(p->p);
    if (*p->ktrig)
      ftload(pp);
    return OK;
}


int ftsave(FTLOAD *p)
{
    MYFLT **argp = p->argums;
    char filename[MAXNAME];
    int nargs;
    FILE *file;

    if ((nargs = p->INOCOUNT - 2) <= 0) goto err2;

    if (*p->ifilno == SSTRCOD) { /* if char string name given */
      if (p->STRARG == NULL) strcpy(filename,unquote(currevent->strarg));
      else strcpy(filename,unquote(p->STRARG));    /* unquote it,  else use */
    }
    if (*p->iflag <= 0) {
      if (!(file = fopen(filename, "wb"))) goto err3;
      while (nargs--) {
        FUNC *ftp;

        if ((ftp = ftfind(*argp)) != NULL) {
          MYFLT *table = ftp->ftable;
          long flen = ftp->flen;
          fwrite(ftp, sizeof(FUNC)-sizeof(MYFLT)-SSTRSIZ, 1, file);
          fwrite(table, sizeof(MYFLT), flen, file);
        }
        else
          goto err;
        argp++;
      }
    }
    else {
      if (!(file = fopen(filename, "w"))) goto err3;
      while (nargs--)  {
        FUNC *ftp;

        if ((ftp = ftfind(*argp)) != NULL) {
          long flen = ftp->flen;
          long j;
          MYFLT *table = ftp->ftable;
          /* IMPORTANT!! If FUNC structure and/or GEN01ARGS structure
             will be modified, the following code has to be modified too */
          fprintf(file,"======= TABLE %ld size: %ld values ======\n",
                  ftp->fno, ftp->flen);
          fprintf(file,"flen: %ld\n", ftp->flen);
          fprintf(file,"lenmask: %ld\n", ftp->lenmask);
          fprintf(file,"lobits: %ld\n",ftp->lobits);
          fprintf(file,"lomask: %ld\n",ftp->lomask);
          fprintf(file,"lodiv: %f\n",ftp->lodiv);
          fprintf(file,"cvtbas: %f\n",ftp->cvtbas);
          fprintf(file,"cpscvt: %f\n",ftp->cpscvt);
          fprintf(file,"loopmode1: %d\n", (int) ftp->loopmode1);
          fprintf(file,"loopmode2: %d\n", (int) ftp->loopmode2);
          fprintf(file,"begin1: %ld\n",ftp->begin1);
          fprintf(file,"end1: %ld\n",ftp->end1);
          fprintf(file,"begin2: %ld\n",ftp->begin2);
          fprintf(file,"end2: %ld\n",ftp->end2);
          fprintf(file,"soundend: %ld\n",ftp->soundend);
          fprintf(file,"flenfrms: %ld\n",ftp->flenfrms);
          fprintf(file,"nchnls: %ld\n",ftp->nchanls);
          fprintf(file,"fno: %ld\n",ftp->fno);

          fprintf(file,"gen01args.gen01: %f\n",ftp->gen01args.gen01);
          fprintf(file,"gen01args.ifilno: %f\n",ftp->gen01args.ifilno);
          fprintf(file,"gen01args.iskptim: %f\n",ftp->gen01args.iskptim);
          fprintf(file,"gen01args.iformat: %f\n",ftp->gen01args.iformat);
          fprintf(file,"gen01args.channel: %f\n",ftp->gen01args.channel);
          fprintf(file,"gen01args.sample_rate: %f\n",ftp->gen01args.sample_rate);
          /* WARNING! skips ftp->gen01args.strarg from saving/loading in
             text format */
          fprintf(file,"---------END OF HEADER--------------\n");

          for (j=0; j < flen; j++) {
            MYFLT val = table[j];
            fprintf(file,"%f\n",val);
          }
          fprintf(file,"---------END OF TABLE---------------\n");
        }
        else goto err;
        argp++;
      }
    }
    fclose(file);
    return OK;
 err:
    return initerror(Str(X_1775,
                         "ftsave: Bad table number. Saving is possible only "
                         "for existing tables."));
 err2:
    initerror(Str(X_1776,"ftsave: no table numbers"));
    return NOTOK;
 err3:
    initerror(Str(X_1777,"ftsave: unable to open file"));
    return NOTOK;
}

int ftsave_k_set(FTLOAD_K *p)
{
    memcpy(&(p->p.h), &(p->h), sizeof(OPDS));
    p->p.INOCOUNT = p->INOCOUNT -1;
    p->p.ifilno = p->ifilno;
    p->p.iflag = p->iflag;
    memcpy( p->p.argums, p->argums, sizeof(MYFLT*)*p->INOCOUNT - 3);
    return OK;
}

int ftsave_k(FTLOAD_K *p)
{
    FTLOAD *pp = &(p->p);
    if (*p->ktrig)
      ftsave(pp);
    return OK;
}

