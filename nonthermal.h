#ifndef NONTHERMAL_H
#define NONTHERMAL_H

// #include "sn3d.h"
#include "types.h"
#include <stdio.h>

void nt_init(int my_rank);
void nt_close_file(void);
void nt_solve_spencerfano(int modelgridindex, int timestep, int iteration);
double nt_ionization_ratecoeff(int modelgridindex, int element, int ion);
void calculate_deposition_rate_density(int modelgridindex, int timestep);
double get_deposition_rate_density(int modelgridindex);
float get_nt_frac_heating(int modelgridindex);
double nt_excitation_ratecoeff(int modelgridindex, int lineindex);
void do_ntlepton(PKT *pkt_ptr);
void nt_write_restart_data(FILE *gridsave_file);
void nt_read_restart_data(FILE *gridsave_file);
void nt_MPI_Bcast(int my_rank, int root, int root_nstart, int root_ndo);

#endif //NONTHERMAL_H
