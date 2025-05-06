// Created Sep 2018
// Sep 30 2022: MXPAR_PySEDMODEL -> 100 (was 20) for BAYESN
// Nov 20 2020: MXPAR_PySEDMODEL -> 20 (was 10) for SNEMO
// Nov 11 2021: Add BayeSN

// define pre-processor command to use python interface

#ifndef GENMAG_PySEDMODEL_H
#define GENMAG_PySEDMODEL_H


#define USE_PYTHON                          

// ===========================================
// global variables

#define MXLAM_PySEDMODEL  10000  // max wave bins to define SED
#define MXPAR_PySEDMODEL     100  // max number of params to describe SED
#define MXHOSTPAR_PySEDMODEL 20  // max number of items in NAMES_HOSTPAR
#define OPTMASK_ALLOW_C_ONLY 4096 // allow running C-code without python


// store inputs from init_genmag_PySEDMODEL
struct {
  char *PATH, *ARGLIST, *NAMES_HOSTPAR ;
  char *NAME_ARRAY_HOSTPAR[MXHOSTPAR_PySEDMODEL] ;
  int  OPTMASK;

  // stuff determined from inputs above
  char  MODEL_NAME[40] ; // e.g., BYOSED, SNEMO ....
  char  PyCLASS_NAME[60] ; // e.g., genmag_BYOSED

} INPUTS_PySEDMODEL ;


struct {
  int NLAM;           // number of bins in SED
  double *LAM, *SED;  // SED
  int EXTERNAL_ID, LAST_EXTERNAL_ID ;
  double Tobs_template; 

  int    NPAR ;
  char   **PARNAME;    // par names set during init stage
  double *PARVAL;      // par values update for each SED
} Event_PySEDMODEL ;


// ===========================================
// function declarations
void load_PySEDMODEL_CHOICE_LIST(void);

void init_genmag_PySEDMODEL(char *MODEL_NAME, char *PATH_VERSION,
			    int OPTMASK, char *ARGLIST, char *NAMES_HOSTPAR);

// xxx mark void get_MODEL_NAME_PySEDMODEL(char *PATH,char *MODEL_NAME);
void set_lamRanges_PySEDMODEL(char *MODEL_NAME);

void prepEvent_PySEDMODEL(int EXTERNAL_ID, double zCMB, 
			  int NHOSTPAR, double *HOSTPAR_LIST,
                          int NOBS_ALL, double *TOBS_LIST,
			  int NSPEC_ALL, double *TSPEC_LIST );

void genmag_PySEDMODEL(int EXTERNAL_ID, double zHEL, double zCMB, double MU, 
		       double MJDOFF, double MWEBV, int NHOSTPAR, double *HOSTPAR_LIST,
		       int IFILT, int NOBS, double *TOBS_list,
		       double *MAGOBS_list, double *MAG_TEMPLATE,
		       double *MAGERR_list );

int  fetchParNames_PySEDMODEL(char **parNameList);
void fetchParVal_PySEDMODEL(double *parVal);
void fetchSED_PySEDMODEL(int EXTERNAL_ID, int NEWEVT_FLAG, double Tobs,
			 int MXLAM, double *HOSTPAR_LIST, int *NLAM,
			 double *LAM, double *FLUX);

void INTEG_zSED_PySEDMODEL(int OPT_SPEC, int IFILT_OBS, double Tobs,
			   double zHEL, double x0,
			   double RV, double AV,
			   int NLAM, double *LAM, double *SED,
			   double *Finteg, double *Fspec, int *FLAG_Finteg );

// return spectrum for spectrograph
void genSpec_PySEDMODEL(double Tobs, double z, double MU, double MWEBV,
			double RV_host, double AV_host, // (I)
			int NHOSTPAR, double *HOSTPAR_LIST,
			double *GENFLUX_LIST,         // (O) fluxGen per bin
			double *GENMAG_LIST );        // (O) magGen per bin

void read_SALT2_template0(void); // for debug only (no python)

#endif

// ==== END ====
