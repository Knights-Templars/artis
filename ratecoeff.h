#ifndef RATECOEFF_H
#define RATECOEFF_H

#include <cstdbool>

void ratecoefficients_init(void);

double select_continuum_nu(int element, int ion, int level, int upperionlevel, float T_e);

double interpolate_corrphotoioncoeff(int element, int ion, int level, int phixstargetindex, double T);

double get_spontrecombcoeff(int element, int ion, int level, int phixstargetindex, float T_e);
double get_stimrecombcoeff(int element, int lowerion, int level, int phixstargetindex, int modelgridindex);

double get_corrphotoioncoeff(int element, int ion, int level, int phixstargetindex, int modelgridindex);
double get_corrphotoioncoeff_ana(int element, int ion, int level, int phixstargetindex, int modelgridindex);

double calculate_iongamma_per_gspop(int modelgridindex, int element, int ion);
double calculate_iongamma_per_ionpop(
  int modelgridindex, float T_e, int element, int lowerion,
  bool assume_lte, bool collisional_not_radiative, bool printdebug, bool use_bfest);

double calculate_ionrecombcoeff(
  int modelgridindex, float T_e,
  int element, int upperion,
  bool assume_lte, bool collisional_not_radiative, bool printdebug,
  bool lower_superlevel_only, bool per_groundmultipletpop, bool stimonly);

extern double T_step_log;

#endif //RATECOEFF_H