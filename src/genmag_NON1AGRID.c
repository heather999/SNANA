/*******************************************************
 Created March 2016 by R.Kessler

 Plug-in module for simulation to generate nonIa (CC or whatever) SNe. 
 The basic idea here is to read pre-computed magnitudes on a grid of
 { log(z), EPOCH, NON1A_INDEX }, select random NON1A_INDEX based on
 the weight of each NON1A template, and then interpolate to the
 generated EPOCH and redshift.

 The input GRID file is in FITS format, and it can be generated
 by the SNANA simulation using  "GENSOURCE: GRID", or it can
 be generated by an external code.
 
 ******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "fitsio.h"
#include "sntools.h"
#include "sntools_grid.h"
#include "genmag_NON1AGRID.h"
#include "MWgaldust.h"

// =========================================
void init_genmag_NON1AGRID(char *GRIDFILE, double FRAC_PEC1A) {

  // Created March 2016
  // Read input GRIDFILE and load structure NON1AGRID.
  // If FRAC_PEC1A>0, then 
  //  * sort to have NON1A first followed by FRAC_PEC1A
  //  * re-normalize NON1A and PEC1A separately, but with
  //    total WGTSUM=1.0
  //
  //  Aug 14, 2016: new input arg FRAC_PEC1A

  char fnam[] = "init_genmag_NON1AGRID" ;
  char FILENAME[MXPATHLEN] ;  // full filenam of GRIDFILE
  char PATH_NON1AGRID[MXPATHLEN];
  int  gzipFlag; 
  FILE *fp ;

  // --------------- BEGIN ---------------

  sprintf(BANNER,"%s: init LC grid vs. index and redshift\n", fnam);
  print_banner(BANNER);

  sprintf(PATH_NON1AGRID,"%s/models/NON1AGRID", PATH_SNDATA_ROOT );
  fp = snana_openTextFile(1,PATH_NON1AGRID,       // (I) public area  
			  GRIDFILE,             // (I) filename  
			  FILENAME,            // (O) full filename  
			  &gzipFlag );         // (O) gzip flag
  
  if ( fp == NULL ) {
    sprintf(c1err,"Could not open NON1GRID file:");
    sprintf(c2err,"%s", GRIDFILE);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }
  fclose(fp);

  // read and fill NON1AGRID
  int OPT_READ =1 ;  // verbose mode
  fits_read_SNGRID(OPT_READ, FILENAME, &NON1AGRID );  

  NON1AGRID.FRAC_PEC1A = FRAC_PEC1A ;
  renorm_wgts_SNGRID(&NON1AGRID); // set sum of weights to 1

  dump_SNGRID(&NON1AGRID);   // screen dump of each template

  fflush(stdout);

  return ;

} // end init_genmag_NON1AGRID


// =========================================
void genmag_NON1AGRID (int ifilt_obs, double mwebv, double z,
		       double RVhost, double AVhost,
		       double ranWgt, double ranSmear,
		       int NOBS, double *TobsList, 
		       double *magList, double *magerrList, double *magSmear){
 
  // Mar 2016
  // return *magList and *magerrList for the NOBS TobsList values.
  // Beware that random numbers ranWgt [0-1] and ranSmear [GauRan, sig=1]
  // should be the same for each band and epoch to maintain coherence.
  // RanWgt is used to select a random NON1A_INDEX based on their
  // input wights [read from GRID], and ranSmear is used to select 
  // a random MAGSMEAR.
  //
  // Jan 18 2019: abort on undefined filter.

  int obs, indx, N_INDEX, i, ifilt ;
  double MAGSMEAR, MAGSMEAR_SIGMA, MAGOFF, z1, Tobs, Trest, MAG, magInterp ;
  double AV_MW, XT_MW, XT_HOST, meanlam_obs ;
  int LDMP = 0; // (ifilt_obs==1 );
  char fnam[] = "genmag_NON1AGRID" ;

  // ----------- BEGIN --------------

  INDEX_NON1AGRID = -9 ;
  N_INDEX = NON1AGRID.NBIN[IPAR_GRIDGEN_SHAPEPAR];
  for(indx=1; indx <= N_INDEX; indx++ ) {
    if ( ranWgt >= NON1AGRID.NON1A_WGTSUM[indx-1] &&
	 ranWgt <= NON1AGRID.NON1A_WGTSUM[indx]  )  {
      INDEX_NON1AGRID = indx;
    }
  }


  MAGOFF         = NON1AGRID.NON1A_MAGOFF[INDEX_NON1AGRID];
  MAGSMEAR_SIGMA = NON1AGRID.NON1A_MAGSMEAR[INDEX_NON1AGRID];

  // store log(z) and iz index in global so that it's only
  // computed once for al NOBS
  LOGZ_NON1AGRID  = log10(z);
  ILOGZ_NON1AGRID = 
    INDEX_GRIDGEN(IPAR_GRIDGEN_LOGZ, LOGZ_NON1AGRID, &NON1AGRID);
  z1 = 1.0 + z ;

  // make sure that redshift is valid
  checkRange_NON1AGRID(IPAR_GRIDGEN_LOGZ, LOGZ_NON1AGRID );

  // find sparse 'ifilt' from ifiltobs
  int NFILT = NON1AGRID.NBIN[IPAR_GRIDGEN_FILTER];
  ifilt = -9 ;
  for(i=0; i < NFILT; i++ ) {
    if ( NON1AGRID.IFILTOBS[i] == ifilt_obs ) { ifilt = i; }
  }

  if ( ifilt < 0 ) {
    sprintf(c1err,"Could not find '%c' filter in NON1AGRID.", 
	    FILTERSTRING[ifilt_obs]);
    sprintf(c2err,"Available NON1AGRID filters are '%s' ", 
	    NON1AGRID.FILTERS);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }
 

  // -------------------------------------------------------
  // get approx Galactic extinction using central wavelength of filter 
  meanlam_obs = NON1AGRID.FILTER_LAMAVG[ifilt];
  AV_MW       = RV_MWDUST * mwebv ;
  XT_MW       = GALextinct ( RV_MWDUST, AV_MW, meanlam_obs, 94 );

  // get extinction from host in rest-frame 
  XT_HOST = GALextinct ( RVhost, AVhost, meanlam_obs/z1, 94 ); 


  /*
  if ( ifilt_obs==4 && NON1AGRID.NON1A_INDEX[INDEX_NON1AGRID] == 203 ) {
    printf(" xxx 203:  MAGSMEAR=%.4f  ranSmear=%.4f  XT_MW=%.4f\n", 
	   ifilt_obs, MAGSMEAR, ranSmear, XT_MW) ;
  }
  */

  MAGSMEAR = (ranSmear * MAGSMEAR_SIGMA ) ;
  *magSmear = MAGSMEAR ; // return arg

  if ( LDMP )   { 
    printf(" xxx --------------------------------------- \n" ); 
    printf(" xxx ifilt_obs=%d  z=%.3f  <lamObs>=%.0f \n",
	   ifilt_obs, z, meanlam_obs);
  }

  // xyz
  // -------------------------------------------------------
  for(obs=0; obs < NOBS;  obs++ ) {
    Tobs = TobsList[obs];
    Trest = Tobs/z1 ;
    checkRange_NON1AGRID(IPAR_GRIDGEN_TREST, Trest);
    magInterp = magInterp_NON1AGRID(ifilt,INDEX_NON1AGRID,z,Trest);

    MAG = 
      magInterp 
      //      + MAGOFF // already applied to make GRID
      + MAGSMEAR
      + XT_MW 
      + XT_HOST
      ;
    
    if( LDMP && Trest < -2.0 ) {
      printf(" xxx Trest=%6.2f  magInterp=%.2f  XT[MW,HOST]=%.2f,%.2f \n",
	     Trest, magInterp, XT_MW, XT_HOST); // xxx REMOVE
    }

    magList[obs]    = MAG ;
    magerrList[obs] = 0.1000; // dummy -> has no effect
  }



  //  if ( ifilt_obs ==5 ) {  debugexit(fnam);  }

  return;

} // end genmag_NON1AGRID

// ===============================================
void  checkRange_NON1AGRID(int IPAR, double VAL) {

  // ABORT if value VAL is outside grid range for IPAR.
  int ABORT = 0 ;
  char fnam[] = "checkRange_NON1AGRID" ;

  if ( VAL < NON1AGRID.VALMIN[IPAR] ) { ABORT = 1; }
  if ( VAL > NON1AGRID.VALMAX[IPAR] ) { ABORT = 1; }

  if ( ABORT ) {
    sprintf(c1err,"Invalid %s = %f", NON1AGRID.NAME[IPAR], VAL ) ;
    sprintf(c2err,"Valid range is %f to %f ",
	    NON1AGRID.VALMIN[IPAR], NON1AGRID.VALMAX[IPAR] );
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }

} // end checkRange_NON1AGRID

// ===============================================
double  magInterp_NON1AGRID(int ifilt, int NON1A_INDEX, double z, double Trest) {

  int EPGRID, IZGRID, iz, ep ;
  double WGT, MAG, MAGSUM, WGTSUM, Dz, DT, logz ;

  int NBIN_logz  = NON1AGRID.NBIN[IPAR_GRIDGEN_LOGZ] ;
  int NBIN_Trest = NON1AGRID.NBIN[IPAR_GRIDGEN_TREST] ;
  double BINSIZE_logz  = (double)NON1AGRID.BINSIZE[IPAR_GRIDGEN_LOGZ] ;
  double BINSIZE_Trest = (double)NON1AGRID.BINSIZE[IPAR_GRIDGEN_TREST] ;
  double logz_node, Trest_node;
  int LDMP, CORNER, ABORT;
  char fnam[] = "magInterp_NON1AGRID" ;

  // -------------- BEGIN -------------

  LDMP = (NON1A_INDEX == -19 && ifilt == 0 && 
	  z > 0.30 && fabs(Trest+14.9)<.1 ) ;

  MAG = MAGSUM = WGTSUM = 0.0 ;

  EPGRID = INDEX_GRIDGEN(IPAR_GRIDGEN_TREST, Trest, &NON1AGRID );
  IZGRID = ILOGZ_NON1AGRID;        // avoid re-computation
  logz   = log10(z);

  if ( IZGRID == NBIN_logz  ) { IZGRID-- ; }
  if ( EPGRID == NBIN_Trest ) { EPGRID-- ; }

  if ( LDMP ) {
    printf(" xxx ---------- %s DUMP --------------- \n", fnam);
    printf(" xxx ifilt=%d(%c)  NON1A_INDEX=%d  z=%.4f (logz=%.5f) Trest=%.1f \n",
	   ifilt,  NON1AGRID.FILTERS[ifilt], NON1A_INDEX, z, logz, Trest) ;    
    printf(" xxx BINSIZE(logz,Trest) = %f, %f \n", 
	   BINSIZE_logz, BINSIZE_Trest);
  }

  // loop over four corners in z,Tobs space and take
  // mag-weighted average, where the weight at each corner 
  // is the 1/(distance to the corner).

  CORNER = ABORT = 0;
  
  for(iz=IZGRID; iz <= IZGRID+1; iz++ ) {
    for(ep=EPGRID; ep <= EPGRID+1; ep++ ) {

      CORNER++ ;
      MAG = magNode_NON1AGRID(ifilt,NON1A_INDEX,iz,ep);

      logz_node  = (double)NON1AGRID.VALUE[IPAR_GRIDGEN_LOGZ][iz];
      Trest_node = (double)NON1AGRID.VALUE[IPAR_GRIDGEN_TREST][ep];

      Dz = logz  - logz_node ;
      Dz /= BINSIZE_logz ; // normalize distance to 0-1

      DT = Trest - Trest_node ;
      DT /= BINSIZE_Trest ; // normalize distance to 0-1

      if ( fabs(Dz) > 1.0001 || fabs(DT) > 1.0001 ) {
	printf("\n PRE-ABORT DUMP: \n");
	printf("\t ifilt=%d  NON1A_INDEX=%d  z=%.4f  Trest=%.1f \n",
	       ifilt,  NON1A_INDEX, z, Trest) ;
	printf("\t EPGRID=%d  ep=%d  IZGRID=%d  iz=%d \n", 
	       EPGRID, ep, IZGRID, iz);
	printf("\t logz=%.5f  logz_node=%.5f  Dz=%f \n",
	       logz, logz_node, Dz);
	printf("\t Trest=%.3f  Trest_node=%.3f  DT=%f \n",
	       Trest, Trest_node, DT);
	ABORT = 1;
      }

      //SQD = (Dz*Dz + DT*DT) ;  D=sqrt(SQD);    WGT = 1.0/(D + 1.0E-12);

      WGT = (1.0 - fabs(Dz) ) * (1.0 - fabs(DT) );
      MAGSUM += (WGT*MAG);    WGTSUM += WGT ;

      if ( LDMP ) {
	printf("\t %d xxx iz=%d(logz=%.5f) ep=%d(Trest=%.2f) \n",
	       CORNER, iz, logz_node, ep, Trest_node );
	printf("\t %d xxx Dz=%f  DT=%f\n", CORNER, Dz, DT);
	printf("\t %d xxx WGT=%f  MAG=%f \n", CORNER, WGT, MAG);
	fflush(stdout);
      }
    }
  }

  MAG = MAGSUM/WGTSUM ;

  if ( LDMP ) 
    { printf(" xxx interpMag = %f/%f = %f \n",
	     MAGSUM,WGTSUM,MAG);fflush(stdout);}


  if( ABORT ) {
    sprintf(c1err,"Invalid interp distance (Dz>1 or DT >1)");
    sprintf(c2err,"Each must be < 1. See PRE-ABORT messages above.");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );	
  }

  return(MAG);

} // end magInterp_NON1AGRID


// =============================================
double magNode_NON1AGRID(int ifilt, int NON1A_INDEX, int iz, int ep) {

  // Return mag at grid-node specified by input indices.

  short  *I2PTR, I2MAG ;
  int ILC, IOFF_FILT, IPTROFF ;
  int NBIN_Trest = NON1AGRID.NBIN[IPAR_GRIDGEN_TREST] ;
  double DMAG ;
  char fnam[] = "magNode_NON1AGRID" ;
  
  // ---------------- BEGIN --------------

  ILC = 1 
    + (NON1AGRID.ILCOFF[IPAR_GRIDGEN_SHAPEPAR] * (NON1A_INDEX-1) )
    + (NON1AGRID.ILCOFF[IPAR_GRIDGEN_LOGZ]     * (iz-1) )
    ;

  IPTROFF =  NON1AGRID.PTR_GRIDGEN_LC[ILC] ;
  I2PTR   = &NON1AGRID.I2GRIDGEN_LCMAG[IPTROFF];

  // make sure that 1st word is BEGIN-LC marker             
  if ( I2PTR[0] != MARK_GRIDGEN_LCBEGIN ) {
    sprintf(c1err,"First I*2 word of ILC=%d is %d .", ILC, I2PTR[0] );
    sprintf(c2err,"But expected %d", MARK_GRIDGEN_LCBEGIN );
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }

  IOFF_FILT   = (ifilt*NBIN_Trest) + NPADWD_LCBEGIN - 1 ;

  I2MAG = I2PTR[IOFF_FILT+ep] ;  
  DMAG  = ((double)I2MAG) / GRIDGEN_I2LCPACK ;

  return(DMAG);

} // end magNode_NON1AGRID

// ========================================
double fetchInfo_NON1AGRID(char *what) {

  // Jan 8 2017: split ITYPE_USER -> ITYPE_[USER_AUTO,USER]

  int ITMP ;
  if( strcmp(what,"NON1A_INDEX") == 0 )  { 
    ITMP = NON1AGRID.NON1A_INDEX[INDEX_NON1AGRID] ;
    return ( (double)ITMP ) ;
  }

  if( strcmp(what,"NON1A_ITYPE_AUTO") == 0 )  { 
    ITMP = NON1AGRID.NON1A_ITYPE_AUTO[INDEX_NON1AGRID] ;
    return ( (double)ITMP ) ;
  }

  if( strcmp(what,"NON1A_ITYPE_USER") == 0 )  { 
    ITMP = NON1AGRID.NON1A_ITYPE_USER[INDEX_NON1AGRID] ;
    return ( (double)ITMP ) ;
  }

  return(0.0);

} // end fetchInfo_NON1AGRID
