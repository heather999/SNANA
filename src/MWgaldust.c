
/*
 * MWgaldust_av is an interface to schlegel's dust_getval for
 * use inside of snana.  All the inputs are set to default values
 * and the calling sequence sequence is internal rather than via the
 * command line.  The standard input is RA,DEC in degrees which are translated
 * to galactic coordinates which is what dust_getval expects.
 * Below is the original documentation of dust_getval, and all the
 * needed routines from Schlegel are combined in one file below this
 * simple interface subroutine.
 * D.Cinabro 6 June 2007
 *
 * Usage: void MWgaldust(double RA,double DEC,double *avgal, double galebmv)
 * RA,DEC are in degrees, avgal is the extnction in Sloan u,g,r,i,z filters,
 * and galebmv is the Milky Way galaxy E(B-V) redening from the Schlegal map
 */

/******************************************************************************/
/*
 * NAME:
 *   dust_getval
 *
 * PURPOSE:
 *   Read values from BH files or our dust maps.
 *
 *   Either the coordinates "gall" and "galb" must be set, or these coordinates
 *   must exist in the file "infile".  Output is written to standard output
 *   or the file "outfile".
 *
 * CALLING SEQUENCE:
 *   dust_getval gall galb map=map ipath=ipath interp=interp noloop=noloop \
 *    infile=infile outfile=outfile verbose=verbose
 *
 * OPTIONAL INPUTS:
 *   gall:       Galactic longitude(s) in degrees
 *   galb:       Galactic latitude(s) in degrees
 *   map:        Set to one of the following (default is "Ebv"):
 *               I100: 100-micron map in MJy/Sr
 *               X   : X-map, temperature-correction factor
 *               T   : Temperature map in degrees Kelvin for n=2 emissivity
 *               Ebv : E(B-V) in magnitudes
 *               mask: Mask values
 *   infile:     If set, then read "gall" and "galb" from this file
 *   outfile:    If set, then write results to this file
 *   interp:     Set this flag to "y" to return a linearly interpolated value
 *               from the 4 nearest pixels.
 *               This is disabled if map=='mask'.
 *   noloop:     Set this flag to "y" to read entire image into memory
 *               rather than reading pixel values for one point at a time.
 *               This is a faster option for reading a large number of values,
 *               but requires reading up to a 64 MB image at a time into
 *               memory.  (Actually, the smallest possible sub-image is read.)
 *   verbose:    Set this flag to "y" for verbose output, printing pixel
 *               coordinates and map values
 *   ipath:      Path name for dust maps; default to path set by the
 *               environment variable $DUST_DIR/maps, or to the current
 *               directory.
 *
 * EXAMPLES:
 *   Read the reddening value E(B-V) at Galactic (l,b)=(12,+34.5),
 *   interpolating from the nearest 4 pixels, and output to the screen:
 *   % dust_getval 12 34.5 interp=y
 *
 *   Read the temperature map at positions listed in the file "dave.in",
 *   interpolating from the nearest 4 pixels, and output to file "dave.out".
 *   The path name for the temperature maps is "/u/schlegel/".
 *   % dust_getval map=T ipath=/u/schlegel/ interp=y \
 *     infile=dave.in outfile=dave.out 
 *
 * DATA FILES FOR SFD MAPS:
 *   SFD_dust_4096_ngp.fits
 *   SFD_dust_4096_sgp.fits
 *   SFD_i100_4096_ngp.fits
 *   SFD_i100_4096_sgp.fits
 *   SFD_mask_4096_ngp.fits
 *   SFD_mask_4096_sgp.fits
 *   SFD_temp_ngp.fits
 *   SFD_temp_sgp.fits
 *   SFD_xmap_ngp.fits
 *   SFD_xmap_sgp.fits
 *
 * DATA FILES FOR BH MAPS:
 *   hinorth.dat
 *   hisouth.dat
 *   rednorth.dat
 *   redsouth.dat
 *
 * REVISION HISTORY:
 *   Written by D. Schlegel, 19 Jan 1998, Durham
 *   5-AUG-1998 Modified by DJS to read a default path from an environment
 *              variable $DUST_DIR/map.
 *
 *  Jan 28, 2007 D.Cinabro : add MWEBV = E(B-V) as additional output arg
 *
 *
 *  Oct 7, 2007 R.Kessler move contents of $DUST_DIR/maps to
 *              $SNDATA_ROOT/MWDUST/ and change local path 
 *
 * Mar 02, 2012: uchar    pLabel_temp[8] -> pLabel_temp[9] 
 *                  (bug found by S.Rodney)
 *
 * Feb 22, 2013: RK - update to compile with c++
 *
 * Sep 21 2013 RK - move GAL-related functions from sntools.c to here
 * 
 * Oct 29 2013 RK - move slalib routines to sntools.c
 *
 * Jan 28 2020 RK - abort if WAVE>12000 and using Fitz99 color law
 * 
 * Oct 9 2021 DB and DS - update Fitz/Odonell ratio and extend WAVE to 15000
 */
/**************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h> 
#include <ctype.h>

#include "MWgaldust.h"
#include "sntools.h"
//#include "sntools_cosmology.h"


// #####################################################
//
//   snana-GALextinct functions (moved from sntools)
//
// #####################################################



// *****************************
// mangled routines for fortran
double galextinct_(double *RV, double *AV, double *WAVE, int *OPT, double *PARLIST, char *callFun ) {
  return GALextinct(*RV, *AV, *WAVE, *OPT, PARLIST, callFun);
}
void text_mwoption__(char *nameOpt, int  *OPT, char *TEXT, char *callFun) {
  text_MWoption(nameOpt,*OPT,TEXT, callFun);
}
void modify_mwebv_sfd__(int *OPT, double *RA, double *DECL, 
			double *MWEBV, double *MWEBV_ERR) {
  modify_MWEBV_SFD(*OPT, *RA, *DECL, MWEBV, MWEBV_ERR) ;
}

// **********************************************
void text_MWoption(char *nameOpt, int OPT, char *TEXT, char *callFun) {

  // Created Sep 19 2013
  // Return corresponding *TEXT description of 
  // integer option OPT for 
  // *nameOpt = "MWCOLORLAW" or "COLORLAW" or "MWEBV" or "EBV"
  // ABORT on invalid OPT.

  char fnam[60] ;
  concat_callfun_plus_fnam(callFun, "text_MWoption", fnam); // return fnam

  // ------------------ BEGIN ------------------

  sprintf(TEXT,"NULL");


  // ----------------------------------------
  if ( strcmp(nameOpt,"MWCOLORLAW") == 0  || 
       strcmp(nameOpt,"COLORLAW"  ) == 0 ) {

    if ( OPT == OPT_MWCOLORLAW_OFF ) 
      { sprintf(TEXT,"No Extinction");  }

    else if ( OPT == OPT_MWCOLORLAW_CCM89 ) 
      { sprintf(TEXT,"CCM89");  }

    else if ( OPT == OPT_MWCOLORLAW_ODON94 ) 
      { sprintf(TEXT,"CCM89+ODonell94");  }  

    else if ( OPT == OPT_MWCOLORLAW_FITZ99_APPROX ) 
      { sprintf(TEXT,"Fitzpatrick99 (approx fit to F99/ODonnel94)");  }

    else if ( OPT == OPT_MWCOLORLAW_FITZ99_EXACT ) 
      { sprintf(TEXT,"Fitzpatrick99 (cubic spline)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_GORD03 ) 
      { sprintf(TEXT,"Gordon03 (cubic spline)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_FITZ04 ) 
      { sprintf(TEXT,"Fitzpatrick04 (cubic spline)");  }

    else if ( OPT == OPT_MWCOLORLAW_GOOB08 ) 
      { sprintf(TEXT,"Goobar08 (power law)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_MAIZ14 ) 
      { sprintf(TEXT,"MaizApellaniz14 (cubic spline)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_GORD16 ) 
      { sprintf(TEXT,"Gordon16 (cubic spline)");  }

    else if ( OPT == OPT_MWCOLORLAW_FITZ19_LINEAR ) 
      { sprintf(TEXT,"Fitzpatrick19 (linear interpolation)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_FITZ19_CUBIC ) 
      { sprintf(TEXT,"Fitzpatrick19 (cubic spline)");  }
    
    else if ( OPT == OPT_MWCOLORLAW_GORD23 ) 
      { sprintf(TEXT,"Gordon23");  }

    else if ( OPT == OPT_MWCOLORLAW_SOMM25 ) 
      { sprintf(TEXT,"Sommovigo25 (Learning the Universe)");  }
    
    else {
      sprintf(c1err,"Invalid OPT_MWCOLORLAW = %d", OPT);
      sprintf(c2err,"Check OPT_MWCOLORAW_* in MWgaldust.h");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
    }
  }

  // ----------------------------------------
  else if ( strcmp(nameOpt,"MWEBV")== 0 || 
	    strcmp(nameOpt,"EBV"  )== 0 ) {

    if ( OPT == OPT_MWEBV_OFF ) 
      { sprintf(TEXT,"No Extinction");  }

    else if ( OPT == OPT_MWEBV_FILE ) 
      { sprintf(TEXT,"FILE value (SIMLIB or data header)");  }

    else if ( OPT == OPT_MWEBV_SFD98 ) 
      { sprintf(TEXT,"SFD98");  }

    else if ( OPT == OPT_MWEBV_Sch11_PS2013 ) 
      { sprintf(TEXT,"Schlafly11+PS2013: 0.86*MWEBV(SFD98)" );  }

    else {
      sprintf(c1err,"Invalid OPT_MWEBV = %d", OPT);
      sprintf(c2err,"Check OPT_MWEBV_* in sntools.h");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

  }

  // ----------------------------------------
  else {
    sprintf(c1err,"Invalid nameOpt = %s", nameOpt );
    sprintf(c2err,"Valid nameOpt are COLORLAW and EBV");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);       
  }
  
} // end of text_MWoption

// **************************************
void modify_MWEBV_SFD(int OPT, double RA, double DECL, 
		      double *MWEBV, double *MWEBV_ERR) {

  // Created Sep 21 2013 by R.Kessler
  // According to input integer option OPT, modify MWEBV & MWEBV_ERR. 
  // Note that input MWEBV arguments are the FILE values, 
  // and may be changed !
  // Input RA and DEC may or may not be used depending on the option.

  double MWEBV_INP, MWEBV_OUT=0.0, MWEBV_ERR_OUT=0.0, MWEBV_SFD98=0.0 ;
  double dumXT[10] ;
  //  char fnam[] = "modify_MWEBV_SFD" ;

  // ----------- BEGIN -----------

  MWEBV_INP = *MWEBV ;
  MWEBV_OUT = -999.0 ;

  // check trival option with no Galactic extiction
  if ( OPT == OPT_MWEBV_OFF )  { 
    MWEBV_OUT = MWEBV_ERR_OUT = 0.0 ; 
    goto LOAD_OUTPUT ; 
  }  

  // ----------------------------------------------------
  // always compute MWEBV_SFD98 since many options need this.
  if ( OPT >= OPT_MWEBV_SFD98 )  
    {  MWgaldust(RA, DECL, dumXT, &MWEBV_SFD98 );  }
  else 
    { MWEBV_SFD98 = -999. ; }

  // ----------------------------------------------------

  if ( OPT == OPT_MWEBV_FILE )  {  
      // already defined extinction from file -> use it 
      MWEBV_OUT     = *MWEBV ;      // do nothing
      MWEBV_ERR_OUT = *MWEBV_ERR ;    
  }    

  else if ( OPT == OPT_MWEBV_SFD98 )  { 
    // force SFD98 regardless of input/FILE value
    MWEBV_OUT     = MWEBV_SFD98 ;
    MWEBV_ERR_OUT = MWEBV_SFD98/6.0 ;
  } 

  else if ( OPT == OPT_MWEBV_Sch11_PS2013 ) {

    MWEBV_OUT = 0.86 * MWEBV_SFD98 ;  // Apr 13 2018: Dan suggests this

    //    MWEBV_ERR_OUT = 0.1 * MWEBV_OUT ; // error is 10% of EBV
    MWEBV_ERR_OUT = 0.05 * MWEBV_OUT ; // Apr 13 2018

  }

  // load output
 LOAD_OUTPUT:
  *MWEBV     = MWEBV_OUT ;
  *MWEBV_ERR = MWEBV_ERR_OUT ;


} // end of modify_MWEBV_SFD


// **********************************************
double GALextinct(double RV, double AV, double WAVE, int OPT, double *PARLIST, char *callFun) {

/*** 
  
  Input : 
    AV   = V band (defined to be at 5495 Angstroms) extinction
    RV   = assumed A(V)/E(B-V) (e.g., = 3.1 in the LMC)
    WAVE = wavelength in angstroms

    OPT=89 => use original CCM89 :
              Cardelli, Clayton, & Mathis (1989) extinction law.

    OPT=94 => use update from O'Donell

    OPT=-99 => use Fitzpatrick 1999 (PASP 111, 63) as implemented by 
              D.Scolnic with polynomial fit to ratio of F'99/O'94 vs. lambda.
              O'94 is the opt=94 option and F'99 was computed from 
              http://idlastro.gsfc.nasa.gov/ftp/pro/astro/fm_unred.pro
              Deprecated to OPT=-99 from OPT=99 September 25 2024.
              Only reliable for RV=3.1.

    OPT=99 => use Fitzpatrick 1999 (PASP 111, 63) as implemented by S. Thorp.
                This version directly evaluates the cubic spline in inverse
                wavelength, as defined by the fm_unred.pro code. Consistent
                with extinction.py by K. Barbary, and BAYESN F99 implementation.
                Promoted to OPT=99 September 25 2024.

    OPT=203 => use Gordon et al. 2003 (ApJ 594, 279) as implemented by S. Thorp.
                This is the SMC bar dust law. No significant UV bump. Only
                defined for RV=2.74, will abort for all other values. This
                is the refined version from Gordon et al. 2016 (ApJ, 826, 104),
                based on the implementation in Gordon 2024 (JOSS 9, 7023).
                Not recommended for use as a standalone dust law.

    OPT=204 => use Fitzpatrick 2004 (ASP Conf. Ser. 309, 33) as implemented by S. Thorp.
                This uses the same curve as Fitzpatrick 99, but with updated
                behaviour in the IR. Checked against implementation by
                Gordon 2024 (JOSS 9, 7023): github.com/karllark/dust_extinction.

    OPT=208 => use Goobar 2008 (ApJ 686, L103) power law for circumstellar dust.
                This is a two parameter model, controlled by P and A.
                P is read from PARLIST[0];
                A is read from PARLIST[1].
                RV argument is ignored.
                Aborts if the required PARLIST entries are not present or
                within the valid ranges. P=-1.5, A=0.9 gives MW-like circum-
                stellar dust (G08 fit to Draine 2003). P=-2.5, A=0.8 gives
                LMC-like circumstellar dust (G08 fit to Weingartner & Draine 2001).

    OPT=214=> use Maiz Apellaniz et al. 2014 (A&A 564, A63) CCM-like curve.
                Only valid above 0.3 microns. Tested against Gordon 2024 version.

    OPT=216 => use Gordon et al. 2016 (ApJ 826, 104) as implemented by S. Thorp.
                This is a two parameter model, controlled by RVA and FA.
                RVA is read from PARLIST[0];
                FA is read from PARLIST[1].
                RV argument is ignored.
                Aborts if these are not present or within the valid ranges.
                The final curve is a mixture of Fitzpatrick 1999 and
                Gordon et al. 2003 (ApJ 594, 279), where the latter is SMC bar-like
                dust with RV=2.74 and no UV bump. FA=1 reverts to Fitzpatrick 1999 
                with RV=RVA. FA=0 gives Gordon et al. 2003 with RV=2.74. 
                Effective RV = 1/[FA/RV + (1-FA)/2.74].
                Tested against Gordon 2024 implementation.

    OPT=223 => use Gordon et al. 2023 (ApJ 950, 86) as implemented by S. Thorp.
                This is a full UV-OPT-IR extinction law parameterized by RV.
                Defined by a combination of Fitzpatrick & Massa 1990 (ApJS 72, 163)
                in the UV plus various other functions composed together at
                other wavelengths. Tested against Gordon 2024 (JOSS 9, 7023).

    OPT=225 => use Sommovigo et al. 2025 (arXiv:2502.13240) as implemented by S. Thorp.
                This is a one-parameter extinction law based on a 4-parameter Pei-like
                functional form, and some scaling relations for the coefficients
                (c1, c2, c3, c4) as a function of AV. RV is ignored as the shape is
                entirely set by AV. See Eq. 2, 7, 8, 9 in the Sommovigo paper. 
                Based on fits to simulations by the Learning the Universe collaboration.

   PARLIST => optional set of double-precision parameters to refine calculations
              Number of PARLIST params and their meaning depend on OPT.
              OPT=208 : PARLIST[0]=P, PARLIST[1]=A;
              OPT=216 : PARLIST[0]=RVA, PARLIST[1]=FA;

  Returns magnitudes of extinction.

 Nov 1, 2006: Add option to use new/old NIR coefficients
              (copied from Jha's MLCS code)

;     c1 = [ 1. , 0.17699, -0.50447, -0.02427,  0.72085,    $ ;Original
;                 0.01979, -0.77530,  0.32999 ]               ;coefficients
;     c2 = [ 0.,  1.41338,  2.28305,  1.07233, -5.38434,    $ ;from CCM89
;                -0.62251,  5.30260, -2.09002 ]

      c1 = [ 1. , 0.104,   -0.609,    0.701,  1.137,    $    ;New coefficients
                 -1.718,   -0.827,    1.647, -0.505 ]        ;from O'Donnell
      c2 = [ 0.,  1.952,    2.908,   -3.989, -7.985,    $    ;(1994)
                 11.102,    5.491,  -10.805,  3.347 ]

  Aug 4 2019 RK
   + fix subtle bug by returning XT=0 only if AV=0, and not if AV<1E-9.
     Recall that negative AV are used for warping spectra in kcor.c.
     This bug caused all AV<0 to have same mag as AV=0.

  Sep 19 2024 ST
   + add an exact Fitzpatrick 99 implementation with opt=9999.

  Sep 25 2024 S.Thorp
   + Exact F'99 spline implementation promoted to opt=99
   - Old F'99 based on F'99/O'94 ratio deprecated to opt=-99

  Oct 19 2024 S. Thorp
   + Begun adding more dust laws

  Oct 24 2024 R.Kessler
   + pass PARLIST based on sim-input key PARLIST_MWCOLORLAW: p0,p1,p2,...
     PARLIST is not used yet, but is available for future development.

  Oct 26 2024 S. Thorp
   + use the new PARLIST for Gordon '16 dust law
   + add Goobar '08 circumstellar dust law
   + add Maiz Apellaniz '14

  Feb 26 2025 S. Thorp
   + add Sommovigo '25
   + add 4-parameter Pei '92 curve (Li '08)
 ***/

  int i, DO94  ;
  double XT, x, y, a, b, fa, fb, xpow, xx, xx2, xx3 ;
  double y2, y3, y4, y5, y6, y7, y8 ;


  char fnam[60];  concat_callfun_plus_fnam(callFun, "GALextinct", fnam); // return fnam

  // ------------------- BEGIN --------------

  XT = 0.0 ;

  if ( AV == 0.0  )  {  return XT ; }

  // -----------------------------------------
  // if selecting non-CCM89-like option,
  // bypass everything else and call S. Thorp's functions

  //  printf(" xxx %s: PARLIST = %f %f %f \n", PARLIST[0], PARLIST[1], PARLIST[2] ); fflush(stdout);
  
  if ( OPT == OPT_MWCOLORLAW_FITZ99_EXACT || OPT == OPT_MWCOLORLAW_FITZ04 || OPT == OPT_MWCOLORLAW_GORD03 )  {
    XT = GALextinct_Fitz99_exact(RV, AV, WAVE, OPT, callFun);
    return XT ;
  } else if ( OPT == OPT_MWCOLORLAW_GOOB08 ) {
      double WAVE0 = 5495.0; // reference V-band wavelength
      double P = PARLIST[0]; //extract power law index from PARLIST
      double A = PARLIST[1]; //extract prefactor from PARLIST
      // try to catch missing arguments
      if ( PARLIST[0] == -99.0 || PARLIST[1] == -99.0 ) {
          sprintf(c1err,"Found suspicious inputs: PARLIST[0]=%.1f and PARLIST[1]=%.1f",
                  PARLIST[0], PARLIST[1]);
          sprintf(c2err,"Goobar (2008) requires two values in PARLIST_MWCOLORLAW: P,A.");
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
      // check parameter ranges
      if ( P > PMAX_GOOB08 || P < PMIN_GOOB08 ){
          sprintf(c1err,"Read invalid P=%.1f from PARLIST_MWCOLORLAW!", P);
          sprintf(c2err,"Goobar (2008) only recommended for %.1f<=P<=%.1f.",
                  PMIN_GOOB08, PMAX_GOOB08);
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
      if ( A > 1.0 || A <= 0.0 ){
          sprintf(c1err,"Read invalid A=%.1f from PARLIST_MWCOLORLAW!", A);
          sprintf(c2err,"Goobar (2008) only valid for 0.0<A<=1.0.");
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
      // check wavelength range
      if ( WAVE < WAVEMIN_GOOB08 || WAVE > WAVEMAX_GOOB08 ) {
          sprintf(c1err,"WAVE=%.1f out of range for Goobar (2008)", WAVE);
          sprintf(c2err,"Recommended limits are %.1f<=WAVE<=%.1f.", 
                  WAVEMIN_GOOB08, WAVEMAX_GOOB08);
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }

      // power law (eq. 3 in G08)
      XT = 1.0 - A + A*pow(WAVE/WAVE0, P);
      return AV*XT;
  } else if ( OPT == OPT_MWCOLORLAW_MAIZ14 ) {
    XT = GALextinct_Maiz14(RV, AV, WAVE, callFun);
    return XT;
  } else if ( OPT == OPT_MWCOLORLAW_GORD16 ) {
      double XTA, XTB;
      double RVB = 2.74; // R,K. -- ensure double cast for this param
      double RVA = PARLIST[0]; // extract RVA from PARLIST
      double FA  = PARLIST[1]; // extract FA from PARLIST
      // sanity check arguments from PARLIST
      // try to catch missing arguments
      if ( PARLIST[0] == -99.0 || PARLIST[1] == -99.0 ) {
          sprintf(c1err,"Found suspicious inputs: PARLIST[0]=%.1f and PARLIST[1]=%.1f",
                  PARLIST[0], PARLIST[1]);
          sprintf(c2err,"Gordon et al. (2016) requires two values in PARLIST_MWCOLORLAW: RVA,FA.");
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
      // check parameter ranges
      if ( RVA > RVMAX_FITZ99 || RVA < RVMIN_FITZ99 ){
          sprintf(c1err,"Read invalid RVA=%.1f from PARLIST_MWCOLORLAW!", RVA);
          sprintf(c2err,"Gordon et al. (2016) only valid for %.1f<=RVA<=%.1f.",
                  RVMIN_FITZ99, RVMAX_FITZ99);
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }
      if ( FA > 1.0 || FA < 0.0 ){
          sprintf(c1err,"Read invalid FA=%.1f from PARLIST_MWCOLORLAW!", FA);
          sprintf(c2err,"Gordon et al. (2016) only valid for 0.0<=FA<=1.0.");
          errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
      }

      XTA = GALextinct_Fitz99_exact(RVA, AV, WAVE, OPT_MWCOLORLAW_FITZ99_EXACT, callFun);
      XTB = GALextinct_Fitz99_exact(RVB, AV, WAVE, OPT_MWCOLORLAW_GORD03,       callFun);

      return FA*XTA + (1-FA)*XTB ;
      
  } else if ( abs(OPT) == OPT_MWCOLORLAW_FITZ19_CUBIC ) {
    XT = GALextinct_Fitz19(RV, AV, WAVE, (OPT>0) ? 1 : 0,  callFun);
    return XT;
  } else if ( OPT == OPT_MWCOLORLAW_GORD23 ) {
    XT = GALextinct_Gord23(RV, AV, WAVE, callFun);
    return XT;
  } else if ( OPT == OPT_MWCOLORLAW_SOMM25 ) {
    XT = GALextinct_Somm25(AV, WAVE, callFun);
    return XT;
  }
  
  // -----------------------------------------
  DO94 = (OPT == OPT_MWCOLORLAW_ODON94 ||
	  OPT == OPT_MWCOLORLAW_FITZ99_APPROX ) ;

  x = 10000./WAVE;    // inverse wavelength in microns
  y = x - 1.82;

  if (x >= 0.3 && x < 1.1) {           // IR
    xpow = pow(x,1.61) ;
    a =  0.574 * xpow ;
    b = -0.527 * xpow ;
  } 
  else if (x >= 1.1 && x < 3.3) {    // Optical/NIR

    y2 = y  * y ;
    y3 = y2 * y ;
    y4 = y2 * y2 ;
    y5 = y3 * y2;
    y6 = y3 * y3;
    y7 = y4 * y3;
    y8 = y4 * y4;

    if ( DO94 ) {
    a = 1. + 0.104*y - 0.609*y2 + 0.701*y3 + 1.137*y4
      - 1.718*y5 - 0.827*y6 + 1.647*y7 -0.505*y8 ;

    b = 1.952*y + 2.908*y2 -3.989*y3 - 7.985*y4
      + 11.102*y5 + 5.491*y6 - 10.805*y7 + 3.347*y8;
    }
    else {
    a = 1. + 0.17699*y - 0.50447*y2 - 0.02427*y3
      + 0.72085*y4 + 0.01979*y5 - 0.77530*y6 + 0.32999*y7 ;

    b = 1.41338*y + 2.28305*y2 + 1.07233*y3 - 5.38434*y4
      - 0.62251*y5 + 5.30260*y6 - 2.09002*y7 ;
    }

  } 
  else if (x >= 3.3 && x < 8.0 ) {    // UV
    if (x >= 5.9) {
      xx  = x - 5.9 ;
      xx2 = xx  * xx ;
      xx3 = xx2 * xx ;

      fa = -0.04473*xx2 - 0.009779*xx3 ;
      fb =  0.21300*xx2 + 0.120700*xx3 ;

    } else {
      fa = fb = 0.0;
    }

    xx = x - 4.67 ; xx2 = (xx*xx);
    a =  1.752 - 0.316*x - 0.104/(xx2 + 0.341) + fa;

    xx = x - 4.62 ; xx2 = (xx*xx);
    b = -3.090 + 1.825*x + 1.206/(xx2 + 0.263) + fb;
  } 
  else if (x >= 8.0 && x <= 10.0) {  // Far-UV
    xx  = x - 8.0  ;
    xx2 = xx  * xx ;
    xx3 = xx2 * xx ; 

    a = -1.073 - 0.628*xx + 0.137*xx2 - 0.070*xx3 ;
    b = 13.670 + 4.257*xx - 0.420*xx2 + 0.374*xx3 ;
  } else {
    a = b = 0.0;
  }

  XT = AV*(a + b/RV);

  // Sep 18 2013 RK/DS - Check option for Fitzptrack 99

#define NPOLY_FITZ99 11 //Dillon and Dan upped to 10, Oct 9 2021
  if ( OPT == OPT_MWCOLORLAW_FITZ99_APPROX ) {  

    double XTcor, wpow[NPOLY_FITZ99] ;    
    double F99_over_O94[NPOLY_FITZ99] = {  // Dillon and Dan, Oct 9 2021
      8.55929205e-02,  1.91547833e+00, -1.65101945e+00,  7.50611119e-01,
      -2.00041118e-01,  3.30155576e-02, -3.46344458e-03,  2.30741420e-04,
      -9.43018242e-06,  2.14917977e-07, -2.08276810e-09
    };

    if ( WAVE > WAVEMAX_FITZ99  ) {
      sprintf(c1err,"Invalid WAVE=%.1f A for Fitzpatrick 99 color law.",
	      WAVE );
      sprintf(c2err,"Avoid NIR (>%.1f), or update Fitz99 in NIR",
	      WAVEMAX_FITZ99 );
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // compute powers of wavelength without using slow 'pow' function
    wpow[0]  = 1.0 ;
    wpow[1]  = WAVE/1000. ;
    wpow[2]  = wpow[1] * wpow[1] ;
    wpow[3]  = wpow[1] * wpow[2] ;
    wpow[4]  = wpow[2] * wpow[2] ;
    wpow[5]  = wpow[3] * wpow[2] ;
    wpow[6]  = wpow[3] * wpow[3] ;
    wpow[7]  = wpow[4] * wpow[3] ;
    wpow[8]  = wpow[4] * wpow[4] ;
    wpow[9]  = wpow[5] * wpow[4] ;
    wpow[10]  = wpow[5] * wpow[5] ;

    XTcor = 0.0 ;
    for(i=0; i < NPOLY_FITZ99; i++ ) 
      {  XTcor += (wpow[i] * F99_over_O94[i]) ; }
    
    XT *= XTcor ;
  }

  return XT ;

}  // end of GALextinct


// ============= EXACT F99 EXTINCTION LAW ==============
double GALextinct_Fitz99_exact(double RV, double AV, double WAVE, int OPT, char *callFun) {
/*** 
  Created by S. Thorp, Sep 19 2024

  Default Fitzpatrick (1999) implementation since Sep 25 2024

  Also used to compute Fitzpatrick (2004), Gordon et al. (2003),
  and Gordon et al. (2016) laws.

  Input : 
    AV   = V band (defined to be at 5495 Angstroms) extinction
    RV   = assumed A(V)/E(B-V) (e.g., = 3.1 in the LMC)
    WAVE = wavelength in angstroms
    OPT  = Option from (99, 203, 204, 216)
Returns :
    XT = magnitudes of extinction
***/

  char fnam[60];
  concat_callfun_plus_fnam(callFun, "GALextinct_Fitz99_exact", fnam); // return fnam

  //Check RV=2.74 for Gordon et al. (2003)
  if ( OPT == OPT_MWCOLORLAW_GORD03 && RV != 2.74 ) {
    sprintf(c1err,"Requested OPT=%d and RV=%.2f", OPT, RV);
    sprintf(c2err,"Gordon et al. 2003 only valid for RV=2.74");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }
  //Check wavelengths in valid range
  if ( WAVE < WAVEMIN_FITZ99_EXACT || WAVE > WAVEMAX_FITZ99_EXACT ) {
    sprintf(c1err,"Requested WAVE=%.3f Angstroms", WAVE);
    sprintf(c2err,"F99-like curves only valid in [%.1f, %.1f]A",
	    WAVEMIN_FITZ99_EXACT, WAVEMAX_FITZ99_EXACT);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }
  
  //number of knots
  int Nk;
  // constants
  double x02, gamma2, c1, c2, c3, c4, c5;
  // target wavenumber in inverse microns
  double x = 10000.0/WAVE;
  // spline result
  double y;

  // constants
  c2 = -0.824 + 4.717/RV;
  c5 = 5.90;
  if ( OPT == OPT_MWCOLORLAW_FITZ99_EXACT ) {
    x02 = 21.123216; // 4.596*4.596
    gamma2 = 0.9801; // 0.99*0.99
    c1 = 2.03 - 3.007*c2;
    c3 = 3.23;
    c4 = 0.41;
    Nk = 9;
  } else if ( OPT == OPT_MWCOLORLAW_FITZ04 ) {
    x02 = 21.086464; // 4.592*4.592
    gamma2 = 0.850084; // 0.922*0.922
    c1 = 2.18 - 2.91*c2;
    c3 = 2.991;
    c4 = 0.319;
    Nk = 10;
  } else if ( OPT == OPT_MWCOLORLAW_GORD03 ) {
    x02 = 21.16; // 4.6*4.6
    gamma2 = 1.0;
    c1 = -4.959;
    c2 = 2.264;
    c3 = 0.389;
    c4 = 0.461;
    Nk = 11;
  } else {
    sprintf(c1err,"Requested OPT=%d", OPT);
    sprintf(c2err,"Only 99, 203, 204 are implemented!");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  if (WAVE <= 2700.0) { //FM90 curve in UV
    y = GALextinct_FM90(x, c1, c2, c3, c4, c5, x02, gamma2);
    return AV * (1.0 + y/RV); 
  } else { //spline for optical/IR
    // powers of RV
    double RV2, RV3, RV4;
    
    // spline knot locations in inverse microns
    double xF[Nk];
    xF[0] = 0.0; // always put an anchor at 1/lambda = 0
    if ( OPT == OPT_MWCOLORLAW_GORD03 ) {
      xF[1] = 1.0/2.198;
      xF[2] = 1.0/1.65;
      xF[3] = 1.0/1.25;
      xF[4] = 1.0/0.81;
      xF[5] = 1.0/0.65;
      xF[6] = 1.0/0.55;
      xF[7] = 1.0/0.44;
      xF[8] = 1.0/0.37;
    } else {
      if ( OPT == OPT_MWCOLORLAW_FITZ04 ) {
	// Use FM07 knots for Fitzpatrick (2004) curve
	xF[1] = 0.5;
	xF[2] = 0.75;
	xF[3] = 1.0;
      } else {
	xF[1] = 1.0/2.65;
	xF[2] = 1.0/1.22;
      }
      xF[Nk-6] = 1.0/0.60;
      xF[Nk-5] = 1.0/0.547;
      xF[Nk-4] = 1.0/0.467; 
      xF[Nk-3] = 1.0/0.411;
    }
    // always anchor in the UV
    xF[Nk-2] = 1.0/0.270;
    xF[Nk-1] = 1.0/0.260;
    // spline knot values
    double yF[Nk];

    // RV-dependent spline knot values
    // polynomial coeffs match FM_UNRED.pro and extinction.py
    // NOTE: the optical coefficients differ from Gordon 24 implementation
    double yFNIR;
    yF[0] = -RV;
    if ( OPT == OPT_MWCOLORLAW_GORD03 ) {
      // knot values have 1 subtracted and are multiplied by RV
      yF[1] = -2.4386; //0.11*RV-RV
      yF[2] = -2.27694; //0.169*RV-RV
      yF[3] = -2.055; //0.25*RV-RV
      yF[4] = -1.18642; //0.567*RV-RV
      yF[5] = -0.54526; //0.801*RV-RV
      yF[6] = 0.0;
      yF[7] = 1.02476; //1.374*RV-RV 
      yF[8] = 1.84128; //1.672*RV-RV
    } else {
      // powers of RV
      RV2 = RV*RV;
      RV3 = RV2*RV;
      RV4 = RV2*RV2;
      if ( OPT == OPT_MWCOLORLAW_FITZ04 ) {
	yFNIR = (0.63*RV -0.84);
	yF[1] = yFNIR*pow(xF[1], 1.84) - RV;
	yF[2] = yFNIR*pow(xF[2], 1.84) - RV;
	yF[3] = yFNIR*pow(xF[3], 1.84) - RV;
      }
      else {
	yF[1] = -0.914616129*RV; // 0.26469*(RV/3.1) - RV
	yF[2] = -0.7325*RV; // 0.82925*(RV/3.1) - RV
      }
      yF[Nk-6] = -0.422809 + 0.00270*RV +  2.13572e-04*RV2;
      yF[Nk-5] = -5.13540e-02 + 0.00216*RV - 7.35778e-05*RV2;
      yF[Nk-4] =  7.00127e-01 + 0.00184*RV - 3.32598e-05*RV2;
      yF[Nk-3] =  1.19456 + 0.01707*RV - 5.46959e-03*RV2 +  
	7.97809e-04*RV3 - 4.45636e-05*RV4;
    }
    // UV knots using FM90
    yF[Nk-2] = GALextinct_FM90(xF[Nk-2], c1, c2, c3, c4, c5, x02, gamma2);
    yF[Nk-1] = GALextinct_FM90(xF[Nk-1], c1, c2, c3, c4, c5, x02, gamma2);
    
    y = GALextinct_FM_spline(x, Nk, xF, yF, 0);
    
    return AV*(1.0 + y/RV);
  }

} // end of GALextinct_Fitz99_exact

// ============= MAIZ APELLANIZ ET AL. 2014 EXTINCTION LAW ==============
double GALextinct_Maiz14(double RV, double AV, double WAVE, char *callFun) {
/*** 
  Created by S. Thorp, Oct 26 2024

  Input : 
    AV    = V band (defined to be at 5495 Angstroms) extinction
    RV    = assumed A(V)/E(B-V) (e.g., = 3.1 in the LMC)
    WAVE  = wavelength in angstroms
  Returns :
    XT = magnitudes of extinction
***/

    char fnam[60] ;
    concat_callfun_plus_fnam(callFun, "GALextinct_Maiz14", fnam); // return fnam

    // Abort if out of bounds
    if ( WAVE > WAVEMAX_MAIZ14 || WAVE < WAVEMIN_MAIZ14 ) {
      sprintf(c1err,"Requested WAVE=%.3f Angstroms", WAVE);
      sprintf(c2err,"Maiz Apellaniz et al. 2014 only valid from %.0f-%.0f Angstroms",
              WAVEMIN_MAIZ14, WAVEMAX_MAIZ14);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // target wavelength in inverse microns
    double x = 10000.0/WAVE;
    // curve at WAVE
    double y;

    // terms we'll compute
    double a, b; //a and b curves at x

    // evaluate the a and b curves
    if (x < 1.0) { // just do the IR power law above 1 micron
        a =  0.574 * pow(x, 1.61);
        b = -0.527 * pow(x, 1.61);
    } else { // do the spline
        // all knots are independent of RV
        // coefficients extracted from Gordon's SciPy spline implementation
        double xk[11] = { 1.0, 1.15, 1.81984, 2.1, 2.27015, 2.7, 3.5,
            3.9, 4.0, 4.1, 4.2 }; // knot positions
        double a3[10] = { -3.09348541,  2.28902153e-1,  5.41605406e-1,
            -6.37404842e-1,  3.52950213e-1, -5.91231605e-2,
            -5.56727269,  48.1384135, -11.6556097, -12.6892172 }; //x^3
        double a2[10] = { 5.57088021e-1, -8.34980412e-1, -3.74996957e-1,
            8.02115549e-2, -2.45151747e-1,  2.09995201e-1,  6.80996157e-2,
            -6.61262761, 7.82889643,  4.33221353 };  //x^2
        double a1[10] = { 9.24140000e-1,  8.82456141e-1,  7.19649009e-2,
            -1.06221772e-2, -3.86867508e-2, -5.37987921e-2, 1.68677061e-1,
            -2.44913414, -2.32750725, -1.11139626 }; //x^1
        double a0[10] = { 5.74000000e-1,  7.14714967e-1,  9.99971669e-1,
            1.00260970,  9.99984676e-1,  9.66090893e-1, 1.02717773,  
            7.49239041e-1,  4.86337764e-1, 3.20220393e-1 }; //x^0
        double b3[10] = { 6.11543973, -4.71924979e-1, -3.75700076,
            3.30710701, -6.80610047e-1,  4.81511488e-1, 17.8352808,
            -124.325934,  12.0120271, 48.1516935 }; //x^3
        double b2[10] = { -2.49479124e-1,  2.50246875,  1.55412607,
            -1.60355793,  8.45548471e-2, -7.93125839e-1, 3.62501733e-1,
            21.7648387, -15.5329415, -11.9293334 }; //x^2
        double b1[10] = { -8.48470000e-1, -5.10521556e-1,  2.20674792,
            2.19289909,  1.93444072,  1.62986148, 1.28536219,
            10.1362984,  10.7594881, 8.01326059 }; //x^1
        double b0[10] = { -5.27000000e-1, -6.39244171e-1, -2.26082358e-4,
            6.57384043e-1,  1.00037205,  1.79345802, 2.83628055,  
            4.54988367,  5.65683596, 6.58946738 };
       

        // find index in knot list
        int q = 0; // qmin = 0; qmax = 9
        while (q < 10) {
            if (x < xk[q+1]) { 
                break; 
            } else {
                q++;
            }
        }

        //powers of x
        double x1 = x - xk[q];
        double x2 = x1*x1;
        double x3 = x2*x1;
        
        // interpolate
        a = a3[q]*x3 + a2[q]*x2 + a1[q]*x1 + a0[q];
        b = b3[q]*x3 + b2[q]*x2 + b1[q]*x1 + b0[q];

    }
    return AV * (a + b/RV);

} // end of GALextinct_Maiz14

// ============= FITZPATRICK ET AL. 2019 EXTINCTION LAW ==============
double GALextinct_Fitz19(double RV, double AV, double WAVE, int CUBIC, char *callFun) {
/*** 
  Created by S. Thorp, Oct 20 2024

  Input : 
    AV    = V band (defined to be at 5495 Angstroms) extinction
    RV    = assumed A(V)/E(B-V) (e.g., = 3.1 in the LMC)
    WAVE  = wavelength in angstroms
    CUBIC = if 1, uses cubic interpolation; else linear interpolation
  Returns :
    XT = magnitudes of extinction
***/

    char fnam[60] ;
    concat_callfun_plus_fnam(callFun, "GALextinct_Fitz19", fnam); // return fnam

    // Abort if out of bounds
    if ( WAVE > WAVEMAX_FITZ19 || WAVE < WAVEMIN_FITZ19 ) {
      sprintf(c1err,"Requested WAVE=%.3f Angstroms", WAVE);
      sprintf(c2err,"Fitzpatrick et al. 2019 only valid from %.0f-%.0f Angstroms",
              WAVEMIN_FITZ19, WAVEMAX_FITZ19);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // target wavelength in inverse microns
    double x = 10000.0/WAVE;
    // curve at WAVE
    double y;

    // number of tabulated values
    int Nk = 102;

    //tabulated values of E(x-V)/E(B-V)
    //based on dust_extinction / Table 3 in Fitzpatrick+19
    double xk[102] = { 0.000, 0.455, 0.606, 0.800, 1.000, 1.100, 1.200, 1.250,
        1.300, 1.350, 1.400, 1.450, 1.500, 1.550, 1.600, 1.650, 1.700, 1.750,
        1.800, 1.818, 1.850, 1.900, 1.950, 2.000, 2.050, 2.100, 2.150, 2.200,
        2.250, 2.273, 2.300, 2.350, 2.400, 2.450, 2.500, 2.550, 2.600, 2.650,
        2.700, 2.750, 2.800, 2.850, 2.900, 2.950, 3.000, 3.100, 3.200, 3.300,
        3.400, 3.500, 3.600, 3.700, 3.800, 3.900, 4.000, 4.100, 4.200, 4.300,
        4.400, 4.500, 4.600, 4.700, 4.800, 4.900, 5.000, 5.100, 5.200, 5.300,
        5.400, 5.500, 5.600, 5.700, 5.800, 5.900, 6.000, 6.100, 6.200, 6.300,
        6.400, 6.500, 6.600, 6.700, 6.800, 6.900, 7.000, 7.100, 7.200, 7.300,
        7.400, 7.500, 7.600, 7.700, 7.800, 7.900, 8.000, 8.100, 8.200, 8.300,
        8.400, 8.500, 8.600, 8.700 }; //knot positions in inverse microns
    double k302k[102] = { -3.020, -2.747, -2.528, -2.222, -1.757, -1.567, -1.300,
        -1.216, -1.070, -0.973, -0.868, -0.750, -0.629, -0.509, -0.407, -0.320,
        -0.221, -0.133, -0.048, 0.000, 0.071, 0.188, 0.319, 0.438, 0.575, 0.665,
        0.744, 0.838, 0.951, 1.000, 1.044, 1.113, 1.181, 1.269, 1.346, 1.405,
        1.476, 1.558, 1.632, 1.723, 1.791, 1.869, 1.948, 2.009, 2.090, 2.253,
        2.408, 2.565, 2.746, 2.933, 3.124, 3.328, 3.550, 3.815, 4.139, 4.534,
        5.012, 5.560, 6.118, 6.565, 6.767, 6.681, 6.394, 6.038, 5.704, 5.432,
        5.226, 5.078, 4.978, 4.913, 4.877, 4.862, 4.864, 4.879, 4.904, 4.938,
        4.982, 5.038, 5.105, 5.181, 5.266, 5.359, 5.460, 5.569, 5.684, 5.805,
        5.933, 6.067, 6.207, 6.352, 6.502, 6.657, 6.817, 6.981, 7.150, 7.323,
        7.500, 7.681, 7.866, 8.054, 8.246, 8.441 }; //k function [R(5500)=3.02]
    double sk[102] = { -1.000, -0.842, -0.728, -0.531, -0.360, -0.284, -0.223,
        -0.198, -0.173, -0.150, -0.130, -0.110, -0.096, -0.081, -0.063, -0.048,
        -0.032, -0.017, -0.005, 0.000, 0.007, 0.013, 0.012, 0.010, 0.004, 0.003,
        0.000, 0.002, 0.001, 0.000, -0.000, 0.001, 0.001, -0.002, 0.000, -0.002,
        -0.002, -0.006, -0.009, -0.011, -0.017, -0.025, -0.029, -0.037, -0.043,
        -0.064, -0.092, -0.122, -0.161, -0.201, -0.249, -0.303, -0.366, -0.437,
        -0.517, -0.603, -0.692, -0.774, -0.843, -0.888, -0.908, -0.903, -0.880,
        -0.849, -0.816, -0.785, -0.760, -0.741, -0.729, -0.722, -0.722, -0.726,
        -0.734, -0.745, -0.760, -0.778, -0.798, -0.820, -0.845, -0.870, -0.898,
        -0.926, -0.956, -0.988, -1.020, -1.053, -1.087, -1.122, -1.158, -1.195,
        -1.232, -1.270, -1.309, -1.349, -1.389, -1.429, -1.471, -1.513, -1.555,
        -1.598, -1.641, -1.685 }; //s function
                                  
    double kRVk[102];
    for (int i=0; i<Nk; i++) { kRVk[i] = k302k[i] + sk[i]*(RV-3.10)*0.99; }

    y = GALextinct_FM_spline(x, Nk, xk, kRVk, CUBIC ? 0 : 1);

    return AV*(1.0 + y/RV);

} //end of GALextinct_Fitz19

// ============= GORDON ET AL. 2023 EXTINCTION LAW ==============
double GALextinct_Gord23(double RV, double AV, double WAVE, char *callFun) {
/*** 
  Created by S. Thorp, Oct 20 2024

  Input : 
    AV   = V band (defined to be at 5495 Angstroms) extinction
    RV   = assumed A(V)/E(B-V) (e.g., = 3.1 in the LMC)
    WAVE = wavelength in angstroms
  Returns :
    XT = magnitudes of extinction
***/

    char fnam[60] ;
    concat_callfun_plus_fnam(callFun, "GALextinct_Gord23", fnam); // return fnam

    // target wavelength in inverse microns
    double x = 10000.0/WAVE;
    
    double x2, x3, x4; // powers of x
    x2 = x*x;
    x3 = x2*x;
    x4 = x2*x2;

    // variables for a and b part of curve
    // w = weighting function in overlap regions
    double a, b, w, f;
    a = 0.0;
    b = 0.0;

    // terms for the optical part
    double x01, x02, x03, FW1, FW2; //Drude params
    double FX1, FX2, FX3, XX1, XX2, XX3, D1, D2, D3; //derived terms

    // constants for the N-MIR part
    double scale=0.38526, alpha=1.68467, alpha2=0.78791, swave=4.30578, swidth=4.78338,
        sil1_amp=0.06652, sil1_center=9.8434, sil1_fwhm=2.21205, sil1_asym=-0.24703,
        sil2_amp=0.0267, sil2_center=19.58294, sil2_fwhm=17.0, sil2_asym=-0.27;
    double mwave = WAVE / 10000.0; //wavelength in microns
    double fweight, pweight, ratio; //power law transition
    double sil1_gamma, sil2_gamma, sil1_gx2, sil2_gx2, sil1_xx, sil2_xx; //Si drude params

    // Abort if out of bounds
    if ( WAVE > WAVEMAX_GORD23 || WAVE < WAVEMIN_GORD23 ) {
      sprintf(c1err,"Requested WAVE=%.3f Angstroms; X=%.3f inv. microns", WAVE, x);
      sprintf(c2err,"Gordon et al. 2023 only valid from %.0f-%.0f Angstroms",
              WAVEMIN_GORD23, WAVEMAX_GORD23);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // UV / OPT / NIR
    if ( 1.0/0.33 <= x && x <= 1.0/0.09 ) { //UV (including UV-OPT overlap)
        // weighting function
        if ( x > 1.0/0.30 ) {
            w = 1.0;
        } else {
            // Gordon "smoothstep" function
            f = (mwave - 0.3)/0.03;
            w = 3.0 - 2.0*f;
            w = 1.0 - w*f*f;
        }
        // a component: 21.16 = 4.6*4.6; 0.9801 = 0.99*0.99
        a += w*GALextinct_FM90(x, 0.81297, 0.2775, 1.06295, 0.11303, 5.90, 21.16, 0.9801);
        // b component 
        b += w*GALextinct_FM90(x, -2.97868, 1.89808, 3.10334, 0.65484, 5.90, 21.16, 0.9801);
    }
    if ( 1.0/1.1 <= x && x < 1.0/0.3 ) { //OPT (including both overlaps)
        // weighting function
        if ( 1.0/0.9 < x && x < 1.0/0.33 ) { //internal
            w = 1.0;
        } else if ( x >= 1.0/0.33 ) { //overlap with UV
            // Gordon "smoothstep" function
            f = (mwave - 0.3)/0.03;
            w = 3.0 - 2.0*f;
            w = w*f*f;
        } else if ( x <= 1.0/0.9 ) { //overlap with IR
            // Gordon "smoothstep" function
            f = (mwave - 0.9)/0.2;
            w = 3.0 - 2.0*f;
            w = 1.0 - w*f*f;
        }

        // polynomial terms
        a += w*(-0.35848 + 0.7122*x + 0.08746*x2 - 0.05403*x3 + 0.00674*x4);
        b += w*(0.12354 - 2.68335*x + 2.01901*x2 - 0.39299*x3 + 0.03355*x4);

        //the Drude abides
        // shared terms
        x01 = 2.288;
        x02 = 2.054;
        x03 = 1.587;
        FW1 = 0.243; //FW3 = FW1
        FW2 = 0.179;
        FX1 = (FW1*FW1)/(x01*x01);
        FX2 = (FW2*FW2)/(x02*x02);
        FX3 = (FW1*FW1)/(x03*x03);
        XX1 = (x/x01 - x01/x);
        XX2 = (x/x02 - x02/x);
        XX3 = (x/x03 - x03/x);
        D1 = FX1 / (XX1*XX1 + FX1);
        D2 = FX2 / (XX2*XX2 + FX2);
        D3 = FX3 / (XX3*XX3 + FX3);
        // add contributions to a and b curves
        a += w*(0.03893*D1 + 0.02965*D2 + 0.01747*D3);
        b += w*(0.18453*D1 + 0.19728*D2 + 0.1713*D3);

    }
    if ( 1.0/35.0 <= x && x < 1.0/0.9 ) { //IR (including OPT-IR overlap)
        // weighting function
        if ( x < 1.0/1.1 ) {
            w = 1.0;
        } else {
            // Gordon "smoothstep" function
            f = (mwave - 0.9)/0.2;
            w = 3.0 - 2.0*f;
            w = w*f*f;
        }
        // a curve Gordon21 double power law
        // Gordon smoothstep
        fweight = (mwave - (swave - 0.5*swidth))/swidth;
        if (fweight < 0) {
            pweight = 0.0;
        } else if (fweight > 1) {
            pweight = 1.0;
        } else {
            pweight = (3.0 - 2.0*fweight)*fweight*fweight;
        }
        // ratio
        ratio = pow(swave, -alpha)/pow(swave, -alpha2);
        // power law 1
        a += w * scale * (1.0 - pweight) * pow(mwave, -alpha);
        // power law 2
        a += w * scale * ratio * pweight * pow(mwave, -alpha2);
        // silicate features
        sil1_gamma = 2.0 * sil1_fwhm / (1.0 + exp(sil1_asym*(mwave - sil1_center)));
        sil2_gamma = 2.0 * sil2_fwhm / (1.0 + exp(sil2_asym*(mwave - sil2_center)));
        sil1_gx2 = sil1_gamma*sil1_gamma/(sil1_center*sil1_center);
        sil2_gx2 = sil2_gamma*sil2_gamma/(sil2_center*sil2_center);
        sil1_xx = (mwave/sil1_center) - (sil1_center/mwave);
        sil2_xx = (mwave/sil2_center) - (sil2_center/mwave);
        a += w * sil1_amp * sil1_gx2 / (sil1_xx*sil1_xx + sil1_gx2);
        a += w * sil2_amp * sil2_gx2 / (sil2_xx*sil2_xx + sil2_gx2);

        // b curve power law
        b+= -1.01251 * w * pow(x, 1.06099);
    } 

    return AV * ( a + b*((1.0/RV) - (1.0/3.1)) );

} // end of GALextinct_Gord23

// ============= SOMMOVIGO ET AL. 2025 =======================
double GALextinct_Somm25(double AV, double WAVE, char *callFun) {
/*** 
  Created by S. Thorp, Feb 26 2025

  Input : 
    AV   = V band (defined to be at 5495 Angstroms) extinction
    WAVE = wavelength in angstroms
  Returns :
    XT = magnitudes of extinction
***/

    char fnam[60] ;
    concat_callfun_plus_fnam(callFun, "GALextinct_Somm25", fnam); // return fnam
    
    // target wavelength in inverse microns
    double x = 10000.0/WAVE;
    
    // Abort if out of bounds
    if ( WAVE > WAVEMAX_SOMM25 || WAVE < WAVEMIN_SOMM25 ) {
      sprintf(c1err,"Requested WAVE=%.3f Angstroms; X=%.3f inv. microns", WAVE, x);
      sprintf(c2err,"Sommovigo et al. 2025 only valid from %.0f-%.0f Angstroms",
              WAVEMIN_SOMM25, WAVEMAX_SOMM25);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // coefficients as a function of AV
    double c1, c2, c3, c4;
    double logc1, logc4, logAV;
    logAV = log10(AV);
    logc1 = -0.37*logAV + 0.75; // Eq. 7
       c1 = pow(10.0, logc1);
       c2 =  1.88;              // median for TNG galaxies
       c3 =  1.21*logc1 - 1.33; // Eq. 8
    logc4 = -0.59*logAV - 1.42; // Eq. 9
       c4 = pow(10.0, logc4);
    
    return AV*GALextinct_Pei4(x, c1, c2, c3, c4);

} // end of GALextinct_Somm25
 
// ============= FITZPATRICK & MASSA 1990 ====================
double GALextinct_FM90(double x, double c1, double c2, double c3, double c4, 
        double c5, double x02, double g2) {
  /*
  Created by S. Thorp, Oct 20 2024

  Input : 
    x   = wavenumber (inverse microns)
    c1  = y-intercept of linear component
    c2  = slope of linear component
    c3  = bump amplitude
    c4  = FUV rise amplitude
    c5  = FUV transition point
    x02 = x0*x0, centroid of bump squared
    g2  = gamma*gamma, width of bump squared
Returns :
    E(x-V)/E(B-V)
  */

    char fnam[] = "GALextinct_FM90" ;

    double x2, y, y2, b, k;
    x2 = x*x;
    b = x2 / ((x2-x02)*(x2-x02) + x2*g2);
    k = c1 + c2*x + c3*b;
    if (x >= c5) {
        y = x - c5;
        y2 = y * y;
        k += c4 * (0.5392*y2 + 0.05644*y2*y);
    }
    return k;

} // end of GALextinct_FM90

// ============= PEI 1992 / LI ET AL. 2008 ===================
double GALextinct_Pei4(double x, double c1, double c2, double c3, double c4) {
  /*
  Created by S. Thorp, Feb 26 2025

  Four-parameter version of the Pei 1992 (ApJ 395, 130) extinction curve.
  From Li et al. 2008 (ApJ 685, 1046) and Sommovigo et al. 2025 (arXiv:2502.13240).

  Input : 
    x   = wavenumber (inverse microns)
    c1  = UV rise
    c2  = slope
    c3  = FUV shape
    c4  = bump strength
  Returns :
    Ax/AV
  */

    char fnam[] = "GALextinct_Pei4" ;

    double x08, x046, x2175;
    double y08, y046, y2175;
    double k, b; 
    x08 = x*0.08;
    x046 = x*0.046;
    x2175 = x*0.2175;
    y08 = pow(x08, c2);
    y046 = x046*x046;
    y2175 = x2175*x2175;
    b = pow(0.145, c2);

    k = c1 / (y08 + 1.0/y08 + c3);
    k += 233.0*(1.0 - c4/4.60 - c1/(b + 1.0/b + c3)) / (y046 + 1.0/y046 + 90.0);
    k += c4 / (y2175 + 1.0/y2175 - 1.95);
    return k;

} // end of GALextinct_Pei4

// ============= FM_UNRED SPLINE ====================
double GALextinct_FM_spline(double x, int Nk, double *xk, double *yk, int lin) {
  /*
  Created by S. Thorp, Oct 22 2024

  Natural cubic spline (a la FM_UNRED).

  Option to return after linear term computed.

  Input :
    x   =  Target to evaluate curve at (inverse microns)
    Nk  =  Number of spline knots (including edges)
    xk  =  Locations of spline knots (inverse microns)
    yk  =  Value of curve at knot locations
    lin =  If 1, quit early and return a linear interpolation.
Returns :
    y   =  Value of curve at x.
  */

    char fnam[] = "GALextinct_FM_spline" ;

    // abort on x out of knot range
    if (x < xk[0] || x > xk[Nk-1]) {
      sprintf(c1err,"Spline interpolation out of bounds!");
      sprintf(c2err,"Requested %.3f. Limits are [%.3f, %.3f].", 
              x, xk[0], xk[Nk-1]);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    int j, q; //indexes
    double A, B, C, D; //coefficients
    double deltax, deltax2; // gap between bounding knots
    double y; //result

    // find index in knot list
    q = 0; // qmin = 0; qmax = Nk-2
    while (q < Nk-1) {
        if (x < xk[q+1]) { 
            break; 
        } else {
            q++;
        }
    }

    // evaluate the spline (linear part & coeffs)
    deltax = xk[q+1] - xk[q];
    deltax2 = deltax * deltax;
    A = (xk[q+1] - x) / deltax;
    B = 1.0 - A;
    y = A*yk[q] + B*yk[q+1];
    if ( lin == 1 ) { return y; } //stop at linear part
    // cubic part below
    C = (A*A*A - A) * deltax2 / 6.0;
    D = (B*B*B - B) * deltax2 / 6.0;

    // compute 2nd derivatives
    // tridiagonal solve using Thomas algorithm
    double d2yq = 0.0;
    double d2yq1 = 0.0;
    double Kb[Nk-2]; //main diagonal in tridiagonal system
    double Kc[Nk-3]; //off-diagonal in tridiagonal system
    double Vd[Nk-2]; //right hand side
    double wj; //scratch variable
    // fill vectors
    for (j=0; j<Nk-2; j++) {
        Kb[j] = (xk[j+2] - xk[j])/3.0;
        if (j<Nk-3) { Kc[j] = (xk[j+2] - xk[j+1])/6.0; }
        Vd[j] = (yk[j+2] - yk[j+1])/(xk[j+2] - xk[j+1]) - (yk[j+1] - yk[j])/(xk[j+1] - xk[j]);
    }
    // forward substitution
    for (j=1; j<Nk-2; j++) {
        wj = Kc[j-1]/Kb[j-1]; //w factor
        Kb[j] -= wj*Kc[j-1]; //update diagonal
        Vd[j] -= wj*Vd[j-1]; //update rhs
    } // forward substitution complete
    // back substitution (stop at q)
    d2yq = Vd[Nk-3]/Kb[Nk-3]; //final element of solution
    // if q=Nk-2, terminate straight away
    // otherwise enter, the loop and work back to q
    for (j=Nk-4; j>q-2; j--) { //loop backwards
        d2yq1 = d2yq; //shift previous element
        if (j<0) { //we've gone far enough
            d2yq = 0;
            break;
        } else { //find next element
            d2yq = (Vd[j] - Kc[j]*d2yq1)/Kb[j];
        }
    }
    y += C*d2yq + D*d2yq1;
    return y;
} //end GALextinct_FM_spline

// ========== FUNCTION TO RETURN EBV(SFD) =================
void MWgaldust(
	       double RA          // (I) RA
	       ,double DEC        // (I) DEC
	       ,double *galxtinct // (O) u,g,r,i,z extinction
	       ,double *galebmv   // (O) E(B-V)
	       )

{
   int      imap;
   int      qInterp;
   int      qVerbose;
   int      qNoloop;
   int      nGal;
   double    tmpl;
   double    tmpb;
   float *  pGall = NULL;
   float *  pGalb = NULL;
   float *  pMapval;
   double dustval;

   /* Declarations for keyword input values */
   char  *  pMapName;
   char  *  pIPath     = NULL ;
   char     pDefMap[]  = "Ebv" ;
   char     pDefPath[200];

   /* Declarations for data file names */
   char     pFileN[MAX_FILE_NAME_LEN];
   char     pFileS[MAX_FILE_NAME_LEN];
   struct   mapParms {
      char *   pName;
      char *   pFile1;
      char *   pFile2;
   } ppMapAll[] = {
     { "Ebv" , "SFD_dust_4096_ngp.fits", "SFD_dust_4096_sgp.fits" },
     { "I100", "SFD_i100_4096_ngp.fits", "SFD_i100_4096_sgp.fits" },
     { "X"   , "SFD_xmap_ngp.fits"     , "SFD_xmap_sgp.fits"      },
     { "T"   , "SFD_temp_ngp.fits"     , "SFD_temp_sgp.fits"      },
     { "mask", "SFD_mask_4096_ngp.fits", "SFD_mask_4096_sgp.fits" }
   };

   double RV[5];

   const int nmap = sizeof(ppMapAll) / sizeof(ppMapAll[0]);

   /* Set defaults */

   // xxx old   sprintf(pDefPath, "%s/maps", getenv("DUST_DIR") );
   sprintf(pDefPath, "%s/MWDUST", getenv("SNDATA_ROOT") );
   pIPath = pDefPath ;

   pMapName = pDefMap;
   qInterp = 1; /* interpolation */
   qVerbose = 0; /* not verbose */
   qNoloop = 0; /* do not read entire image into memory */

   RV[0] = 5.155; // u
   RV[1] = 3.793; // g
   RV[2] = 2.751; // r
   RV[3] = 2.086; // i
   RV[4] = 1.479; // z

   // Translate from RA and DEC to galactic

   slaEqgal(RA,DEC,&tmpl,&tmpb);
   nGal = 1;
   pGall = ccvector_build_(nGal);
   pGalb = ccvector_build_(nGal);
   pGall[0] = (float)tmpl;
   pGalb[0] = (float)tmpb;

   /* Determine the file names to use */
   for (imap=0; imap < nmap; imap++) {
      if (strcmp(pMapName,ppMapAll[imap].pName) == 0) {
	         sprintf(pFileN, "%s/%s", pIPath, ppMapAll[imap].pFile1);
	         sprintf(pFileS, "%s/%s", pIPath, ppMapAll[imap].pFile2);
      }
   }

   /* Read values from FITS files in Lambert projection */
   pMapval = lambert_getval(pFileN, pFileS, nGal, pGall, pGalb,
    qInterp, qNoloop, qVerbose);

   dustval = (double) *pMapval;
   galxtinct[0] = RV[0]*dustval;
   galxtinct[1] = RV[1]*dustval;
   galxtinct[2] = RV[2]*dustval;
   galxtinct[3] = RV[3]*dustval;
   galxtinct[4] = RV[4]*dustval;
   *galebmv      = dustval;
   return;

}



char Label_lam_nsgp[]  = "LAM_NSGP";
char Label_lam_scal[]  = "LAM_SCAL";

uchar *  label_lam_nsgp  = (uchar*)Label_lam_nsgp;
uchar *  label_lam_scal  = (uchar*)Label_lam_scal;

/******************************************************************************/
/* Fortran wrapper for reading Lambert FITS files */
void DECLARE(fort_lambert_getval)
  (char  *  pFileN,
   char  *  pFileS,
   void  *  pNGal,
   float *  pGall,
   float *  pGalb,
   void  *  pQInterp,
   void  *  pQNoloop,
   void  *  pQVerbose,
   float *  pOutput)
{
   int      iChar;
   int      qInterp;
   int      qNoloop;
   int      qVerbose;
   long     iGal;
   long     nGal;
   float *  pTemp;

   /* Truncate the Fortran-passed strings with a null,
    * in case they are padded with spaces */
   for (iChar=0; iChar < 80; iChar++)
    if (pFileN[iChar] == ' ') pFileN[iChar] = '\0';
   for (iChar=0; iChar < 80; iChar++)
    if (pFileS[iChar] == ' ') pFileS[iChar] = '\0';

   /* Select the 4-byte words passed by a Fortran call */
   if (sizeof(short) == 4) {
      nGal = *((short *)pNGal);
      qInterp = *((short *)pQInterp);
      qNoloop = *((short *)pQNoloop);
      qVerbose = *((short *)pQVerbose);
   } else if (sizeof(int) == 4) {
      nGal = *((int *)pNGal);
      qInterp = *((int *)pQInterp);
      qNoloop = *((int *)pQNoloop);
      qVerbose = *((int *)pQVerbose);
   } else if (sizeof(long) == 4) {
      nGal = *((long *)pNGal);
      qInterp = *((long *)pQInterp);
      qNoloop = *((long *)pQNoloop);
      qVerbose = *((long *)pQVerbose);
   }

   pTemp = lambert_getval(pFileN, pFileS, nGal, pGall, pGalb,
    qInterp, qNoloop, qVerbose);

   /* Copy results into Fortran-passed location for "pOutput",
    * assuming that memory has already been allocated */
   for (iGal=0; iGal < nGal; iGal++) pOutput[iGal] = pTemp[iGal];
}

/******************************************************************************/
/* Read one value at a time from NGP+SGP polar projections.
 * Set qInterp=1 to interpolate, or =0 otherwise.
 * Set qVerbose=1 to for verbose output, or =0 otherwise.
 */
float * lambert_getval
  (char  *  pFileN,
   char  *  pFileS,
   long     nGal,
   float *  pGall,
   float *  pGalb,
   int      qInterp,
   int      qNoloop,
   int      qVerbose)
{
   int      iloop;
   int      iGal;
   int      ii;
   int      jj;
   int   *  pNS; /* 0 for NGP, 1 for SGP */
   int      nIndx;
   int   *  pIndx;
   int   *  pXPix;
   int   *  pYPix;
   int      xPix;
   int      yPix;
   int      xsize;
   DSIZE    pStart[2];
   DSIZE    pEnd[2];
   DSIZE    nSubimg;
   float *  pSubimg;
   float    dx;
   float    dy;
   float    xr;
   float    yr;
   float    pWeight[4];
   float    mapval;
   float *  pOutput;
   float *  pDX = NULL;
   float *  pDY = NULL;

   /* Variables for FITS files */
   int      qRead;
   int      numAxes;
   DSIZE *  pNaxis;
   char  *  pFileIn = NULL;
   HSIZE    nHead;
   uchar *  pHead;

   /* Allocate output data array */
   pNS = ccivector_build_(nGal);
   pOutput = ccvector_build_(nGal);

   /* Decide if each point should be read from the NGP or SGP projection */
   for (iGal=0; iGal < nGal; iGal++)
    pNS[iGal] = (pGalb[iGal] >= 0.0) ? 0 : 1; /* ==0 for NGP, ==1 for SGP */

   if (qNoloop == 0) {  /* LOOP THROUGH ONE POINT AT A TIME */

      /* Loop through first NGP then SGP */
      for (iloop=0; iloop < 2; iloop++) {
         qRead = 0;

         /* Loop through each data point */
         for (iGal=0; iGal < nGal; iGal++) {
            if (pNS[iGal] == iloop) {

               /* Read FITS header for this projection if not yet read */
               if (qRead == 0) {
                  if (iloop == 0) pFileIn = pFileN; else pFileIn = pFileS;
                  fits_read_file_fits_header_only_(pFileIn, &nHead, &pHead);
                  qRead = 1;
               }

               if (qInterp == 0) {  /* NEAREST PIXELS */

                  /* Determine the nearest pixel coordinates */
                  lambert_lb2pix(pGall[iGal], pGalb[iGal], nHead, pHead,
                   &xPix, &yPix);

                  pStart[0] = xPix;
                  pStart[1] = yPix;

                  /* Read one pixel value from data file */
                  fits_read_point_(pFileIn, nHead, pHead, pStart, &mapval);
                  pOutput[iGal] = mapval;

                  if (qVerbose != 0)
                   printf("%8.3f %7.3f %1d %8d %8d %12.5e\n",
                   pGall[iGal], pGalb[iGal], iloop, xPix, yPix, mapval);

               } else {  /* INTERPOLATE */

                  fits_compute_axes_(&nHead, &pHead, &numAxes, &pNaxis);

                  /* Determine the fractional pixel coordinates */
                  lambert_lb2fpix(pGall[iGal], pGalb[iGal], nHead, pHead,
                   &xr, &yr);
/* The following 4 lines introduced an erroneous 1/2-pixel shift
   (DJS 18-Mar-1999).
                  xPix = (int)(xr-0.5);
                  yPix = (int)(yr-0.5);
                  dx = xPix - xr + 1.5;
                  dy = yPix - yr + 1.5;
 */
                  xPix = (int)(xr);
                  yPix = (int)(yr);
                  dx = xPix - xr + 1.0;
                  dy = yPix - yr + 1.0;

                  /* Force pixel values to fall within the image boundaries */
                  if (xPix < 0) { xPix = 0; dx = 1.0; }
                  if (yPix < 0) { yPix = 0; dy = 1.0; }
                  if (xPix >= pNaxis[0]-1) { xPix = pNaxis[0]-2; dx = 0.0; }
                  if (yPix >= pNaxis[1]-1) { yPix = pNaxis[1]-2; dy = 0.0; }

                  pStart[0] = xPix;
                  pStart[1] = yPix;
                  pEnd[0] = xPix + 1;
                  pEnd[1] = yPix + 1;

                  /* Create array of weights */
                  pWeight[0] =    dx  *    dy  ;
                  pWeight[1] = (1-dx) *    dy  ;
                  pWeight[2] =    dx  * (1-dy) ;
                  pWeight[3] = (1-dx) * (1-dy) ;

                  /* Read 2x2 array from data file */
                  fits_read_subimg_(pFileIn, nHead, pHead, pStart, pEnd,
                   &nSubimg, &pSubimg);

                  pOutput[iGal] = 0.0;
                  for (jj=0; jj < 4; jj++)
                   pOutput[iGal] += pWeight[jj] * pSubimg[jj];

                  fits_free_axes_(&numAxes, &pNaxis);
                  ccfree_((void **)&pSubimg);

                  if (qVerbose != 0)
                   printf("%8.3f %7.3f %1d %9.3f %9.3f %12.5e\n",
                   pGall[iGal], pGalb[iGal], iloop, xr, yr, pOutput[iGal]);

               }  /* -- END NEAREST PIXEL OR INTERPOLATE -- */
            }
         }
      }

   } else {  /* READ FULL IMAGE */

      pIndx = ccivector_build_(nGal);
      pXPix = ccivector_build_(nGal);
      pYPix = ccivector_build_(nGal);
      if (qInterp != 0) {
         pDX = ccvector_build_(nGal);
         pDY = ccvector_build_(nGal);
      }

      /* Loop through first NGP then SGP */
      for (iloop=0; iloop < 2; iloop++) {

         /* Determine the indices of data points in this hemisphere */
         nIndx = 0;
         for (iGal=0; iGal < nGal; iGal++) {
            if (pNS[iGal] == iloop) {
               pIndx[nIndx] = iGal;
               nIndx++;
            }
         }

         /* Do not continue if no data points in this hemisphere */
         if (nIndx > 0) {

            /* Read FITS header for this projection */
            if (iloop == 0) pFileIn = pFileN; else pFileIn = pFileS;
            fits_read_file_fits_header_only_(pFileIn, &nHead, &pHead);

            if (qInterp == 0) {  /* NEAREST PIXELS */

               /* Determine the nearest pixel coordinates */
               for (ii=0; ii < nIndx; ii++) {
                  lambert_lb2pix(pGall[pIndx[ii]], pGalb[pIndx[ii]],
                   nHead, pHead, &pXPix[ii], &pYPix[ii]);
               }

               pStart[0] = ivector_minimum(nIndx, pXPix);
               pEnd[0] = ivector_maximum(nIndx, pXPix);
               pStart[1] = ivector_minimum(nIndx, pYPix);
               pEnd[1] = ivector_maximum(nIndx, pYPix);

               /* Read smallest subimage containing all points in this hemi */
               fits_read_subimg_(pFileIn, nHead, pHead, pStart, pEnd,
                &nSubimg, &pSubimg);
               xsize = pEnd[0] - pStart[0] + 1;

               /* Determine data values */
               for (ii=0; ii < nIndx; ii++) {
                  pOutput[pIndx[ii]] = pSubimg[ pXPix[ii]-pStart[0] +
                   (pYPix[ii]-pStart[1]) * xsize ];

               }

               ccfree_((void **)&pSubimg);

            } else {  /* INTERPOLATE */

               fits_compute_axes_(&nHead, &pHead, &numAxes, &pNaxis);

               /* Determine the fractional pixel coordinates */
               for (ii=0; ii < nIndx; ii++) {
                  lambert_lb2fpix(pGall[pIndx[ii]], pGalb[pIndx[ii]],
                   nHead, pHead, &xr, &yr);
/* The following 4 lines introduced an erroneous 1/2-pixel shift
   (DJS 03-Mar-2004).
                  pXPix[ii] = (int)(xr-0.5);
                  pYPix[ii] = (int)(yr-0.5);
                  pDX[ii] = pXPix[ii] - xr + 1.5;
                  pDY[ii] = pYPix[ii] - yr + 1.5;
*/
                  pXPix[ii] = (int)(xr);
                  pYPix[ii] = (int)(yr);
                  pDX[ii] = pXPix[ii] - xr + 1.0;
                  pDY[ii] = pYPix[ii] - yr + 1.0;

                  /* Force pixel values to fall within the image boundaries */
                  if (pXPix[ii] < 0) { pXPix[ii] = 0; pDX[ii] = 1.0; }
                  if (pYPix[ii] < 0) { pYPix[ii] = 0; pDY[ii] = 1.0; }
                  if (pXPix[ii] >= pNaxis[0]-1)
                   { pXPix[ii] = pNaxis[0]-2; pDX[ii] = 0.0; }
                  if (pYPix[ii] >= pNaxis[1]-1)
                   { pYPix[ii] = pNaxis[1]-2; pDY[ii] = 0.0; }

               }

               pStart[0] = ivector_minimum(nIndx, pXPix);
               pEnd[0] = ivector_maximum(nIndx, pXPix) + 1;
               pStart[1] = ivector_minimum(nIndx, pYPix);
               pEnd[1] = ivector_maximum(nIndx, pYPix) + 1;

               /* Read smallest subimage containing all points in this hemi */
               fits_read_subimg_(pFileIn, nHead, pHead, pStart, pEnd,
                &nSubimg, &pSubimg);
               xsize = pEnd[0] - pStart[0] + 1;

               /* Determine data values */
               for (ii=0; ii < nIndx; ii++) {
                  /* Create array of weights */
                  pWeight[0] =    pDX[ii]  *    pDY[ii]  ;
                  pWeight[1] = (1-pDX[ii]) *    pDY[ii]  ;
                  pWeight[2] =    pDX[ii]  * (1-pDY[ii]) ;
                  pWeight[3] = (1-pDX[ii]) * (1-pDY[ii]) ;

                  pOutput[pIndx[ii]] =
                    pWeight[0] * pSubimg[
                     pXPix[ii]-pStart[0]   + (pYPix[ii]-pStart[1]  )*xsize ]
                   +pWeight[1] * pSubimg[
                     pXPix[ii]-pStart[0]+1 + (pYPix[ii]-pStart[1]  )*xsize ]
                   +pWeight[2] * pSubimg[
                     pXPix[ii]-pStart[0]   + (pYPix[ii]-pStart[1]+1)*xsize ]
                   +pWeight[3] * pSubimg[
                     pXPix[ii]-pStart[0]+1 + (pYPix[ii]-pStart[1]+1)*xsize ] ;

               }

               fits_free_axes_(&numAxes, &pNaxis);
               ccfree_((void **)&pSubimg);

            }  /* -- END NEAREST PIXEL OR INTERPOLATE -- */
         }

      }

      ccivector_free_(pIndx);
      ccivector_free_(pXPix);
      ccivector_free_(pYPix);
      if (qInterp != 0) {
         ccvector_free_(pDX);
         ccvector_free_(pDY);
      }
   }

   /* Free the memory allocated for the FITS header 
      (Moved outside previous brace by Chris Stoughton 19-Jan-1999) */
   fits_dispose_array_(&pHead);

   /* Deallocate output data array */
   ccivector_free_(pNS);

   return pOutput;
}

/******************************************************************************/
/* Transform from galactic (l,b) coordinates to fractional (x,y) pixel location.
 * Latitude runs clockwise from X-axis for NGP, counterclockwise for SGP.
 * This function returns the ZERO-INDEXED pixel position.
 * Updated 04-Mar-1999 to allow ZEA coordinate convention for the same
 * projection.
 */
void lambert_lb2fpix
  (float    gall,   /* Galactic longitude */
   float    galb,   /* Galactic latitude */
   HSIZE    nHead,
   uchar *  pHead,
   float *  pX,     /* X position in pixels from the center */
   float *  pY)     /* Y position in pixels from the center */
{
   int      q1;
   int      q2;
   int      nsgp;
   float    scale;
   float    crval1;
   float    crval2;
   float    crpix1;
   float    crpix2;
   float    cdelt1;
   float    cdelt2;
   float    cd1_1;
   float    cd1_2;
   float    cd2_1;
   float    cd2_2;
   float    lonpole;
   float    xr;
   float    yr;
   float    theta;
   float    phi;
   float    Rtheta;
   float    denom;
   static double dradeg = 180 / 3.1415926534;
   char  *  pCtype1;
   char  *  pCtype2;

   fits_get_card_string_(&pCtype1, label_ctype1, &nHead, &pHead);
   fits_get_card_string_(&pCtype2, label_ctype2, &nHead, &pHead);
   fits_get_card_rval_(&crval1, label_crval1, &nHead, &pHead);
   fits_get_card_rval_(&crval2, label_crval2, &nHead, &pHead);
   fits_get_card_rval_(&crpix1, label_crpix1, &nHead, &pHead);
   fits_get_card_rval_(&crpix2, label_crpix2, &nHead, &pHead);

   if (strcmp(pCtype1, "LAMBERT--X")  == 0 &&
       strcmp(pCtype2, "LAMBERT--Y")  == 0) {

      fits_get_card_ival_(&nsgp, label_lam_nsgp, &nHead, &pHead);
      fits_get_card_rval_(&scale, label_lam_scal, &nHead, &pHead);

      lambert_lb2xy(gall, galb, nsgp, scale, &xr, &yr);
      *pX = xr + crpix1 - crval1 - 1.0;
      *pY = yr + crpix2 - crval2 - 1.0;

   } else if (strcmp(pCtype1, "GLON-ZEA")  == 0 &&
              strcmp(pCtype2, "GLAT-ZEA")  == 0) { 

      q1 = fits_get_card_rval_(&cdelt1, label_cdelt1, &nHead, &pHead);
      q2 = fits_get_card_rval_(&cdelt2, label_cdelt2, &nHead, &pHead);
      if (q1 == TRUE_MWDUST && q2 == TRUE_MWDUST) {
          cd1_1 = cdelt1;
          cd1_2 = 0.0;
          cd2_1 = 0.0;
          cd2_2 = cdelt2;
       } else {
         fits_get_card_rval_(&cd1_1, label_cd1_1, &nHead, &pHead);
         fits_get_card_rval_(&cd1_2, label_cd1_2, &nHead, &pHead);
         fits_get_card_rval_(&cd2_1, label_cd2_1, &nHead, &pHead);
         fits_get_card_rval_(&cd2_2, label_cd2_2, &nHead, &pHead);
      }
      q1 = fits_get_card_rval_(&lonpole, label_lonpole, &nHead, &pHead);
      if (q1 == FALSE_MWDUST) lonpole = 180.0; /* default value */

      /* ROTATION */
      /* Equn (4) - degenerate case */
      if (crval2 > 89.9999) {
         theta = galb;
         phi = gall + 180.0 + lonpole - crval1;
      } else if (crval2 < -89.9999) {
         theta = -galb;
         phi = lonpole + crval1 - gall;
      } else {
         printf("ERROR: Unsupported projection!!!\n");
         /* Assume it's an NGP projection ... */
         theta = galb;
         phi = gall + 180.0 + lonpole - crval1;
      }

      /* Put phi in the range [0,360) degrees */
      phi = phi - 360.0 * floor(phi/360.0);

      /* FORWARD MAP PROJECTION */
      /* Equn (26) */
      Rtheta = 2.0 * dradeg * sin((0.5 / dradeg) * (90.0 - theta));

      /* Equns (10), (11) */
      xr = Rtheta * sin(phi / dradeg);
      yr = - Rtheta * cos(phi / dradeg);

      /* SCALE FROM PHYSICAL UNITS */
      /* Equn (3) after inverting the matrix */
      denom = cd1_1 * cd2_2 - cd1_2 * cd2_1;
      *pX = (cd2_2 * xr - cd1_2 * yr) / denom + (crpix1 - 1.0);
      *pY = (cd1_1 * yr - cd2_1 * xr) / denom + (crpix2 - 1.0);

   } else {

      *pX = -99.0;
      *pY = -99.0;

   }

   ccfree_((void **)&pCtype1);
   ccfree_((void **)&pCtype2);
}

/******************************************************************************/
/* Transform from galactic (l,b) coordinates to (ix,iy) pixel location.
 * Latitude runs clockwise from X-axis for NGP, counterclockwise for SGP.
 * This function returns the ZERO-INDEXED pixel position.
 * 
 */
void lambert_lb2pix
  (float    gall,   /* Galactic longitude */
   float    galb,   /* Galactic latitude */
   HSIZE    nHead,
   uchar *  pHead,
   int   *  pIX,    /* X position in pixels from the center */
   int   *  pIY)    /* Y position in pixels from the center */
{
   int      naxis1;
   int      naxis2;
   float    xr;
   float    yr;

   fits_get_card_ival_(&naxis1, label_naxis1, &nHead, &pHead);
   fits_get_card_ival_(&naxis2, label_naxis2, &nHead, &pHead);

   lambert_lb2fpix(gall, galb, nHead, pHead, &xr, &yr);
   *pIX = (int)floor(xr + 0.5);
   *pIY = (int)floor(yr + 0.5);

   /* Force bounds to be valid at edge, for ex at l=0,b=0 */
   //printf("NAXES %d %d\n", naxis1,naxis2);
   if (*pIX >= naxis1) *pIX = naxis1 - 1;
   if (*pIY >= naxis2) *pIY = naxis2 - 1;
}

/******************************************************************************/
/* Transform from galactic (l,b) coordinates to (x,y) coordinates from origin.
 * Latitude runs clockwise from X-axis for NGP, counterclockwise for SGP.
 */
void lambert_lb2xy
  (float    gall,   /* Galactic longitude */
   float    galb,   /* Galactic latitude */
   int      nsgp,   /* +1 for NGP projection, -1 for SGP */
   float    scale,  /* Radius of b=0 to b=90 degrees in pixels */
   float *  pX,     /* X position in pixels from the center */
   float *  pY)     /* Y position in pixels from the center */
{
   double   rho;
   static double dradeg = 180 / 3.1415926534;

   rho = sqrt(1. - nsgp * sin(galb/dradeg));
/* The following two lines were modified by Hans Schwengeler (17-Mar-1999)
   to get this to work on a Tur64 Unix 4.0E (DEC Alpha).  It appears that
   float and double on not the same on this machine.
   *pX = rho * cos(gall/dradeg) * scale;
   *pY = -nsgp * rho * sin(gall/dradeg) * scale;
*/
   *pX = rho * cos((float)((double)gall/dradeg)) * scale;
   *pY = -nsgp * rho * sin((float)((double)gall/dradeg)) * scale;
}
/******************************************************************************/
/* Find the min value of the nData elements of an integer array pData[].
 */
int ivector_minimum
  (int      nData,
   int   *  pData)
{
   int      i;
   int      vmin;

   vmin = pData[0];
   for (i=1; i<nData; i++) if (pData[i] < vmin) vmin=pData[i];

   return vmin;
}

/******************************************************************************/
/* Find the max value of the nData elements of an integer array pData[].
 */
int ivector_maximum
  (int      nData,
   int   *  pData)
{
   int      i;
   int      vmax;

   vmax = pData[0];
   for (i=1; i<nData; i++) if (pData[i] > vmax) vmax=pData[i];

   return vmax;
}


/**********************************************************************/
/*
 * Read an ASCII file as a 2-dimensional array of floating point numbers.
 * The number of columns is determined by the number of entries on the
 * first non-comment line.  Missing values are set to zero.
 * Comment lines (preceded with a hash mark) are ignored.
 * The returned matrix is in COLUMN-MAJOR order, and as such is
 * addressed as (*ppData)[iCol*NRows+iRow].
 * This is the Fortran storage scheme, but it is useful in addressing
 * a column as a vector that is contiguous in memory.
 * If the data array is empty, it should be passed with the value ppData = NULL.
 *
 * Return IO_GOOD if the file exists, and IO_FALSE_MWDUST otherwise.
 */
int asciifile_read_colmajor
  (char     pFileName[],
   int      numColsMax,
   int   *  pNRows,
   int   *  pNCols,
   float ** ppData)
{
   int      iCol;
   int      iRow;
   int      qExist;
   MEMSZ    memSize;
   float *  pNewData;

   qExist = asciifile_read_rowmajor(pFileName, numColsMax,
    pNRows, pNCols, ppData);

   if (qExist == IO_GOOD) {
      /* Create a new array of the same dimensions */
      memSize = sizeof(float) * (*pNRows)*(*pNCols);
      ccalloc_(&memSize, (void **)&pNewData);

      /* Copy the data into this array in column-major order */
      for (iCol=0; iCol < (*pNCols); iCol++) {
      for (iRow=0; iRow < (*pNRows); iRow ++) {
         pNewData[iCol*(*pNRows)+iRow] = (*ppData)[iRow*(*pNCols)+iCol];
      } }

      /* Toss out the old array */
      ccfree_((void **)ppData);
      *ppData = pNewData;
   }

   return qExist;
}

/******************************************************************************/
/*
 * Read an ASCII file as a 2-dimensional array of floating point numbers.
 * The number of columns is determined by the number of entries on the
 * first non-comment line.  Missing values are set to zero.
 * Comment lines (preceded with a hash mark) are ignored.
 * The returned matrix is in row-major order, and as such is
 * addressed as (*ppData)[iRow*NCols+iCol].
 * If the data array is empty, it should be passed with the value ppData = NULL.
 *
 * Return IO_GOOD if the file exists, and IO_BAD otherwise.
 */
int asciifile_read_rowmajor
  (char     pFileName[],
   int      numColsMax,
   int   *  pNRows,
   int   *  pNCols,
   float ** ppData)
{
   int      fileNum;
   int      iCol;
   int      nValues;
   int      qExist;
   const int numAddRows = 10;
   MEMSZ    memSize;
   MEMSZ    newMemSize;
   char  *  iq;
   float *  pValues;
   float *  pData;
   char     pPrivR[] = "r\0";

   *pNCols = 0;
   *pNRows = 0;

   qExist = inoutput_open_file(&fileNum, pFileName, pPrivR);

   if (qExist == IO_GOOD) {

      /* Allocate a starting block of memory for the data array */
      /* Start with enough memory for numAddRows rows */
      memSize = numAddRows * sizeof(float) * numColsMax;
      /* SHOULD BE ABLE TO USE REALLOC BELOW !!!??? */
      ccalloc_(&memSize, (void **)ppData);
      pData = *ppData;

      /* Allocate the temporary memory for each line of values */
      pValues = ccvector_build_(numColsMax);

      /* Read the first line, which determines the # of cols for all lines */
      iq = asciifile_read_line(fileNum, numColsMax, pNCols, pData);

      /* Read the remaining lines if a first line was read successfully */
      if (iq != NULL) {
         *pNRows=1;

         while ((iq = asciifile_read_line(fileNum, numColsMax,
          &nValues, pValues)) != NULL) {

            /* Allocate more memory for the data array if necessary */
            /* Always keep enough memory for at least one more row */
            newMemSize = sizeof(float) * (*pNRows + 1) * (*pNCols);
            if (newMemSize > memSize) {
               newMemSize = newMemSize + sizeof(float)*(numAddRows);
               ccalloc_resize_(&memSize, &newMemSize, (void **)ppData);
               pData = *ppData;
               memSize = newMemSize;
            }
   
            /* Case where the line contained fewer values than allowed */
            if (nValues < *pNCols) {
               for (iCol=0; iCol<nValues; iCol++)
                  pData[iCol+(*pNCols)*(*pNRows)] = pValues[iCol];
               for (iCol=nValues; iCol < *pNCols; iCol++)
                  pData[iCol+(*pNCols)*(*pNRows)] = 0;

            /* Case where line contained as many or more values than allowed */
            } else {
               for (iCol=0; iCol < *pNCols; iCol++)
                  pData[iCol+(*pNCols)*(*pNRows)] = pValues[iCol];
            }

            (*pNRows)++;
         }
      }

      inoutput_close_file(fileNum);
      ccvector_free_(pValues);
   }

   return qExist;
}

/******************************************************************************/
/*
 * Read all values from the next line of the input file.
 * Return NULL if end of file is reached.
 * Comment lines (preceded with a hash mark) are ignored.
 */
char * asciifile_read_line
  (int      filenum,
   int      numColsMax,
   int   *  pNValues,
   float *  pValues)
{
   char  *  pRetval;
   const    char whitespace[] = " \t\n";
   char     pTemp[MAX_FILE_LINE_LEN];
   char  *  pToken;

   /* Read next line from open file into temporary string */
   /* Ignore comment lines that are preceded with a hash mark */
   while (( (pRetval = fgets(pTemp, MAX_FILE_LINE_LEN, pFILEfits[filenum]))
    != NULL) && (pTemp[0] == '#') );

   if (pRetval != NULL) {
      /* Read one token at a time as a floating point value */
      *pNValues = 0;
      pToken = pTemp;
      while ((pToken = strtok(pToken, whitespace)) != NULL &&
             (*pNValues) < numColsMax ) {
         sscanf(pToken, "%f", &pValues[*pNValues]);
         pToken = NULL;
         (*pNValues)++;
      }
   }

   return pRetval;
}

/******************************************************************************/
/*
 * Subroutines to read and write FITS format files.
 * Note all variables are passed as pointers, so the routines can be called
 * by either C or Fortran programs.
 * Remember to omit the final underscore for calls from Fortran,
 * so one says 'call fits_add_card(...)' or 'i=fits_add_card(...)' in Fortran,
 * but 'i=fits_add_card_(...)' in C.
 *
 * D Schlegel -- Berkeley -- ANSI C
 * Mar 1992  DJS  Created
 * Dec 1993  DJS  Major revisions to allow dynamic memory allocations.
 */
/******************************************************************************/

#define min(a,b) ( ((a) < (b)) ? (a) : (b) )
#define max(a,b) ( ((a) > (b)) ? (a) : (b) )
#define TRUE_MWDUST  1
#define FALSE_MWDUST 0

char Datum_zero[]    = "\0\0\0\0";
char Label_airmass[] = "AIRMASS ";
char Label_bitpix[]  = "BITPIX  ";
char Label_blank[]   = "BLANK   ";
char Label_bscale[]  = "BSCALE  ";
char Label_bzero[]   = "BZERO   ";
char Label_ctype1[]  = "CTYPE1  ";
char Label_ctype2[]  = "CTYPE2  ";
char Label_cdelt1[]  = "CDELT1  ";
char Label_cdelt2[]  = "CDELT2  ";
char Label_cd1_1[]   = "CD1_1   ";
char Label_cd1_2[]   = "CD1_2   ";
char Label_cd2_1[]   = "CD2_1   ";
char Label_cd2_2[]   = "CD2_2   ";
char Label_latpole[] = "LATPOLE ";
char Label_lonpole[] = "LONPOLE ";
char Label_crpix1[]  = "CRPIX1  ";
char Label_crpix2[]  = "CRPIX2  ";
char Label_crval1[]  = "CRVAL1  ";
char Label_crval2[]  = "CRVAL2  ";
char Label_date_obs[]= "DATE-OBS";
char Label_dec[]     = "DEC     ";
char Label_empty[]   = "        ";
char Label_end[]     = "END     ";
char Label_exposure[]= "EXPOSURE";
char Label_extend[]  = "EXTEND  ";
char Label_filtband[]= "FILTBAND";
char Label_filter[]  = "FILTER  ";
char Label_ha[]      = "HA      ";
char Label_instrume[]= "INSTRUME";
char Label_lamord[]  = "LAMORD  ";
char Label_loss[]    = "LOSS    ";
char Label_naxis[]   = "NAXIS   ";
char Label_naxis1[]  = "NAXIS1  ";
char Label_naxis2[]  = "NAXIS2  ";
char Label_object[]  = "OBJECT  ";
char Label_observer[]= "OBSERVER";
char Label_pa[]      = "PA      ";
char Label_platescl[]= "PLATESCL";
char Label_ra[]      = "RA      ";
char Label_rnoise[]  = "RNOISE  ";
char Label_rota[]    = "ROTA    ";
char Label_seeing[]  = "SEEING  ";
char Label_skyrms[]  = "SKYRMS  ";
char Label_skyval[]  = "SKYVAL  ";
char Label_slitwidt[]= "SLITWIDT";
char Label_st[]      = "ST      ";
char Label_telescop[]= "TELESCOP";
char Label_time[]    = "TIME    ";
char Label_tub[]     = "TUB     ";
char Label_ut[]      = "UT      ";
char Label_vhelio[]  = "VHELIO  ";
char Label_vminusi[] = "VMINUSI ";
char Card_simple[] =
   "SIMPLE  =                    T          "\
   "                                        ";
char Card_empty[] =
   "                                        "\
   "                                        ";
char Card_null[] =
   "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"\
   "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"\
   "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"\
   "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
char Card_end[] =
   "END                                     "\
   "                                        ";
char Text_T[] = "T";
char Text_F[] = "F";

uchar *  datum_zero    = (uchar*)Datum_zero;
uchar *  label_airmass = (uchar*)Label_airmass;
uchar *  label_bitpix  = (uchar*)Label_bitpix;
uchar *  label_blank   = (uchar*)Label_blank;
uchar *  label_bscale  = (uchar*)Label_bscale;
uchar *  label_bzero   = (uchar*)Label_bzero;
uchar *  label_ctype1  = (uchar*)Label_ctype1;
uchar *  label_ctype2  = (uchar*)Label_ctype2;
uchar *  label_cdelt1  = (uchar*)Label_cdelt1;
uchar *  label_cdelt2  = (uchar*)Label_cdelt2;
uchar *  label_cd1_1   = (uchar*)Label_cd1_1;
uchar *  label_cd1_2   = (uchar*)Label_cd1_2;
uchar *  label_cd2_1   = (uchar*)Label_cd2_1;
uchar *  label_cd2_2   = (uchar*)Label_cd2_2;
uchar *  label_latpole = (uchar*)Label_latpole;
uchar *  label_lonpole = (uchar*)Label_lonpole;
uchar *  label_crpix1  = (uchar*)Label_crpix1;
uchar *  label_crpix2  = (uchar*)Label_crpix2;
uchar *  label_crval1  = (uchar*)Label_crval1;
uchar *  label_crval2  = (uchar*)Label_crval2;
uchar *  label_date_obs= (uchar*)Label_date_obs;
uchar *  label_dec     = (uchar*)Label_dec;
uchar *  label_empty   = (uchar*)Label_empty;
uchar *  label_end     = (uchar*)Label_end;
uchar *  label_exposure= (uchar*)Label_exposure;
uchar *  label_extend  = (uchar*)Label_extend;
uchar *  label_filtband= (uchar*)Label_filtband;
uchar *  label_filter  = (uchar*)Label_filter;
uchar *  label_ha      = (uchar*)Label_ha;
uchar *  label_instrume= (uchar*)Label_instrume;
uchar *  label_lamord  = (uchar*)Label_lamord;
uchar *  label_loss    = (uchar*)Label_loss;
uchar *  label_naxis   = (uchar*)Label_naxis;
uchar *  label_naxis1  = (uchar*)Label_naxis1;
uchar *  label_naxis2  = (uchar*)Label_naxis2;
uchar *  label_object  = (uchar*)Label_object;
uchar *  label_observer= (uchar*)Label_observer;
uchar *  label_pa      = (uchar*)Label_pa;
uchar *  label_platescl= (uchar*)Label_platescl;
uchar *  label_ra      = (uchar*)Label_ra;
uchar *  label_rnoise  = (uchar*)Label_rnoise;
uchar *  label_rota    = (uchar*)Label_rota;
uchar *  label_seeing  = (uchar*)Label_seeing;
uchar *  label_skyrms  = (uchar*)Label_skyrms;
uchar *  label_skyval  = (uchar*)Label_skyval;
uchar *  label_slitwidt= (uchar*)Label_slitwidt;
uchar *  label_st      = (uchar*)Label_st;
uchar *  label_telescop= (uchar*)Label_telescop;
uchar *  label_time    = (uchar*)Label_time;
uchar *  label_tub     = (uchar*)Label_tub;
uchar *  label_ut      = (uchar*)Label_ut;
uchar *  label_vhelio  = (uchar*)Label_vhelio;
uchar *  label_vminusi = (uchar*)Label_vminusi;
uchar *  card_simple   = (uchar*)Card_simple;
uchar *  card_empty    = (uchar*)Card_empty;
uchar *  card_null     = (uchar*)Card_null;
uchar *  card_end      = (uchar*)Card_end;
uchar *  text_T        = (uchar*)Text_T;
uchar *  text_F        = (uchar*)Text_F;

/******************************************************************************/
/*
 * Read in FITS format data.  Assume the header is a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * Any data that follows is ignored.
 */
void fits_read_file_fits_header_only_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      fileNum;
   char     pPrivR[] = "r\0";

   inoutput_open_file(&fileNum, pFileName, pPrivR);

   /* Read header */
   fits_read_fits_header_(&fileNum, pNHead, ppHead);

   inoutput_close_file(fileNum);
}

/******************************************************************************/
/*
 * Read in header cards that are in ASCII format, for example as output
 * by IRAF with carraige returns after lines that are not even 80 characters.
 * The data is read into an array in FITS format.
 *
 * Return IO_GOOD if the file exists, and IO_BAD otherwise.
 */
int fits_read_file_ascii_header_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      cLen;
   int      fileNum;
   int      maxLen = 80;
   int      qExist;
   char     pPrivR[] = "r\0";
   uchar    pCard[80];

   qExist = inoutput_open_file(&fileNum, pFileName, pPrivR);

   if (qExist == IO_GOOD) {
      /* Read the header into memory until the end of file */
      *pNHead = 0;
      while (fgets((char *)pCard, maxLen, pFILEfits[fileNum]) != NULL) {
         /* Replace the /0 and remainder of card with blanks */
         for (cLen=strlen((const char *)pCard); cLen < 80; cLen++)
          pCard[cLen]=' ';

         fits_add_card_(pCard, pNHead, ppHead);
      }

      /* If no END card, then add one */
      if (fits_find_card_(label_end, pNHead, ppHead) == *pNHead) {
         fits_add_card_(card_end, pNHead, ppHead);
      }

      /* Close the file */
      inoutput_close_file(fileNum);
   }

   return qExist;
}

/******************************************************************************/
/*
 * Read in FITS format data.  Assume the header is a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows as either real values or as integer values
 * that should be scaled by the BZERO and BSCALE values.  The data
 * format is determined by the BITPIX card in the header.
 * The data is rescaled to 32-bit reals.
 * Also, the BITPIX card in the header is changed to -32.
 * Memory is dynamically allocated for the header and data arrays.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than indicated in the header, in which case the difference is returned.
 */
DSIZE fits_read_file_fits_r4_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   float ** ppData)
{
   int      bitpix;
   DSIZE    retval;

   retval = fits_read_file_fits_noscale_(pFileName, pNHead,
    ppHead, pNData, &bitpix, (uchar **)ppData);

   /* Convert data to real*4 if not already */
   fits_data_to_r4_(pNHead, ppHead, pNData, (uchar **)ppData);

   return retval;
}

/******************************************************************************/
/*
 * Read in FITS format data.  Assume the header is a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows as either real values or as integer values
 * that should be scaled by the BZERO and BSCALE values.  The data
 * format is determined by the BITPIX card in the header.
 * The data is rescaled to 16-bit integers.
 * Also, the BITPIX card in the header is changed to 16.
 * Memory is dynamically allocated for the header and data arrays.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than indicated in the header, in which case the difference is returned.
 */
DSIZE fits_read_file_fits_i2_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   short int   ** ppData)
{
   int      bitpix;
   DSIZE    retval;

   retval = fits_read_file_fits_noscale_(pFileName, pNHead,
    ppHead, pNData, &bitpix, (uchar **)ppData);

   /* Convert data to integer*4 if not already */
   fits_data_to_i2_(pNHead, ppHead, pNData, (uchar **)ppData);

   return retval;
}

/******************************************************************************/
/*
 * Read subimage from a FITS format data file, indexed from pStart to pEnd
 * in each dimension.
 *
 * The header is assumed to already be read using
 * fits_read_file_fits_header_only_(), to avoid reading it upon every
 * call to this routine.  The axis dimensions and BITPIX are read from
 * the header that is passed.  The dimensions of pLoc must agree with
 * the dimensions specified by NAXIS in this header.
 *
 * The data values are rescaled to 32-bit reals.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than requested, in which case the difference is returned.
 */

DSIZE fits_read_subimg_
  (char     pFileName[],
   HSIZE    nHead,
   uchar *  pHead,
   DSIZE *  pStart,
   DSIZE *  pEnd,
   DSIZE *  pNVal,
   float ** ppVal)
{
   int      bitpix;
   DSIZE    iloc;
   DSIZE    nExpect;
   int      size;
   MEMSZ    memSize;
   int      iAxis;
   int      numAxes;
   DSIZE *  pNaxis;
   float    bscale;
   float    bzero;
   uchar *  pData;

   int      fileNum;
   char     pPrivR[] = "r\0";

   inoutput_open_file(&fileNum, pFileName, pPrivR);

   /* Skip header */
   fits_skip_header_(&fileNum);

   /* From the given header, read BITPIX and PNAXIS */
   fits_get_card_ival_(&bitpix, label_bitpix, &nHead, &pHead);
   fits_compute_axes_(&nHead, &pHead, &numAxes, &pNaxis);

   /* Allocate memory for output */
   nExpect = 1;
   for (iAxis=0; iAxis < numAxes; iAxis++)
    nExpect *= (pEnd[iAxis] - pStart[iAxis] + 1);
   size = fits_size_from_bitpix_(&bitpix);
   memSize = size * nExpect;
   ccalloc_(&memSize, (void **)&pData);

   *pNVal = 0;
   fits_read_subimg1(numAxes, pNaxis, pStart, pEnd, fileNum, bitpix,
    pNVal, pData);
#ifdef LITTLE_ENDIAN
   fits_byteswap(bitpix, *pNVal, pData);
#endif

   /* Convert data to real*4 if not already */
   if (bitpix == -32) {
      *ppVal = (float *)pData;
   } else {
      /* Get the scaling parameters from the header */
      if (fits_get_card_rval_(&bscale, (uchar *)Label_bscale, &nHead, &pHead)
       == FALSE_MWDUST) {
         bscale = 1.0;  /* Default value for BSCALE */
      }
      if (fits_get_card_rval_(&bzero , (uchar *)Label_bzero , &nHead, &pHead)
       == FALSE_MWDUST) {
         bzero = 0.0;  /* Default value for BZERO */
      }
 
      memSize = sizeof(float) * nExpect;
      ccalloc_(&memSize, (void **)ppVal);
      for (iloc=0; iloc < *pNVal; iloc++)
       (*ppVal)[iloc] = fits_get_rval_(&iloc, &bitpix, &bscale, &bzero, &pData);
   }
 
   inoutput_close_file(fileNum);

   /* Plug a memory leak - Chris Stoughton 19-Jan-1999 */
   fits_free_axes_(&numAxes, &pNaxis);

   return (nExpect - (*pNVal));
}

void fits_read_subimg1
  (int      nel,
   DSIZE *  pNaxis,
   DSIZE *  pStart,
   DSIZE *  pEnd,
   int      fileNum,
   int      bitpix,
   DSIZE *  pNVal,
   uchar *  pData)
{
   int      iloop;
   int      ii;
   int      ipos;
   int      size;
   DSIZE    nskip;
   DSIZE    nread;
   FILE  *  pFILEin;

   pFILEin = pFILEfits[fileNum];
   size = fits_size_from_bitpix_(&bitpix);

   /* Skip "nskip" points */
   nskip = pStart[nel-1];
   for (ii=0; ii < nel-1; ii++) nskip = nskip * pNaxis[ii];
   ipos = ftell(pFILEin);
   fseek(pFILEin, (ipos + size*nskip), 0);

   if (nel > 1) {
      for (iloop=0; iloop < pEnd[nel-1]-pStart[nel-1]+1; iloop++)
       fits_read_subimg1(nel-1, pNaxis, pStart, pEnd, fileNum, bitpix,
        pNVal, pData);
   } else {
      nread = pEnd[0]-pStart[0]+1;

      /* Read in "nread" points */
      *pNVal += (int)fread(&pData[(*pNVal)*size], size, nread, pFILEin);
   }

   /* Skip "nskip" points */
   nskip = pNaxis[nel-1] - pEnd[nel-1] - 1;
   for (ii=0; ii < nel-1; ii++) nskip = nskip * pNaxis[ii];
   ipos = ftell(pFILEin);
   fseek(pFILEin, (ipos + size*nskip), 0);
}

/******************************************************************************/
/*
 * Read in one element from a FITS format data file, indexed by the
 * values in pLoc.
 *
 * The header is assumed to already be read using
 * fits_read_file_fits_header_only_(), to avoid reading it upon every
 * call to this routine.  The axis dimensions and BITPIX are read from
 * the header that is passed.  The dimensions of pLoc must agree with
 * the dimensions specified by NAXIS in this header.
 *
 * The data value is rescaled to a 32-bit real.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than requested (1), in which case the difference (1) is returned.
 */

DSIZE fits_read_point_
  (char     pFileName[],
   HSIZE    nHead,
   uchar *  pHead,
   DSIZE *  pLoc,
   float *  pValue)
{
   int      bitpix;
   DSIZE    iloc;
   int      nmult;
   int      size;
   MEMSZ    memSize;
   int      iAxis;
   int      numAxes;
   DSIZE *  pNaxis;
   float    bscale;
   float    bzero;
   uchar *  pData;
   DSIZE    retval;

   int      fileNum;
   int      ipos;
   char     pPrivR[] = "r\0";
   FILE  *  pFILEin;

   inoutput_open_file(&fileNum, pFileName, pPrivR);

   /* Skip header */
   fits_skip_header_(&fileNum);

   /* From the given header, read BITPIX and PNAXIS */
   fits_get_card_ival_(&bitpix, label_bitpix, &nHead, &pHead);
   fits_compute_axes_(&nHead, &pHead, &numAxes, &pNaxis);

   /* Find the 1-dimensional index for the data point requested */
   iloc = 0;
   nmult = 1;
   for (iAxis=0; iAxis < numAxes; iAxis++) {
      iloc = iloc + pLoc[iAxis] * nmult;
      nmult = nmult * pNaxis[iAxis];
   }

   /* Read one element from the data file */
   pFILEin = pFILEfits[fileNum];
   ipos = ftell(pFILEin);
   size = fits_size_from_bitpix_(&bitpix);
   memSize = size;
   ccalloc_(&memSize, (void **)&pData);
   fseek(pFILEin, (ipos + size*iloc), 0);
   retval = 1 - (int)fread(pData, size, 1, pFILEin);
#ifdef LITTLE_ENDIAN
   fits_byteswap(bitpix, 1, pData);
#endif

   /* Convert data to real*4 if not already */
   if (bitpix == -32) {
      *pValue = *( (float *)pData );
   } else {
      /* Get the scaling parameters from the header */
      if (fits_get_card_rval_(&bscale, (uchar *)Label_bscale, &nHead, &pHead)
       == FALSE_MWDUST) {
         bscale = 1.0;  /* Default value for BSCALE */
      }
      if (fits_get_card_rval_(&bzero , (uchar *)Label_bzero , &nHead, &pHead)
       == FALSE_MWDUST) {
         bzero = 0.0;  /* Default value for BZERO */
      }

      iloc = 0;
      *pValue = fits_get_rval_(&iloc, &bitpix, &bscale, &bzero, &pData);
   }

   inoutput_close_file(fileNum);

   /* Plug a memory leak - D. Schlegel 06-Feb-1999 */
   fits_free_axes_(&numAxes, &pNaxis);

   return retval;
}

/******************************************************************************/
/*
 * Read in FITS format data.  Assume the header is a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows as either real values or as integer values
 * that should be scaled by the BZERO and BSCALE values.  The data
 * format is determined by the BITPIX card in the header.
 * Memory is dynamically allocated for the header and data arrays.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than indicated in the header, in which case the difference is returned.
 */
DSIZE fits_read_file_fits_noscale_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   int   *  pBitpix,
   uchar ** ppData)
{
   int      fileNum;
   DSIZE    retval;
   char     pPrivR[] = "r\0";

   inoutput_open_file(&fileNum, pFileName, pPrivR);

   /* Read header */
   fits_read_fits_header_(&fileNum, pNHead, ppHead);

   /* From the header, read BITPIX and determine the number of data points */
   *pNData = fits_compute_ndata_(pNHead, ppHead);
   fits_get_card_ival_(pBitpix, label_bitpix, pNHead, ppHead);

   /* Read data */
   retval = fits_read_fits_data_(&fileNum, pBitpix, pNData, ppData);

   inoutput_close_file(fileNum);
   return retval;
}

/******************************************************************************/
/*
 * Read in EXTENDED FITS format data.  Assume the header is a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * In addition, allow for an extended header file.  (But always reads
 * exactly one additional header.)
 * The data follows as either real values or as integer values
 * that should be scaled by the BZERO and BSCALE values.  The data
 * format is determined by the BITPIX card in the header.
 * Memory is dynamically allocated for the header and data arrays.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than indicated in the header, in which case the difference is returned.
 */
DSIZE fits_read_file_xfits_noscale_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   HSIZE *  pNXhead,
   uchar ** ppXHead,
   DSIZE *  pNData,
   int   *  pBitpix,
   uchar ** ppData)
{

   int      fileNum;
   HSIZE    iCard;
   HSIZE *  pTempN;
   DSIZE    retval;
   uchar    pExtend[40];
   uchar *  pTempHead;
   char     pPrivR[] = "r\0";

   inoutput_open_file(&fileNum, pFileName, pPrivR);

   /* Read header */
   fits_read_fits_header_(&fileNum, pNHead, ppHead);

   /* Read extended header(if it exists) */
   pTempN = pNHead;
   pTempHead = *ppHead;
   iCard = fits_find_card_(label_extend, pNHead, ppHead);
   if (iCard < *pNHead)
     { sscanf( (const char*)&(*ppHead)[iCard*80+10], "%s", pExtend); }

   if (strcmp((const char*)pExtend, (const char *)text_T) == 0) {
      fits_read_fits_header_(&fileNum, pNXhead, ppXHead);
      pTempN = pNXhead;
      pTempHead = *ppXHead;
   }

   /* From the header, read BITPIX and determine the number of data points */
   /* (from the extended header if it exists) */
   *pNData = fits_compute_ndata_(pNHead, ppHead);
   fits_get_card_ival_(pBitpix, label_bitpix, pNXhead, ppXHead);

   /* Read data */
   retval = fits_read_fits_data_(&fileNum, pBitpix, pNData, ppData);

   inoutput_close_file(fileNum);
   return retval;
}

/******************************************************************************/
/*
 * Write FITS format data.  Write the header as a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows is written as 32-bit reals.  If necessary, the
 * data is converted to that format first.
 *
 * Returned value is 0 unless not all of the data points are written,
 * in which case the difference is returned.  (Does not work!!!???  Values
 * returned from fwrite() are bizarre!)
 */

DSIZE fits_write_file_fits_r4_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   float ** ppData)
{
   int      bitpix = -32;
   DSIZE    retval;

   fits_data_to_r4_(pNHead, ppHead, pNData, (uchar **)ppData);
   retval = fits_write_file_fits_noscale_(pFileName, pNHead,
    ppHead, pNData, &bitpix, (uchar **)ppData);
   return retval;
}

/******************************************************************************/
/*
 * Write FITS format data.  Write the header as a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows is written as 16-bit integers.  If necessary, the
 * data is converted to that format first.
 *
 * Returned value is 0 unless not all of the data points are written,
 * in which case the difference is returned.  (Does not work!!!???  Values
 * returned from fwrite() are bizarre!)
 */

DSIZE fits_write_file_fits_i2_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   short int ** ppData)
{
   int      bitpix = 16;
   DSIZE    retval;

   fits_data_to_i2_(pNHead, ppHead, pNData, (uchar **)ppData);
   retval = fits_write_file_fits_noscale_(pFileName, pNHead,
    ppHead, pNData, &bitpix, (uchar **)ppData);
   return retval;
}

/******************************************************************************/
/*
 * Write FITS format data.  Write the header as a multiple of
 * 2880-byte blocks, with the last block containing an END card.
 * The data follows as either real values or as integer values
 * that should be scaled by the BZERO and BSCALE values.  The data
 * format is determined by the BITPIX card in the header.
 *
 * Returned value is 0 unless not all of the data points are written,
 * in which case the difference is returned.  (Does not work!!!???  Values
 * returned from fwrite() are bizarre!)
 */

DSIZE fits_write_file_fits_noscale_
  (char     pFileName[],
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   int   *  pBitpix,
   uchar ** ppData)
{
   int      fileNum;
   DSIZE    retval;
   char     pPrivW[] = "w\0";

   inoutput_open_file(&fileNum,pFileName, pPrivW);

   /* Write header */
   fits_write_fits_header_(&fileNum, pNHead, ppHead);

   /* Write data */
   retval = fits_write_fits_data_(&fileNum, pBitpix, pNData, ppData);

   inoutput_close_file(fileNum);
   return retval;
}

/******************************************************************************/
/*
 * Read data blocks from an open FITS file.
 * One contiguous area of memory is dynamically allocated.
 *
 * Returned value is 0 unless the FITS file contains fewer data points
 * than indicated in the header, in which case the difference is returned.
 */
DSIZE fits_read_fits_data_
  (int   *  pFilenum,
   int   *  pBitpix,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      size;
   DSIZE    retval;

   /* Allocate the minimum number of 2880-byte blocks for the data */
   fits_create_fits_data_(pBitpix, pNData, ppData);

   /* Read the data until the number of data points or until the end
      of file is reached. */
   size = fits_size_from_bitpix_(pBitpix);
   retval = *pNData - (int)fread(*ppData, size, *pNData, pFILEfits[*pFilenum]);
#ifdef LITTLE_ENDIAN
   fits_byteswap(*pBitpix, *pNData, *ppData);
#endif

   return retval;
}

/******************************************************************************/
/*
 * Write data blocks to an open FITS file.
 *
 * Returned value is 0 unless not all of the data points are written,
 * in which case the difference is returned.  (Does not work!  Values
 * returned from fwrite() are bizarre!)
 */
DSIZE fits_write_fits_data_
  (int   *  pFilenum,
   int   *  pBitpix,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      i;
   int      j;
   int      k;
   int      size;
   int      retval;

   /* Write the number of data points indicated */
   size = fits_size_from_bitpix_(pBitpix);
#ifdef LITTLE_ENDIAN
   fits_byteswap(*pBitpix, *pNData, *ppData);
#endif
   retval = *pNData - (int)fwrite(*ppData, size, *pNData, pFILEfits[*pFilenum]);
#ifdef LITTLE_ENDIAN
   fits_byteswap(*pBitpix, *pNData, *ppData);
#endif
 
   /* Write some zeros such that the data takes up an integral number
      of 2880 byte blocks */
   j = (ftell(pFILEfits[*pFilenum]) % 2880)/size ;
   if (j != 0) {
      k = 1;
      for (i=j; i<(2880/size); i++)
       fwrite(datum_zero, size, k, pFILEfits[*pFilenum]);
   }

   return retval;
}

/******************************************************************************/
/*
 * Read header blocks from an open FITS file.
 * Memory for new blocks are dynamically allocated when needed.
 */
void fits_read_fits_header_
  (int   *  pFilenum,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   HSIZE    jCard;
   uchar    pCard[80];

   /* Read the header into memory until the END card */
   *pNHead = 0;
   while (fits_get_next_card_(pFilenum, pCard)) {
      /* Only include this card if it is not blank */
      if (strncmp((const char *)pCard, (const char *)card_empty, 80) != 0) {
         fits_add_card_(pCard, pNHead, ppHead);
      }
   }
   fits_add_card_(card_end, pNHead, ppHead);
 
   /* Finish reading to the end of the last header block (the one w/END) */
   /* ignoring, and in effect deleting, any header cards after the END card */
   jCard = (ftell(pFILEfits[*pFilenum]) % 2880)/80 ;
   if (jCard != 0) {
      for (iCard=jCard; iCard<=35; iCard++) {
         fits_get_next_card_(pFilenum, pCard);
      }
   }

   /* Delete all cards where the label is blank */
   fits_purge_blank_cards_(pNHead, ppHead);

   /* Add missing cards to the FITS header */
   fits_add_required_cards_(pNHead, ppHead);
}

/******************************************************************************/
/*
 * Skip header blocks from an open FITS file.
 * This is a modified version of fits_read_fits_header_().
 */
void fits_skip_header_
  (int   *  pFilenum)
{
   HSIZE    iCard;
   HSIZE    jCard;
   uchar    pCard[80];

   /* Read the header into memory until the END card */
   while (fits_get_next_card_(pFilenum, pCard));
 
   /* Finish reading to the end of the last header block (the one w/END) */
   jCard = (ftell(pFILEfits[*pFilenum]) % 2880)/80 ;
   if (jCard != 0) {
      for (iCard=jCard; iCard<=35; iCard++) {
         fits_get_next_card_(pFilenum, pCard);
      }
   }
}

/******************************************************************************/
/*
 * Add any cards to the header that are required by the FITS definition
 * but are missing.
 */
void fits_add_required_cards_
  (HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iAxis;
   int      numAxes;
   int      naxis;
   int      naxisX;
#if 0
   int      crpixX;
   float    crvalX;
   float    cdeltX;
#endif
   DSIZE *  pNaxis;
   uchar    pLabel_temp[9]; /* Must be long enough for 8 chars + NULL */

   if (fits_get_card_ival_(&naxis, label_naxis, pNHead, ppHead) == FALSE_MWDUST) {
      naxis = 0; /* default to no data axes */
      fits_change_card_ival_(&naxis, label_naxis, pNHead, ppHead);
   }

   fits_compute_axes_(pNHead, ppHead, &numAxes, &pNaxis);

   for (iAxis=0; iAxis < numAxes; iAxis++) {
      /* For each axis, be sure that a NAXISx, CRPIXx, CRVALx and CDELTx
       * card exists.  If one does not exist, then create it.
       * Create the labels for each axis for which to look as pLabel_temp.
       * Be certain to pad with spaces so that a NULL is not written.
       */

      sprintf((char *)pLabel_temp, "NAXIS%-3d", iAxis+1);
      if (fits_get_card_ival_(&naxisX, pLabel_temp, pNHead, ppHead) == FALSE_MWDUST) {
         naxisX = 1; /* default to 1 */
         fits_change_card_ival_(&naxisX, pLabel_temp, pNHead, ppHead);
         printf("Adding a card %s\n", pLabel_temp);
      }

#if 0
      sprintf(pLabel_temp, "CRPIX%-3d  ", iAxis+1);
      if (fits_get_card_ival_(&crpixX, pLabel_temp, pNHead, ppHead) == FALSE_MWDUST) {
         crpixX = 1; /* default to start numbering at the first pixel */
         fits_change_card_ival_(&crpixX, pLabel_temp, pNHead, ppHead);
         printf("Adding a card %s\n", pLabel_temp);
      }

      sprintf(pLabel_temp, "CRVAL%-3d  ", iAxis+1);
      if (fits_get_card_rval_(&crvalX, pLabel_temp, pNHead, ppHead) == FALSE_MWDUST) {
         crvalX = 0.0; /* default to the first pixel value to be zero */
         fits_change_card_rval_(&crvalX, pLabel_temp, pNHead, ppHead);
         printf("Adding a card %s\n", pLabel_temp);
      }

      sprintf(pLabel_temp, "CDELT%-3d  ", iAxis+1);
      if (fits_get_card_rval_(&cdeltX, pLabel_temp, pNHead, ppHead) == FALSE_MWDUST) {
         cdeltX = 1.0; /* default to spacing each pixel by a value of 1 */
         fits_change_card_rval_(&cdeltX, pLabel_temp, pNHead, ppHead);
         printf("Adding a card %s\n", pLabel_temp);
      }
#endif
   }

   /* Plug a memory leak - Chris Stoughton 19-Jan-1999 */
   fits_free_axes_(&numAxes, &pNaxis);
}

/******************************************************************************/
/*
 * Write header blocks to an open FITS file.
 */
void fits_write_fits_header_
  (int   *  pFilenum,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iCard;
   int      jCard;
   uchar *  pHead = *ppHead;

   /* Write the number of header cards indicated */
   for (iCard=0; iCard < *pNHead; iCard++) {
      fits_put_next_card_(pFilenum, &pHead[iCard*80]);
   }
 
   /* Write some more blank cards such that the header takes up an
      integral number of 2880 byte blocks */
   jCard = (ftell(pFILEfits[*pFilenum]) % 2880)/80 ;
   if (jCard != 0) {
      for (iCard=jCard; iCard <= 35; iCard++) {
         fits_put_next_card_(pFilenum, card_empty);
      }
   }
}

/******************************************************************************/
/*
 * Create a FITS header that only contains an END card.
 * Memory for new blocks are dynamically allocated when needed.
 */
void fits_create_fits_header_
  (HSIZE *  pNHead,
   uchar ** ppHead)
{
   /* First dispose of any memory already allocated by ppHead. */
   fits_dispose_array_(ppHead);

   /* Create a header with only a SIMPLE and END card.
    * Note that an entire 2880 byte block will be created
    * by the call to fits_add_card_().
    */
   *pNHead = 0;
   fits_add_card_(card_end, pNHead, ppHead);
   fits_add_card_(card_simple, pNHead, ppHead);
}

/******************************************************************************/
/*
 * Copy a FITS header into a newly created array.  Dynamically allocate
 * memory for the new header.
 */
void fits_duplicate_fits_header_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   uchar ** ppHeadCopy)
{
   MEMSZ    memSize;
 
   /* Allocate the minimum number of 2880-byte blocks for the header */
   memSize = ((int)((80*(*pNHead)-1)/2880) + 1) * 2880;
   ccalloc_(&memSize, (void **)ppHeadCopy);

   /* Copy all of the header bytes verbatim */
   memmove((void *)(*ppHeadCopy), (const void *)(*ppHead), memSize);
}

/******************************************************************************/
/*
 * Copy a FITS data array of real*4 into a newly created array.
 */
void fits_duplicate_fits_data_r4_
  (DSIZE *  pNData,
   float ** ppData,
   float ** ppDataCopy)
{
   int      bitpix = -32;

   fits_duplicate_fits_data_(&bitpix, pNData, (uchar **)ppData,
    (uchar **)ppDataCopy);
}

/******************************************************************************/
/*
 * Copy a FITS data array into a newly created array.
 */
void fits_duplicate_fits_data_
  (int   *  pBitpix,
   DSIZE *  pNData,
   uchar ** ppData,
   uchar ** ppDataCopy)
{
   int      size;
   MEMSZ    memSize;
 
   /* Allocate the minimum number of 2880-byte blocks for the header */
   size = fits_size_from_bitpix_(pBitpix);
   memSize = ((int)((size*(*pNData)-1)/2880) + 1) * 2880;
   ccalloc_(&memSize, (void **)ppDataCopy);

   /* Copy all of the data bytes verbatim */
   memmove((void *)(*ppDataCopy), (const void *)(*ppData), memSize);
}

/******************************************************************************/
/*
 * Create a FITS data array of real*4 with the number of elements specified.
 * Memory for new blocks are dynamically allocated when needed.
 */
void fits_create_fits_data_r4_
  (DSIZE *  pNData,
   float ** ppData)
{
   int      bitpix = -32;
   fits_create_fits_data_(&bitpix, pNData, (uchar **)ppData);
}

/******************************************************************************/
/*
 * Create a FITS data array with the number of elements specified.
 * Memory for new blocks are dynamically allocated when needed.
 */
void fits_create_fits_data_
  (int   *  pBitpix,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      size;
   MEMSZ    memSize;

   /* Allocate the minimum number of 2880-byte blocks for the data */
   size = fits_size_from_bitpix_(pBitpix);
   memSize = ((int)((size*(*pNData)-1)/2880) + 1) * 2880;
   ccalloc_(&memSize, (void **)ppData);
}

/******************************************************************************/
/*
 * Free the memory allocated for the header and data arrays.
 * Return TRUE_MWDUST if both arrays existed and were freed, and FALSE_MWDUST otherwise.
 */
int fits_dispose_header_and_data_
  (uchar ** ppHead,
   uchar ** ppData)
{
   int      retval;

   retval = fits_dispose_array_(ppHead) && fits_dispose_array_(ppData);
   return retval;
}

/******************************************************************************/
/*
 * Free the memory allocated for a FITS header or data array.
 * Return TRUE_MWDUST if the array existed and was freed, and FALSE_MWDUST otherwise.
 */
int fits_dispose_array_
  (uchar ** ppHeadOrData)
{
   int      retval;

   retval = FALSE_MWDUST;
   if (*ppHeadOrData != NULL) {
      ccfree_((void **)ppHeadOrData);
      retval = TRUE_MWDUST;
   }
   return retval;
}

/******************************************************************************/
/*
 * Compute the total number of data points.
 * This information is determined from the header cards NAXIS and NAXISx.
 */
DSIZE fits_compute_ndata_
  (HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      numAxes;
   DSIZE    iAxis;
   DSIZE *  pNaxis;
   DSIZE    nData;

   fits_compute_axes_(pNHead, ppHead, &numAxes, &pNaxis);
   if (numAxes == 0)
      nData = 0;
   else {
      nData = 1;
      for (iAxis=0; iAxis < numAxes; iAxis++) nData *= pNaxis[iAxis];
   }

   /* Plug a memory leak - D. Schlegel 06-Feb-1999 */
   fits_free_axes_(&numAxes, &pNaxis);

   return nData;
}

/******************************************************************************/
/*
 * Compute the number of axes and the dimension of each axis.
 * This information is determined from the header cards NAXIS and NAXISx.
 */
void fits_compute_axes_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   int   *  pNumAxes,
   DSIZE ** ppNaxis)
{
   int      iAxis;
   int      ival;
   DSIZE *  pNaxis;
   MEMSZ    memSize;
   uchar    pLabel_temp[9];

   fits_get_card_ival_(pNumAxes, label_naxis, pNHead, ppHead);
   if (*pNumAxes > 0) {
      memSize = (*pNumAxes) * sizeof(DSIZE);
      ccalloc_(&memSize, (void **)ppNaxis);
      pNaxis = *ppNaxis;
      for (iAxis=0; iAxis < *pNumAxes; iAxis++) {
         /* Create the label for this axis for which to look.
          * Be certain to pad with spaces so that a NULL is not written.
          */
         sprintf((char *)pLabel_temp, "NAXIS%d  ", iAxis+1);
         fits_get_card_ival_(&ival, pLabel_temp, pNHead, ppHead);
         pNaxis[iAxis] = ival;
      }
   }
}
       
/******************************************************************************/
/*
 * Free memory for axes dimensions allocated by "fits_compute_axes_".
 */
void fits_free_axes_
  (int   *  pNumAxes,
   DSIZE ** ppNaxis)
{
   if (*pNumAxes > 0) {
      ccfree_((void **)ppNaxis);
   }
}

/******************************************************************************/
/*
 * Compute the wavelength for a given pixel number using Vista coefficients.
 * This must be preceded with a call to fits_compute_vista_poly_coeffs_
 * to find the polynomial coefficients from the header cards.
 * The first element of pCoeff is a central pixel number for the fit
 * and the remaining LAMORD elements are the polynomial coefficients.
 * The wavelength of pixel number iPix (zero-indexed):
 *   wavelength(iPix) = SUM{j=1,nCoeff-1} Coeff(j) * [iPix - Coeff(0)]**(j-1)
 */
float compute_vista_wavelength_
  (DSIZE *  pPixelNumber,
   int   *  pNCoeff,
   float ** ppCoeff)
{
   int      iCoeff;
   DSIZE    centralPixelNumber;
   float    wavelength;

   centralPixelNumber = (DSIZE)(*ppCoeff)[0];
   wavelength = 0.0;
   for (iCoeff=1; iCoeff < *pNCoeff; iCoeff++) {
      wavelength += (*ppCoeff)[iCoeff]
       * pow(*pPixelNumber - centralPixelNumber, (float)(iCoeff-1));
   }
   return wavelength;
}

/******************************************************************************/
/*
 * Compute the number of coefficients for a polynomial wavelength fit
 * and the values of those coefficients.
 * This information is determined from the header cards LAMORD and LPOLYx.
 * Set nCoeff=LAMORD+1, and an array ppCoeff is created that has LAMORD+1
 * elements.  The first element is a central pixel number for the fit
 * and the remaining LAMORD elements are the polynomial coefficients.
 * The wavelength of pixel number iPix (zero-indexed):
 *   wavelength(iPix) = SUM{j=1,nCoeff-1} Coeff(j) * [iPix - Coeff(0)]**(j-1)
 * The coefficients are stored 4 on a line, so that the LPOLY0 card
 * contains up to the first 4 coefficients, and LPOLY1 up to the next 4, etc.
 */
void fits_compute_vista_poly_coeffs_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   int   *  pNCoeff,
   float ** ppCoeff)
{
   int      iCoeff;
   int      iLpolyNum;
   int      nLpolyNum;
   MEMSZ    memSize;
   uchar    pLabel_temp[9]; /* Must be long enough for 8 chars + NULL */
   char  *  pStringVal;
   char  *  pCardLoc;
   const char pCharSpace[] = " \'";

   fits_get_card_ival_(pNCoeff,label_lamord,pNHead,ppHead);
   if (*pNCoeff > 0) {
      (*pNCoeff)++;
      memSize = (*pNCoeff) * sizeof(float);
      ccalloc_(&memSize, (void **)ppCoeff);
      nLpolyNum = (*pNCoeff+3) / 4;
      iCoeff = 0;
      for (iLpolyNum=0; iLpolyNum < nLpolyNum; iLpolyNum++) {
         /* Create the label for this coefficient for which to look.
          * Be certain to pad with spaces so that a NULL is not written.
          */
         sprintf((char *)pLabel_temp, "LPOLY%-3d  ", iLpolyNum);
         fits_get_card_string_(&pStringVal, pLabel_temp, pNHead, ppHead);
         pCardLoc = pStringVal;
         for (iCoeff=iLpolyNum*4; iCoeff < min(iLpolyNum*4+4, *pNCoeff);
          iCoeff++) {
            sscanf(strtok(pCardLoc,pCharSpace), "%f", &(*ppCoeff)[iCoeff]);
            pCardLoc=NULL;
         }
      }
   }
}
       
/******************************************************************************/
/* 
 * Convert a data array to real*4 data, if it is not already.
 * A new array is created for the data, and the old array is discarded.
 * Change the BITPIX card in the header to -32 to indicate the data is real*4.
 */
void fits_data_to_r4_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      bitpix;
   int      newBitpix;
   DSIZE    iData;
   HSIZE    iCard;
   MEMSZ    memSize;
   float    bscale;
   float    bzero;
   float    blankval;
   float    newBlankval;
   float *  pNewData;

   fits_get_card_ival_(&bitpix, label_bitpix, pNHead, ppHead);

   /* Convert data to real*4 if not already */
   if (bitpix != -32) {

      /* Get the scaling parameters from the header */
      if (fits_get_card_rval_(&bscale, (uchar *)Label_bscale, pNHead, ppHead)
       == FALSE_MWDUST) {
         bscale = 1.0;  /* Default value for BSCALE */
      }
      if (fits_get_card_rval_(&bzero , (uchar *)Label_bzero , pNHead, ppHead)
       == FALSE_MWDUST) {
         bzero = 0.0;  /* Default value for BZERO */
      }

      /* Allocate the minimum number of 2880-byte blocks for the data */
      memSize = ((int)((4*(*pNData)-1)/2880) + 1) * 2880;
      ccalloc_(&memSize, (void **)&pNewData);

      /* Convert the data and write to the new array */
      /* Note that nothing is done to rescale BLANK values properly */
      for (iData=0; iData < *pNData; iData++) {
         pNewData[iData] =
          fits_get_rval_(&iData, &bitpix, &bscale, &bzero, ppData);
      }

      /* Free the memory from the old array, and change the ppData pointer
         to point to the new array */
      ccfree_((void **)ppData);
      *ppData = (uchar *)pNewData;

      /* Change the BITPIX card to -32, indicating the data is real*4 */
      newBitpix = -32;
      fits_change_card_ival_(&newBitpix, label_bitpix, pNHead, ppHead);

      /* Delete the BSCALE and BZERO cards which are no longer used */
      fits_delete_card_(label_bscale, pNHead, ppHead);
      fits_delete_card_(label_bzero , pNHead, ppHead);

      /* Rescale the BLANK card if it exists */
      if ((iCard = fits_find_card_(label_blank, pNHead, ppHead)) != *pNHead) {
         fits_get_card_rval_(&blankval, label_blank, pNHead, ppHead);
         if      (bitpix ==  8) newBlankval = blankval * bscale + bzero;
         else if (bitpix == 16) newBlankval = blankval * bscale + bzero;
         else if (bitpix == 32) newBlankval = blankval * bscale + bzero;
         else if (bitpix == -8) newBlankval = blankval;
         else if (bitpix ==-32) newBlankval = blankval;
         else if (bitpix ==-64) newBlankval = blankval;
         fits_change_card_rval_(&newBlankval, label_blank, pNHead, ppHead);
      }

   }
}

/******************************************************************************/
/* 
 * Convert a data array to integer*2 data, if it is not already.
 * A new array is created for the data, and the old array is discarded.
 * Change the BITPIX card in the header to 16 to indicate the data is integer*2.
 */
void fits_data_to_i2_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      bitpix;
   int      newBitpix;
   DSIZE    iData;
   HSIZE    iCard;
   MEMSZ    memSize;
   float    bscale;
   float    bzero;
   float    blankval;
   float    newBlankval;
   short int *  pNewData;

   fits_get_card_ival_(&bitpix, label_bitpix, pNHead, ppHead);

   /* Convert data to integer*2 if not already */
   if (bitpix != 16) {

      /* Get the scaling parameters from the header */
      if (fits_get_card_rval_(&bscale, (uchar *)Label_bscale, pNHead, ppHead)
       == FALSE_MWDUST) {
         bscale = 1.0;  /* Default value for BSCALE */
      }
      if (fits_get_card_rval_(&bzero , (uchar *)Label_bzero , pNHead, ppHead)
       == FALSE_MWDUST) {
         bzero = 0.0;  /* Default value for BZERO */
      }

      /* Allocate the minimum number of 2880-byte blocks for the data */
      memSize = ((int)((2*(*pNData)-1)/2880) + 1) * 2880;
      ccalloc_(&memSize, (void **)&pNewData);

      /* Convert the data and write to the new array */
      /* Note that nothing is done to rescale BLANK values properly */
      for (iData=0; iData < *pNData; iData++) {
         pNewData[iData] =
          fits_get_ival_(&iData, &bitpix, &bscale, &bzero, ppData);
      }

      /* Free the memory from the old array, and change the ppData pointer
         to point to the new array */
      ccfree_((void **)ppData);
      *ppData = (uchar *)pNewData;

      /* Change the BITPIX card to 16, indicating the data is integer*2 */
      newBitpix = 16;
      fits_change_card_ival_(&newBitpix, label_bitpix, pNHead, ppHead);

      /* Delete the BSCALE and BZERO cards which are no longer used */
      fits_delete_card_(label_bscale, pNHead, ppHead);
      fits_delete_card_(label_bzero , pNHead, ppHead);

      /* Rescale the BLANK card if it exists */
      if ((iCard = fits_find_card_(label_blank, pNHead, ppHead)) != *pNHead) {
         fits_get_card_rval_(&blankval, label_blank, pNHead, ppHead);
         if      (bitpix ==  8) newBlankval = blankval * bscale + bzero;
         else if (bitpix == 16) newBlankval = blankval * bscale + bzero;
         else if (bitpix == 32) newBlankval = blankval * bscale + bzero;
         else if (bitpix == -8) newBlankval = blankval;
         else if (bitpix ==-32) newBlankval = blankval;
         else if (bitpix ==-64) newBlankval = blankval;
         fits_change_card_rval_(&newBlankval, label_blank, pNHead, ppHead);
      }

   }
}

/******************************************************************************/
/*
 * Add a card immediately before the END card, or as the next card
 * (if no blank or END card), whichever comes first.  Return the card
 * number of the added card.
 * Memory is dynamically allocated if necessary by adding another 2880-byte
 * block.
 */
HSIZE fits_add_card_
  (uchar    pCard[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    numCardEnd;
   MEMSZ    memSize;
   uchar    pCardTemp[80];
   uchar *  pNewHeader;

   fits_string_to_card_(pCard, pCardTemp);

   numCardEnd=fits_find_card_(card_end, pNHead, ppHead);

   /* Test to see if more memory is needed for the header */
   if ((*pNHead)%36 == 0) {
      /* Copy header to new location and change appropriate pointers */
      memSize = (36+(*pNHead)) * 80;
      ccalloc_(&memSize, (void **)&pNewHeader);
      if (*pNHead > 0) {
         memmove(pNewHeader, *ppHead, (*pNHead)*80);
         ccfree_((void **)ppHead);
      }
      *ppHead = pNewHeader;
      numCardEnd += (pNewHeader - *ppHead);
   }

   if ((*pNHead > 0) && (numCardEnd<*pNHead) ) {
      /* Copy the end card forward 80 bytes in memory */
      memmove(&(*ppHead)[(numCardEnd+1)*80], &(*ppHead)[numCardEnd*80], 80);
      /* Add the new card where the END card had been */
      memmove(&(*ppHead)[numCardEnd*80], pCardTemp, 80);
      (*pNHead)++;
      return numCardEnd;
   }
   else {
      /* There is no end card, so simply add the new card at end of header */
      memmove(&(*ppHead)[(*pNHead)*80], pCardTemp, 80);
      return (*pNHead)++;
   }
}

/******************************************************************************/
/*
 * Add a card in the first card with a blank label or immediately before
 * the END card, or as the next card (if no blank or END card), whichever
 * comes first.  Return the card number of the added card.
 * Memory is dynamically allocated if necessary.
 */
HSIZE fits_add_cardblank_
  (uchar    pCard[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    numCardEmpty;
   HSIZE    numCardEnd;
   MEMSZ    memSize;
   uchar *  pHead = *ppHead;
   uchar *  pNewHeader;

   numCardEmpty = fits_find_card_(card_empty, pNHead, ppHead);
   numCardEnd   = fits_find_card_(card_end  , pNHead, ppHead);

   /* First case finds a blank card before the end card that is overwritten  */
   if ((*pNHead > 0) && (numCardEmpty < numCardEnd)) {
      memmove(&pHead[numCardEmpty*80], pCard, 80);
      return numCardEmpty;
   }
   else {
      /* Test to see if more memory is needed for the header */
      if ((*pNHead)%36 == 0) {
         /* Copy header to new location and change appropriate pointers */
         memSize = (36+(*pNHead)) * 80;
         ccalloc_(&memSize, (void **)&pNewHeader);
         memmove(pNewHeader, pHead, (*pNHead)*80);
         ccfree_((void **)&pHead);
         pHead = pNewHeader;
         numCardEmpty += (pNewHeader-pHead);
         numCardEnd   += (pNewHeader-pHead);
      }
      if ((*pNHead > 0) && (numCardEnd < *pNHead) ) {
         /* Copy the end card forward 80 bytes in memory */
         memmove(&pHead[(numCardEnd+1)*80], &pHead[numCardEnd*80], 80);
         /* Add the new card where the END card had been */
         memmove(&pHead[numCardEnd*80], pCard, 80);
         (*pNHead)++;
         return numCardEnd;
      } else {
         /* There is no end card, so simply add the new card at end of header */
         memmove(&pHead[(*pNHead)*80], pCard, 80);
         return (*pNHead)++;
      }
   }
}

/******************************************************************************/
/*
 * Create a new card with the given label and integer value.
 * Note that this can create multiple cards with the same label.
 * Return the card number of the new card.
 */
HSIZE fits_add_card_ival_
  (int   *  pIval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   sprintf((char *)pCardTemp, "%-8.8s= %20d", pLabel, *pIval);
   iCard = fits_add_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Create a new card with the given label and real value.
 * Note that this can create multiple cards with the same label.
 * Return the card number of the new card.
 */
HSIZE fits_add_card_rval_
  (float *  pRval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   sprintf((char *)pCardTemp, "%-8.8s= %20.7e", pLabel, *pRval);
   iCard = fits_add_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Create a new card with the given label and string value.
 * Note that this can create multiple cards with the same label.
 * Return the card number of the new card.
 */
HSIZE fits_add_card_string_
  (char  *  pStringVal,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   /* !!!??? NOTE: A QUOTE SHOULD BE WRITTEN AS 2 SINGLE QUOTES */
   sprintf((char *)pCardTemp, "%-8.8s= '%-1.68s'", pLabel, pStringVal);
   iCard = fits_add_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Create a new COMMENT card with the given string.
 * Note that this can create multiple cards with the same label.
 * Return the card number of the new card.
 */
HSIZE fits_add_card_comment_
  (char  *  pStringVal,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   /* !!!??? NOTE: A QUOTE SHOULD BE WRITTEN AS 2 SINGLE QUOTES */
   sprintf((char *)pCardTemp, "COMMENT %-1.72s", pStringVal);
   iCard = fits_add_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Create a new HISTORY card with the given string.
 * Note that this can create multiple cards with the same label.
 * Return the card number of the new card.
 */
HSIZE fits_add_card_history_
  (char  *  pStringVal,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   /* !!!??? NOTE: A QUOTE SHOULD BE WRITTEN AS 2 SINGLE QUOTES */
   sprintf((char *)pCardTemp, "HISTORY %-1.72s", pStringVal);
   iCard = fits_add_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Delete all cards where the label is blank.
 * Return the number of cards that were discarded.
 */
HSIZE fits_purge_blank_cards_
  (HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    numDelete;

   numDelete = 0;
   while (fits_delete_card_(label_empty, pNHead, ppHead) != *pNHead) {
      numDelete++;
   }

   return numDelete;
}

/******************************************************************************/
/*
 * Delete the first card that matches the given label, or do nothing if no
 * matches are found.  Return the card number of the deleted card,
 * or return nHead (out of range) if no match was found.
 */
HSIZE fits_delete_card_
  (uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   HSIZE    jCard;
   uchar *  pHead = *ppHead;

   iCard = fits_find_card_(pLabel, pNHead, ppHead);
   if (iCard < *pNHead) {
      (*pNHead)--;
      for (jCard=iCard; jCard <* pNHead; jCard++) {
         memmove(&pHead[jCard*80], &pHead[(jCard+1)*80], 80);
      }
      memmove(&pHead[jCard*80], card_empty, 80);
   }
   return iCard;
}

/******************************************************************************/
/*
 * Return the card number of the 1st header card with the label passed,
 * or return nHead (out of range) if no match was found.
 */
HSIZE fits_find_card_
  (uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar *  pHead;

   if (*pNHead == 0) iCard=0;
   else {
      pHead = *ppHead;
      for (iCard=0;
	   (iCard<*pNHead) && (strncmp((const char*)pLabel, (const char*)&pHead[iCard*80],8)!=0); iCard++);
   }
   return iCard;
}

/******************************************************************************/
/* Swap the integer values in the cards that match the passed labels.
 */
void fits_swap_cards_ival_
  (uchar *  pLabel1,
   uchar *  pLabel2,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      ival1;
   int      ival2;
 
   fits_get_card_ival_(&ival1, pLabel1, pNHead, ppHead);
   fits_get_card_ival_(&ival2, pLabel2, pNHead, ppHead);
   fits_change_card_ival_(&ival2, pLabel1, pNHead, ppHead);
   fits_change_card_ival_(&ival1, pLabel2, pNHead, ppHead);
}
 
/******************************************************************************/
/* Swap the integer values in the cards that match the passed labels.
 */
void fits_swap_cards_rval_
  (uchar *  pLabel1,
   uchar *  pLabel2,
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   float    rval1;
   float    rval2;
 
   fits_get_card_rval_(&rval1, pLabel1, pNHead, ppHead);
   fits_get_card_rval_(&rval2, pLabel2, pNHead, ppHead);
   fits_change_card_rval_(&rval2, pLabel1, pNHead, ppHead);
   fits_change_card_rval_(&rval1, pLabel2, pNHead, ppHead);
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and return the integer value of the argument after the label.
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 */
int fits_get_card_ival_
  (int   *  pIval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   HSIZE    iret;
   uchar *  pHead = *ppHead;
   char     pTemp[21];

   for (iCard=0;
	(iCard<*pNHead) && (strncmp((const char*)pLabel, (const char*)&pHead[iCard*80], 8)!=0); iCard++);
   if (iCard < *pNHead) {
#if 0
     sscanf(&pHead[iCard*80+10], "%20d", pIval);
#endif
     memmove(pTemp, &pHead[iCard*80+10], 20);
     pTemp[20] = '\0';
     sscanf(pTemp, "%d", pIval);
     iret = TRUE_MWDUST;
   }
   else {
     iret = FALSE_MWDUST;
   }
   return iret;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and return the real (float) value of the argument after the label.
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 */
int fits_get_card_rval_
  (float *  pRval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iCard;
   int      iret;
   uchar *  pHead = *ppHead;
   char     pTemp[21];

   
   for (iCard=0; (iCard<*pNHead) && (strncmp((const char*)pLabel, (const char*)&pHead[iCard*80], 8)!=0);
    iCard++);
   if (iCard < *pNHead) {
#if 0
     sscanf(&pHead[iCard*80+10], "%20f", pRval);
#endif
     memmove(pTemp, &pHead[iCard*80+10], 20);
     pTemp[20] = '\0';
     sscanf(pTemp, "%f", pRval);
     iret = TRUE_MWDUST;
   }
   else {
     iret = FALSE_MWDUST;
   }
   return iret;
}

#if 0
/******************************************************************************/
/*
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 */
int fits_get_julian_date_
  (float *  pJulianDate,
   uchar    pLabelDate[],
   uchar    pLabelTime[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iret;
   int      month;
   int      date;
   int      year;
   float    time;

   if (iret=fits_get_card_date_(month,date,year,pLabelDate,pNHead,ppHead)
    == TRUE_MWDUST) {
      *pJulianDate=...
      if (fits_get_card_time_(&time,pLabelTime,pNHead,ppHead) == TRUE_MWDUST) {
         *pJulianDate+=...
      }
   } else {
      *pJulianDate=0.0;
   }
   return iret;
}
#endif

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and return the date as three integers month, date and year.
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 */
int fits_get_card_date_
  (int   *  pMonth,
   int   *  pDate,
   int   *  pYear,
   uchar    pLabelDate[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iret;
   char  *  pStringVal;

   iret = fits_get_card_string_(&pStringVal, pLabelDate, pNHead, ppHead);
   if (iret == TRUE_MWDUST) {
      sscanf(pStringVal, "%d/%d/%d", pMonth, pDate, pYear);
      if (*pYear < 1900) *pYear += 1900;
      /* Free the memory used for the string value of this card */
      ccfree_((void **)&pStringVal);
   }
   return iret;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and return the time (TIME, RA, DEC or HA, for example) converted
 * to a real value.  Typically, this is used with TIME, RA or HA
 * to return a value in hours, or it is used with DEC to return a
 * value in degrees.
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 */
int fits_get_card_time_
  (float *  pTime,
   uchar    pLabelTime[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iret;
   int      timeHour;
   int      timeMin;
   float    timeSec;
   char  *  pStringVal;

   iret = fits_get_card_string_(&pStringVal, pLabelTime, pNHead, ppHead);
   if (iret == TRUE_MWDUST) {
      sscanf(pStringVal, "%d:%d:%f", &timeHour, &timeMin, &timeSec);
      *pTime=abs(timeHour) + timeMin/60.0 + timeSec/3600.0;
      /* Make the returned value negative if a minus sign is in the string */
      if (strchr(pStringVal, '-') != NULL) *pTime=-(*pTime);
      /* Free the memory used for the string value of this card */
      ccfree_((void **)&pStringVal);
   } else {
      *pTime = 0.0;
   }
   return iret;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and return a pointer to the string argument after the label.
 * Memory is dynamically allocated for the string argument.
 * Return TRUE_MWDUST if there is a match, and FALSE_MWDUST if there is none.
 * If there is not match, then create and return the string pStringUnknown.
 */
int fits_get_card_string_
  (char  ** ppStringVal,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   int      iChar;
   int      iret;
   HSIZE    iCard;
   MEMSZ    memSize;
   uchar *  pHead = *ppHead;
   char  *  pTemp;
   char     pStringUnknown[] = "?";

   memSize = 70;
   ccalloc_(&memSize, (void **)&pTemp);
   for (iCard=0;
    (iCard<*pNHead) && (strncmp((const char*)pLabel, (const char*)&pHead[iCard*80], 8)!=0); iCard++);
   if (iCard < *pNHead) {
   /* It must start with a single quote in column 11 (1-indexed) if not blank.
      Otherwise, an empty string is returned, which is O.K. */
     iChar = 11;
     /* Copy characters from column 12 until column 80 or another single
        quote is reached. */
     /* !!!??? NOTE: TWO SINGLE QUOTES SHOULD BE READ IN AS A QUOTE */
     if (pHead[iCard*80+10]=='\'') {
       while (iChar<80 && (pTemp[iChar-11]=pHead[iCard*80+iChar]) != '\'')
        iChar++;
     }

     pTemp[iChar-11]='\0';  /* Pad with a NULL at the end of the string */
     /* Remove trailing blanks; leading blanks are significant */
     iChar = strlen(pTemp);
     while (iChar>0 && pTemp[--iChar]==' ') pTemp[iChar]='\0';

     iret = TRUE_MWDUST;
   }
   else {
     strcpy(pTemp, pStringUnknown);

     iret = FALSE_MWDUST;
   }

   *ppStringVal=pTemp;
   return iret;
}

/******************************************************************************/
/*
 * Change the 1st card that matches the passed label, or add a card if there
 * is not a match.  Return the card number of the changed or added card.
 */
HSIZE fits_change_card_
  (uchar    pCard[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[80];
   uchar *  pHead = *ppHead;

   fits_string_to_card_(pCard, pCardTemp);

   iCard = fits_find_card_(pCardTemp, pNHead, ppHead);
   if (iCard < *pNHead) {
      memmove(&pHead[iCard*80], pCardTemp, 80);
   } else {
      iCard = fits_add_card_(pCardTemp, pNHead, ppHead);
   }

   return iCard;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and change the integer value of the argument after the label.
 * If no card exists, then create one.  Return the card number of
 * the changed or added card.
 */
HSIZE fits_change_card_ival_
  (int   *  pIval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   sprintf( (char*)pCardTemp, "%-8.8s= %20d", pLabel, *pIval);
   iCard = fits_change_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and change the real (float) value of the argument after the label.
 * If no card exists, then create one.  Return the card number of
 * the changed or added card.
 */
HSIZE fits_change_card_rval_
  (float *  pRval,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   sprintf( (char*)pCardTemp, "%-8.8s= %20.7e", pLabel, *pRval);
   iCard = fits_change_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/*
 * Find the 1st header card whose label matches the label passed,
 * and change the string value of the argument after the label.
 * If no card exists, then create one.  Return the card number of
 * the changed or added card.
 */
HSIZE fits_change_card_string_
  (char  *  pStringVal,
   uchar    pLabel[],
   HSIZE *  pNHead,
   uchar ** ppHead)
{
   HSIZE    iCard;
   uchar    pCardTemp[81]; /* Last character is for null from sprintf() */

   /* !!!??? NOTE: A QUOTE SHOULD BE WRITTEN AS 2 SINGLE QUOTES */
   sprintf( (char*)pCardTemp, "%-8.8s= '%-1.68s'", pLabel, pStringVal);
   iCard = fits_change_card_(pCardTemp, pNHead, ppHead);

   return iCard;
}

/******************************************************************************/
/* Convert a character string to a FITS-complient 80-character card.
 * The string is copied until either a NULL or CR is reached or the 80th
 * character is reached.  The remainder of the card is padded with spaces.
 * In addition, the label part of the card (the first 8 characters)
 * are converted to upper case.
 *
 * Note that pCard[] must be dimensioned to at least the necessary 80
 * characters.
 */
void fits_string_to_card_
  (uchar    pString[],
   uchar    pCard[])
{
   int      iChar;
   int      qNull;

   /* Copy the raw string into the card array */
   memmove(pCard, pString, 80);

   /* Search for a NULL or CR in the card, and replace that character and
    * all following characters with a space.
    */
   qNull = FALSE_MWDUST;
   iChar = 0;
   while (iChar < 80) {
      if (pCard[iChar] == '\0' || pCard[iChar] == '\n') qNull = TRUE_MWDUST;
      if (qNull == TRUE_MWDUST) pCard[iChar] = ' ';
      iChar++;
   }

   /* Convert the label (the first 8 characters) to upper case) */
   for (iChar=0; iChar < 8; iChar++) {
      pCard[iChar] = toupper(pCard[iChar]);
   }
}

/******************************************************************************/
/*
 * Return the (float) value of the data array indexed by the iloc'th elements,
 * taking care to use the proper data format as specified by bitpix.
 * Several unconventional values for bitpix are supported: 32, 8, -8.
 * For a 2-dimensional array, set iloc=x+y*naxis1.
 */
float fits_get_rval_
  (DSIZE *  pIloc,
   int   *  pBitpix,
   float *  pBscale,
   float *  pBzero,
   uchar ** ppData)
{
   float    rval;
   uchar     * pIdata8  = (uchar     *)(*ppData);
   short int * pIdata16 = (short int *)(*ppData);
   long  int * pIdata32 = (long  int *)(*ppData);
   float     * pRdata32 = (float     *)(*ppData);
   double    * pRdata64 = (double    *)(*ppData);

   if      (*pBitpix ==-32) rval = pRdata32[*pIloc];
   else if (*pBitpix == 16) rval = pIdata16[*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix == 32) rval = pIdata32[*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix ==-64) rval = pRdata64[*pIloc];
   else if (*pBitpix ==  8) rval = pIdata8 [*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix == -8) rval = pIdata8 [*pIloc];
   else                     rval = 0.0; /* Invalid BITPIX! */
   return rval;
}

/******************************************************************************/
/*
 * Return the (int) value of the data array indexed by the iloc'th elements,
 * taking care to use the proper data format as specified by bitpix.
 * Several unconventional values for bitpix are supported: 32, 8, -8.
 * For a 2-dimensional array, set iloc=x+y*naxis1.
 */
int fits_get_ival_
  (DSIZE *  pIloc,
   int   *  pBitpix,
   float *  pBscale,
   float *  pBzero,
   uchar ** ppData)
{
   int      ival;
   float    rval;
   uchar     * pIdata8  = (uchar     *)(*ppData);
   short int * pIdata16 = (short int *)(*ppData);
   long  int * pIdata32 = (long  int *)(*ppData);
   float     * pRdata32 = (float     *)(*ppData);
   double    * pRdata64 = (double    *)(*ppData);

   if      (*pBitpix ==-32) rval = pRdata32[*pIloc];
   else if (*pBitpix == 16) rval = pIdata16[*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix == 32) rval = pIdata32[*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix ==-64) rval = pRdata64[*pIloc];
   else if (*pBitpix ==  8) rval = pIdata8 [*pIloc] * (*pBscale) + (*pBzero);
   else if (*pBitpix == -8) rval = pIdata8 [*pIloc];
   else                     rval = 0.0; /* Invalid BITPIX! */

   /* Round to the nearest integer */
   if (rval >= 0.0) {
     ival = (int)(rval + 0.5);
   } else {
     ival = (int)(rval - 0.5);
   }

   return ival;
}

/******************************************************************************/
/*
 * Put a (float) value into location iloc of the data array, taking care to
 * use the proper data format as specified by bitpix.  For a 2-dimensional
 * array, set iloc=x+y*naxis1.
 * Several unconventional values for bitpix are supported: 32, 8, -8.
 * Note: Is the rounding done properly!!!???
 */
void fits_put_rval_
  (float *  pRval,
   DSIZE *  pIloc,
   int   *  pBitpix,
   float *  pBscale,
   float *  pBzero,
   uchar ** ppData)
{
   uchar     * pIdata8  = (uchar     *)(*ppData);
   short int * pIdata16 = (short int *)(*ppData);
   long  int * pIdata32 = (long  int *)(*ppData);
   float     * pRdata32 = (float     *)(*ppData);
   double    * pRdata64 = (double    *)(*ppData);

   if      (*pBitpix ==-32) 
     { pRdata32[*pIloc] = *pRval; }
   else if (*pBitpix == 16) 
     { pIdata16[*pIloc] = (short)( (*pRval - *pBzero) / (*pBscale) ); }
   else if (*pBitpix == 32) 
     { pIdata32[*pIloc] = (long)((*pRval - *pBzero) / (*pBscale)); }
   else if (*pBitpix ==-64) 
     { pRdata64[*pIloc] = *pRval; }
   else if (*pBitpix ==  8) 
     { pIdata8 [*pIloc] = (uchar)((*pRval - *pBzero) / (*pBscale)); }
   else if (*pBitpix == -8) 
     { pIdata8 [*pIloc] = (uchar)*pRval; }
}

/******************************************************************************/
/*
 * Ask whether a particular pixel position in the data array is equal
 * to the value specified by the BLANK card.  This test is performed WITHOUT
 * first rescaling the data.  Pass the blank value as
 * a real variable, even if it was originally integer.  For a 2-dimensional
 * array, set iloc=x+y*naxis1.  Return TRUE_MWDUST (iq!=0) if the pixel is
 * BLANK, or FALSE_MWDUST (iq==0) if it is not.
 * The value blankval must be found first and passed to this routine.
 * Several unconventional values for bitpix are supported: 32, 8, -8.
 */
int fits_qblankval_
  (DSIZE *  pIloc,
   int   *  pBitpix,
   float *  pBlankval,
   uchar ** ppData)
{
   int      iq;
   uchar     * pIdata8  = (uchar     *)(*ppData);
   short int * pIdata16 = (short int *)(*ppData);
   long  int * pIdata32 = (long  int *)(*ppData);
   float     * pRdata32 = (float     *)(*ppData);
   double    * pRdata64 = (double    *)(*ppData);

   if      (*pBitpix ==-32) iq = ( pRdata32[*pIloc] == (*pBlankval) );
   else if (*pBitpix == 16) iq = ( pIdata16[*pIloc] == (*pBlankval) );
   else if (*pBitpix == 32) iq = ( pIdata32[*pIloc] == (*pBlankval) );
   else if (*pBitpix ==-64) iq = ( pRdata64[*pIloc] == (*pBlankval) );
   else if (*pBitpix ==  8) iq = ( pIdata8 [*pIloc] == (*pBlankval) );
   else if (*pBitpix == -8) iq = ( pIdata8 [*pIloc] == (*pBlankval) );
   else                     iq = FALSE_MWDUST; /* Invalid BITPIX! */

   return iq;
}

/******************************************************************************/
/*
 * Replace a data element by a BLANK value as determined by blankval.
 * The value is assigned WITHOUT rescaling.
 * The value blankval must be found first and passed to this routine.
 * Several unconventional values for bitpix are supported: 32, 8, -8.
 */
void fits_put_blankval_
  (DSIZE *  pIloc,
   int   *  pBitpix,
   float *  pBlankval,
   uchar ** ppData)
{
   uchar     * pIdata8  = (uchar     *)(*ppData);
   short int * pIdata16 = (short int *)(*ppData);
   long  int * pIdata32 = (long  int *)(*ppData);
   float     * pRdata32 = (float     *)(*ppData);
   double    * pRdata64 = (double    *)(*ppData);

   if      (*pBitpix ==-32) pRdata32[*pIloc] = *pBlankval;
   else if (*pBitpix == 16) pIdata16[*pIloc] = (short)*pBlankval;
   else if (*pBitpix == 32) pIdata32[*pIloc] = (long)*pBlankval;
   else if (*pBitpix ==-64) pRdata64[*pIloc] = *pBlankval;
   else if (*pBitpix ==  8) pIdata8 [*pIloc] = (uchar)*pBlankval;
   else if (*pBitpix == -8) pIdata8 [*pIloc] = (uchar)*pBlankval;
}

/******************************************************************************/
/*
 * Replace any nulls in a card by spaces.
 */
void fits_purge_nulls
  (uchar    pCard[])
{
   int iChar;

   for (iChar=0; iChar < 80; iChar++) {
      if (pCard[iChar] == '\0') pCard[iChar] = ' ';
   }
}

/******************************************************************************/
/*
 * Read the next 80-character card from the specified device.
 * Return 0 if the END card is reached.
 */
int fits_get_next_card_
  (int   *  pFilenum,
   uchar    pCard[])
{
   int      iChar;

   for (iChar=0; iChar < 80; iChar++) {
      pCard[iChar] = fgetc(pFILEfits[*pFilenum]);
   }
   return strncmp((const char *)card_end, (const char *)pCard, 8);
}

/******************************************************************************/
/*
 * Write passed card to open file.  Return FALSE_MWDUST for a write error.
 */
int fits_put_next_card_
  (int   *  pFilenum,
   uchar    pCard[])
{
   int      iChar;
   int      retval;

   retval = TRUE_MWDUST;
   for (iChar=0; iChar < 80; iChar++) {
      if (fputc(pCard[iChar], pFILEfits[*pFilenum]) == EOF) retval = FALSE_MWDUST;
   }
   return retval;
}

/******************************************************************************/
/*
 * Determine the size of an individual datum based upon the FITS definitions
 * of the BITPIX card.
 */
int fits_size_from_bitpix_
  (int *pBitpix)
{
   int size;

   if      (*pBitpix ==   8) size = 1;
   else if (*pBitpix ==  16) size = 2;
   else if (*pBitpix ==  32) size = 4;
   else if (*pBitpix ==  64) size = 8;
   else if (*pBitpix == -16) size = 2;
   else if (*pBitpix == -32) size = 4;
   else if (*pBitpix == -64) size = 8;
   else                      size = 0; /* Bitpix undefined! */

   return size;
}

/******************************************************************************/
/*
 * For data of arbitrary dimensions, shift the pixels along the "*pSAxis"
 * axis by "*pShift" pixels, wrapping data around the image boundaries.
 * For example, if the middle dimension of a 3-dimen array is shifted:
 *   new[i,j+shift,k] = old[i,j,k]
 * The data can be of any data type (i.e., any BITPIX).
 */
void fits_pixshift_wrap_
  (int   *  pSAxis,
   DSIZE *  pShift,
   HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      size;
   DSIZE    posShift;
   int      iAxis;
   int      numAxes;
   DSIZE *  pNaxis;
   DSIZE    dimBig;
   DSIZE    dimSml;
   DSIZE    indxBig;
   DSIZE    indxSml;
   DSIZE    offset;
   DSIZE    iloc;
   int      bitpix;
   MEMSZ    memSize;
   DSIZE    nVector;
   DSIZE    iVector;
   uchar *  pVector;

   fits_compute_axes_(pNHead, ppHead, &numAxes, &pNaxis);
   nVector = pNaxis[*pSAxis];
   posShift = *pShift;
   while (posShift < 0) posShift += nVector; /* Must be positive value */

   /* Allocate an array equal in size to one vector in the *pSAxis dimension */
   fits_get_card_ival_(&bitpix, label_bitpix, pNHead, ppHead);
   size = fits_size_from_bitpix_(&bitpix);
   memSize = size * nVector;
   ccalloc_(&memSize, (void **)&pVector);

   /* Compute the number of larger and smaller indices */
   dimBig = 1;
   for (iAxis=0; iAxis < *pSAxis; iAxis++) dimBig *= pNaxis[iAxis];
   dimSml = 1;
   for (iAxis=*pSAxis+1; iAxis < numAxes; iAxis++) dimSml *= pNaxis[iAxis];

   /* Loop through each of the larger and smaller indices */
   for (indxBig=0; indxBig < dimBig; indxBig++) {
   for (indxSml=0; indxSml < dimSml; indxSml++) {
      offset = indxBig * nVector * dimSml + indxSml;

      /* Copy vector into temporary vector */
      for (iVector=0; iVector < nVector; iVector++) {
         iloc = offset + iVector * dimSml;
         memmove(&pVector[iVector*size], &(*ppData)[iloc*size], size);
      }

      /* Copy the shifted vector back into the main data array */
      for (iVector=0; iVector < nVector; iVector++) {
         /* Use the MOD operator below to wrap the dimensions */
         iloc = offset + ((iVector+(posShift)) % nVector) * dimSml;
         memmove(&(*ppData)[iloc*size], &pVector[iVector*size], size);
      }
   } }

   /* Free memory */
   ccfree_((void **)&pVector);

   /* Plug a memory leak - D. Schlegel 06-Feb-1999 */
   fits_free_axes_(&numAxes, &pNaxis);
}

/******************************************************************************/
/*
 * For data of 2 dimensions, transpose the data by setting
 * pData[i][j] = pData[j][i].
 * A new data array is created and the old one is destroyed.
 * Also, swap the appropriate header cards.
 */
void fits_transpose_data_
  (HSIZE *  pNHead,
   uchar ** ppHead,
   DSIZE *  pNData,
   uchar ** ppData)
{
   int      bitpix;
   int      size;
   int      iByte;
   int      numAxes;
   DSIZE *  pNaxis;
   DSIZE    nData;
   DSIZE    iRow;
   DSIZE    iCol;
   DSIZE    ilocOld;
   DSIZE    ilocNew;
   MEMSZ    memSize;
   uchar *  pNewData;

   fits_compute_axes_(pNHead, ppHead, &numAxes, &pNaxis);
   if (numAxes == 2) {
      /* Allocate an array equal in size to the data array */
      nData = fits_compute_ndata_(pNHead, ppHead);
      fits_get_card_ival_(&bitpix, label_bitpix, pNHead, ppHead);
      size = fits_size_from_bitpix_(&bitpix);
      memSize = size * nData;
      ccalloc_(&memSize, (void **)&pNewData);

      /* Copy the data into the new data array, transposing the first 2 axes */
      for (iRow=0; iRow < pNaxis[1]; iRow++) {
         for (iCol=0; iCol < pNaxis[0]; iCol++) {
            ilocOld = size * (iRow*pNaxis[0] + iCol);
            ilocNew = size * (iCol*pNaxis[1] + iRow);
            /* For each data element, copy the proper number of bytes */
            for (iByte=0; iByte < size; iByte++) {
               pNewData[ilocNew+iByte] = (*ppData)[ilocOld+iByte];
            }
         }
      }

      /* Discard the old data array and return the new one */
      ccfree_((void **)ppData);
      *ppData = pNewData;

      /* Switch the values in the header of NAXIS1 and NAXIS2,
       * and the cards used to label the pixel numbers on those axes.
       */
      fits_swap_cards_ival_(label_naxis1, label_naxis2, pNHead, ppHead);
      fits_swap_cards_rval_(label_crpix1, label_crpix2, pNHead, ppHead);
      fits_swap_cards_rval_(label_crval1, label_crval2, pNHead, ppHead);
      fits_swap_cards_rval_(label_cdelt1, label_cdelt2, pNHead, ppHead);
   }

   /* Plug a memory leak - D. Schlegel 06-Feb-1999 */
   fits_free_axes_(&numAxes, &pNaxis);
}

/******************************************************************************/
/*
 * Average several rows (or columns) of a 2-dimensional array of floats.
 * If (*iq)==0 then average rows; if (*iq)==1 then average columns.
 * Note that *pNaxis1=number of columns and *pNaxis2=number of rows.
 * Memory is dynamically allocated for the output vector.
 */
void fits_ave_rows_r4_
  (int   *  iq,
   DSIZE *  pRowStart,
   DSIZE *  pNumRowAve,
   DSIZE *  pNaxis1,
   DSIZE *  pNaxis2,
   float ** ppData,
   float ** ppOut)
{
   DSIZE   iCol;
   DSIZE   iRow;
   DSIZE   rowStart;
   DSIZE   rowEnd;
   MEMSZ   memSize;
   float   weight;
   float * pData = *ppData;
   float * pOut;

   if (*iq == 0) {
      memSize = sizeof(float) * (*pNaxis1);
      ccalloc_(&memSize, (void **)ppOut);
      pOut = *ppOut;
      rowStart = max(0, *pRowStart);
      rowEnd = min(*pRowStart + *pNumRowAve, *pNaxis2);
      weight = (rowEnd + 1 - rowStart);
      for (iCol=0; iCol < *pNaxis1; iCol++) {
         pOut[iCol] = 0.0;
         for (iRow=rowStart; iRow <= rowEnd; iRow++) {
            pOut[iCol] += pData[iRow*(*pNaxis1) + iCol];
         }
         pOut[iCol] /= weight;
      }
   } else if (*iq == 1) {
      memSize = sizeof(float) * (*pNaxis2);
      ccalloc_(&memSize, (void **)ppOut);
      pOut = *ppOut;
      rowStart = max(0, *pRowStart);
      rowEnd = min(*pRowStart + *pNumRowAve, *pNaxis1);
      weight = (rowEnd + 1 - rowStart);
      for (iRow=0; iRow < *pNaxis2; iRow++) {
         pOut[iRow] = 0.0;
         for (iCol=rowStart; iCol <= rowEnd; iCol++) {
            pOut[iRow] += pData[iRow*(*pNaxis1) + iCol];
         }
         pOut[iRow] /= weight;
      }
   }

}

/******************************************************************************/
/*
 * Average several rows (or columns) of a 2-dimensional array of floats
 * with their standard deviations.  For each combined set of points:
 *    obj_ave = SUM_i {obj_i / sig_i^2} / SUM_i {1 / sig_i^2}
 *    sig_ave = 1 / SUM_i {1 / sig_i^2}
 * If (*iq)==0 then average rows; if (*iq)==1 then average columns.
 * Note that *pNaxis1=number of columns and *pNaxis2=number of rows.
 * Memory is dynamically allocated for the output vector.
 */
void fits_ave_obj_and_sigma_rows_r4_
  (int   *  iq,
   DSIZE *  pRowStart,
   DSIZE *  pNumRowAve,
   DSIZE *  pNaxis1,
   DSIZE *  pNaxis2,
   float ** ppObjData,
   float ** ppSigData,
   float ** ppObjOut,
   float ** ppSigOut)
{
   DSIZE   iCol;
   DSIZE   iRow;
   DSIZE   rowStart;
   DSIZE   rowEnd;
   DSIZE   iloc;
   MEMSZ   memSize;
   float   weight;
   float   oneOverSumVar;
   float * pObjData = *ppObjData;
   float * pSigData = *ppSigData;
   float * pObjOut;
   float * pSigOut;

   if (*iq == 0) {
      memSize = sizeof(float) * (*pNaxis1);
      ccalloc_(&memSize, (void **)ppObjOut);
      ccalloc_(&memSize, (void **)ppSigOut);
      pObjOut = *ppObjOut;
      pSigOut = *ppSigOut;
      rowStart = max(0, (*pRowStart));
      rowEnd = min((*pRowStart) + (*pNumRowAve) - 1, (*pNaxis2) - 1);
      for (iCol=0; iCol < *pNaxis1; iCol++) {
         pObjOut[iCol] = 0.0;
         oneOverSumVar = 0.0;
         for (iRow=rowStart; iRow <= rowEnd; iRow++) {
            iloc = iRow*(*pNaxis1) + iCol;
            weight = 1.0 / (pSigData[iloc] * pSigData[iloc]);
            pObjOut[iCol] += pObjData[iloc] * weight;
            oneOverSumVar += weight;
         }
         pObjOut[iCol] /= oneOverSumVar;
         pSigOut[iCol] = 1.0 / sqrt(oneOverSumVar);
      }

   } else if (*iq == 1) {
      memSize = sizeof(float) * (*pNaxis2);
      ccalloc_(&memSize, (void **)ppObjOut);
      ccalloc_(&memSize, (void **)ppSigOut);
      pObjOut = *ppObjOut;
      pSigOut = *ppSigOut;
      rowStart = max(0, (*pRowStart));
      rowEnd = min((*pRowStart) + (*pNumRowAve) - 1, (*pNaxis1) - 1);
      for (iRow=0; iRow < *pNaxis2; iRow++) {
         pObjOut[iRow] = 0.0;
         oneOverSumVar = 0.0;
         for (iCol=rowStart; iCol <= rowEnd; iCol++) {
            iloc = iRow*(*pNaxis1) + iCol;
            weight = 1.0 / (pSigData[iloc] * pSigData[iloc]);
            pObjOut[iRow] += pObjData[iloc] * weight;
            oneOverSumVar += weight;
         }
         pObjOut[iRow] /= oneOverSumVar;
         pSigOut[iRow] = 1.0 / sqrt(oneOverSumVar);
      }
   }

}

/******************************************************************************/
/*
 * Swap bytes between big-endian and little-endian.
 */
void fits_byteswap
  (int      bitpix,
   DSIZE    nData,
   uchar *  pData)
{
   int      ibits;
   DSIZE    idata;

   ibits = abs(bitpix);
   if (ibits == 16) {
      for (idata=0; idata < nData; idata++) {
         fits_bswap2( &pData[2*idata  ], &pData[2*idata+1] );
      }
   } else if (ibits == 32) {
      for (idata=0; idata < nData; idata++) {
         fits_bswap2( &pData[4*idata  ], &pData[4*idata+3] );
         fits_bswap2( &pData[4*idata+1], &pData[4*idata+2] );
      }
   } else if (ibits == 64) {
      for (idata=0; idata < nData; idata++) {
         fits_bswap2( &pData[8*idata  ], &pData[8*idata+7] );
         fits_bswap2( &pData[8*idata+1], &pData[8*idata+6] );
         fits_bswap2( &pData[8*idata+2], &pData[8*idata+5] );
         fits_bswap2( &pData[8*idata+3], &pData[8*idata+4] );
      }
   }

}

void fits_bswap2
  (uchar *  pc1,
   uchar *  pc2)
{
   uchar    ct;
   ct = *pc1;
   *pc1 = *pc2;
   *pc2 = ct;
}


#ifdef OLD_SUNCC

/******************************************************************************/
/*
 * Copy one section of memory (a string) to another, even if they overlap.
 * The advertised C library routine by this name does not actually exist
 * in old SunOS.
 */
void memmove
  (void  *  s,
   const void  *  ct,
   MEMSZ    n)
{
   MEMSZ    i;
   char  *  ps = (char *)s;
   const char  *  pct = (const char *)ct;
 
   /* Do nothing if ps == pct */
   if (ps > pct) for (i=0; i < n; i++) *(ps+n-i-1) = *(pct+n-i-1);
   else if (ps < pct) for (i=0; i < n; i++) *(ps+i) = *(pct+i);
}

#endif

/******************************************************************************/
/*
 * Change the size of memory for data, and return the new address as *ppData.
 * Copy contents of memory in common.
 */
void ccalloc_resize_
  (MEMSZ *  pOldMemSize,
   MEMSZ *  pNewMemSize,
   void  ** ppData)
{
   void  *  pNewData;

   if (*pNewMemSize > *pOldMemSize) {
      ccalloc_(pNewMemSize,&pNewData);
      memmove((void *)pNewData,(void *)(*ppData),*pOldMemSize);
      ccfree_(ppData);
      *ppData = pNewData;
   } else if (*pNewMemSize < *pOldMemSize) {
      ccalloc_(pNewMemSize,&pNewData);
      memmove((void *)pNewData,(void *)(*ppData),*pNewMemSize);
      ccfree_(ppData);
      *ppData = pNewData;
   }
}

/******************************************************************************/
/*
 * Allocate *pMemSize bytes of data.  The starting memory location is *ppData.
 * If the array has previously been allocated, then resize it.
 */
void ccrealloc_
  (MEMSZ *  pMemSize,
   void  ** ppData)
{
   if (*ppData == NULL) {
      *ppData = (void *)malloc((size_t)(*pMemSize));
   } else {
      *ppData = (void *)realloc(*ppData,(size_t)(*pMemSize));
   }
}

/******************************************************************************/
/*
 * Allocate *pMemSize bytes of data.  The starting memory location is *ppData.
 * Also zero all of the data byes.
 */
void ccalloc_init
  (MEMSZ *  pMemSize,
   void  ** ppData)
{
   size_t   nobj = 1;
   *ppData = (void *)calloc(nobj, (size_t)(*pMemSize));
}

/******************************************************************************/
/*
 * Allocate *pMemSize bytes of data.  The starting memory location is *ppData.
 */
void ccalloc_
  (MEMSZ *  pMemSize,
   void  ** ppData)
{
   *ppData = (void *)malloc((size_t)(*pMemSize));
}

/******************************************************************************/
/*
 * Free the memory block that starts at address *ppData.
 */
void ccfree_
  (void  ** ppData)
{
   free(*ppData);
   *ppData = NULL;
}

/******************************************************************************/
float * ccvector_build_
  (MEMSZ    n)
{
   float * pVector = (float *)malloc((size_t)(sizeof(float) * n));
   return pVector;
}

/******************************************************************************/
double * ccdvector_build_
  (MEMSZ    n)
{
   double * pVector = (double *)malloc((size_t)(sizeof(double) * n));
   return pVector;
}

/******************************************************************************/
int * ccivector_build_
  (MEMSZ    n)
{
   int * pVector = (int *)malloc((size_t)(sizeof(int) * n));
   return pVector;
}
 
/******************************************************************************/
float ** ccpvector_build_
  (MEMSZ    n)
{
   float ** ppVector = (float **)malloc((size_t)(sizeof(float *) * n));
   return ppVector;
}
 
/******************************************************************************/
/* Build a vector of pointers to arrays of type (float **) */
float *** ccppvector_build_
  (MEMSZ    n)
{
   float *** pppVector = (float ***)malloc((size_t)(sizeof(float **) * n));
   return pppVector;
}
 
/******************************************************************************/
float * ccvector_rebuild_
  (MEMSZ    n,
   float *  pOldVector)
{
   float * pVector;

   if (pOldVector == NULL) {
      pVector = (float *)malloc((size_t)(sizeof(float) * n));
   } else {
      pVector = (float *)realloc(pOldVector,(size_t)(sizeof(float) * n));
   }

   return pVector;
}

/******************************************************************************/
double * ccdvector_rebuild_
  (MEMSZ    n,
   double *  pOldVector)
{
   double * pVector;

   if (pOldVector == NULL) {
      pVector = (double *)malloc((size_t)(sizeof(double) * n));
   } else {
      pVector = (double *)realloc(pOldVector,(size_t)(sizeof(double) * n));
   }

   return pVector;
}

/******************************************************************************/
int * ccivector_rebuild_
  (MEMSZ    n,
   int   *  pOldVector)
{
   int   *  pVector;

   if (pOldVector == NULL) {
      pVector = (int *)malloc((size_t)(sizeof(int) * n));
   } else {
      pVector = (int *)realloc(pOldVector,(size_t)(sizeof(int) * n));
   }

   return pVector;
}

/******************************************************************************/
float ** ccpvector_rebuild_
  (MEMSZ    n,
   float ** ppOldVector)
{
   float ** ppVector;

   if (ppOldVector == NULL) {
      ppVector = (float **)malloc((size_t)(sizeof(float *) * n));
   } else {
      ppVector = (float **)realloc(ppOldVector,(size_t)(sizeof(float *) * n));
   }

   return ppVector;
}

/******************************************************************************/
/* Build a vector of pointers to arrays of type (float **) */
float *** ccppvector_rebuild_
  (MEMSZ    n,
   float *** pppOldVector)
{
   float *** pppVector;
 
   if (pppOldVector == NULL) {
      pppVector = (float ***)malloc((size_t)(sizeof(float **) * n));
   } else {
      pppVector = (float ***)realloc(pppOldVector,(size_t)(sizeof(float **) * n)
);
   }
 
   return pppVector;
}

/******************************************************************************/
void ccvector_free_
  (float *  pVector)
{
   free((void *)pVector);
}

/******************************************************************************/
void ccdvector_free_
  (double *  pVector)
{
   free((void *)pVector);
}

/******************************************************************************/
void ccivector_free_
  (int   *  pVector)
{
   free((void *)pVector);
}

/******************************************************************************/
void ccpvector_free_
  (float ** ppVector)
{
   free((void *)ppVector);
}

/******************************************************************************/
void ccppvector_free_
  (float *** pppVector)
{
   free((void *)pppVector);
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of floats with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
float ** ccarray_build_
  (MEMSZ    nRow,
   MEMSZ    nCol)
{
   MEMSZ    iRow;
 
   float *  pSpace  = (float *)malloc(sizeof(float ) * nRow * nCol);
   float ** ppArray = (float**)malloc(sizeof(float*) * nRow);

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(float) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of doubles with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
double ** ccdarray_build_
  (MEMSZ    nRow,
   MEMSZ    nCol)
{
   MEMSZ    iRow;
 
   double *  pSpace  = (double *)malloc(sizeof(double ) * nRow * nCol);
   double ** ppArray = (double**)malloc(sizeof(double*) * nRow);

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(double) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of ints with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
int ** cciarray_build_
  (MEMSZ    nRow,
   MEMSZ    nCol)
{
   MEMSZ    iRow;
 
   int *  pSpace  = (int *)malloc(sizeof(int ) * nRow * nCol);
   int ** ppArray = (int**)malloc(sizeof(int*) * nRow);

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(int) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of floats with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
float ** ccarray_rebuild_
  (MEMSZ    nRow,
   MEMSZ    nCol,
   float ** ppOldArray)
{
   MEMSZ    iRow;
 
   float *  pSpace;
   float ** ppArray;

   if (ppOldArray == NULL) {
      pSpace  = (float *)malloc(sizeof(float ) * nRow * nCol);
      ppArray = (float**)malloc(sizeof(float*) * nRow);
   } else {
      ppArray = (float**)realloc(ppOldArray, sizeof(float*) * nRow);
      pSpace  = (float *)realloc(ppOldArray[0],sizeof(float ) * nRow * nCol);
   }

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(float) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of doubles with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
double ** ccdarray_rebuild_
  (MEMSZ    nRow,
   MEMSZ    nCol,
   double ** ppOldArray)
{
   MEMSZ    iRow;
 
   double *  pSpace;
   double ** ppArray;

   if (ppOldArray == NULL) {
      pSpace  = (double *)malloc(sizeof(double ) * nRow * nCol);
      ppArray = (double**)malloc(sizeof(double*) * nRow);
   } else {
      ppArray = (double**)realloc(ppOldArray, sizeof(double*) * nRow);
      pSpace  = (double *)realloc(ppOldArray[0],sizeof(double ) * nRow * nCol);
   }

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(double) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Build an nRow x nCol matrix, in pointer-style.
 * Allocate one contiguous array of ints with  nRow*nCol elements.
 * Then create a set of nRow pointers, each of which points to the next nCol
 *  elements.
 */
int ** cciarray_rebuild_
  (MEMSZ    nRow,
   MEMSZ    nCol,
   int   ** ppOldArray)
{
   MEMSZ    iRow;
 
   int   *  pSpace;
   int   ** ppArray;

   if (ppOldArray == NULL) {
      pSpace  = (int *)malloc(sizeof(int ) * nRow * nCol);
      ppArray = (int**)malloc(sizeof(int*) * nRow);
   } else {
      ppArray = (int**)realloc(ppOldArray, sizeof(int*) * nRow);
      pSpace  = (int *)realloc(ppOldArray[0],sizeof(int ) * nRow * nCol);
   }

   for (iRow = 0; iRow < nRow; iRow++) {
      /* Quantity (iRow*nCol) scales by sizeof(int) */
      ppArray[iRow] = pSpace + (iRow * nCol);
   }

   return ppArray;
}

/******************************************************************************/
/* Free all memory allocated for an nRow x nCol matrix, as set up
 * with the routine ccarray_build_().
 */
void ccarray_free_
  (float ** ppArray,
   MEMSZ    nRow)
{
   /* Memory has been allocated in one contiguous chunk, so only free
    * ppArray[0] rather than every ppArray[i] for all i.
    */
   free((void *)ppArray[0]);
   free((void *)ppArray);
}

/******************************************************************************/
/* Free all memory allocated for an nRow x nCol matrix, as set up
 * with the routine ccdarray_build_().
 */
void ccdarray_free_
  (double ** ppArray,
   MEMSZ    nRow)
{
   /* Memory has been allocated in one contiguous chunk, so only free
    * ppArray[0] rather than every ppArray[i] for all i.
    */
   free((void *)ppArray[0]);
   free((void *)ppArray);
}

/******************************************************************************/
/* Free all memory allocated for an nRow x nCol matrix, as set up
 * with the routine cciarray_build_().
 */
void cciarray_free_
  (int   ** ppArray,
   MEMSZ    nRow)
{
   /* Memory has been allocated in one contiguous chunk, so only free
    * ppArray[0] rather than every ppArray[i] for all i.
    */
   free((void *)ppArray[0]);
   free((void *)ppArray);
}

/******************************************************************************/
/* Set all elements of an array equal to zero.
 * The array is assumed to be pointer-style, as allocated by ccarray_build_.
 */
void ccarray_zero_
  (float ** ppArray,
   MEMSZ    nRow,
   MEMSZ    nCol)
{
   MEMSZ    iRow;
   MEMSZ    iCol;

   for (iRow=0; iRow < nRow; iRow++) {
      for (iCol=0; iCol < nCol; iCol++) {
         ppArray[iRow][iCol] = 0.0;
      }
   }
}

/******************************************************************************/
/* Set all elements of a vector equal to zero.
 */
void ccvector_zero_
  (float *  pVector,
   MEMSZ    n)
{
   MEMSZ    i;
   for (i=0; i < n; i++) pVector[i] = 0.0;
}

/******************************************************************************/
/* Set all elements of a vector equal to zero.
 */
void ccdvector_zero_
  (double *  pVector,
   MEMSZ    n)
{
   MEMSZ    i;
   for (i=0; i < n; i++) pVector[i] = 0.0;
}

/******************************************************************************/
/* Set all elements of a vector equal to zero.
 */
void ccivector_zero_
  (int    *  pVector,
   MEMSZ    n)
{
   MEMSZ    i;
   for (i=0; i < n; i++) pVector[i] = 0;
}



 



/* Initialize file pointers to NULL -- this is done automatically since
 * this is an external variable.
 * Files are numbered 0 to IO_FOPEN_MAX-1.
 */

FILE  *  pFILEfits[IO_FOPEN_MAX];

/******************************************************************************/
/* Return IO_GOOD if a file exists, and IO_BAD otherwise.
 */
int inoutput_file_exist
  (char  *  pFileName)
{
   int      retval;

   if (access(pFileName, F_OK) == 0) {
      retval = IO_GOOD;
   } else {
      retval = IO_BAD;
   }
   return retval;
}

/******************************************************************************/
/* Return the index number of the first unused (NULL) file pointer.
 * If there are no free file pointers, then return IO_FOPEN_MAX.
 */
int inoutput_free_file_pointer_()
{
   int retval = 0;
 
   while(retval <= IO_FOPEN_MAX && pFILEfits[retval] != NULL) retval++;
   return retval;
}

/******************************************************************************/
/* 
 * Open a file for reading or writing.
 * A file number for the file pointer is returned in *pFilenum.
 *
 * Return IO_GOOD if the file was successfully opened, IO_BAD otherwise.
 * Failure can be due to either lack of free file pointers, or
 * the file not existing if pPriv=="r" for reading.
 */
int inoutput_open_file
  (int   *  pFilenum,
   char     pFileName[],
   char     pPriv[])
{
   int      iChar;
   int      retval;
   char     tempName[IO_FORTRAN_FL];

   if ((*pFilenum = inoutput_free_file_pointer_()) == IO_FOPEN_MAX) {
      printf("ERROR: Too many open files\n");
      retval = IO_BAD;
   } else {
      /* Truncate the Fortran-passed file name with a null,
       * in case it is padded with spaces */
      iChar = IO_FORTRAN_FL;
      strncpy(tempName, pFileName, iChar);
      for (iChar=0; iChar < IO_FORTRAN_FL; iChar++) {
         if (tempName[iChar] == ' ') tempName[iChar] = '\0';
      }

      retval = IO_GOOD;
      if (pPriv[0] == 'r') {
         if (inoutput_file_exist(tempName) == IO_GOOD) {
            if ((pFILEfits[*pFilenum] = fopen(tempName, pPriv)) == NULL) {
               printf("ERROR: Error opening file: %s\n", tempName);
               retval = IO_BAD;
            }
         } else {
            printf("ERROR: File does not exist: %s\n", tempName);
            retval = IO_BAD;
         }
      } else {
         if ((pFILEfits[*pFilenum] = fopen(tempName, pPriv)) == NULL) {
            printf("ERROR: Error opening file: %s\n", tempName);
            retval = IO_BAD;
         }
      }
   }

   return retval;
}

/******************************************************************************/
/*
 * Close a file.  Free the file pointer so it can be reused.
 * Return IO_BAD if an error is encountered; otherwise return IO_GOOD.
 */
int inoutput_close_file
  (int      filenum)
{
   int      retval;

   if (fclose(pFILEfits[filenum]) == EOF) {
      retval = IO_BAD;
   } else {
      retval = IO_GOOD;
   }
   pFILEfits[filenum] = NULL;

   return retval;
}

