/* 2007-10-30 -- MK
   Non-grey treatment of UVOIR opacity as opacity_case 4 added.
   Still not fully commented.
   Comments are marked by ///  Deactivated code by // */
/* 2007-01-17 -- MK
   Several minor modifications (some marked in the code with //MK), these include
     - global printout() routine (located in sn3d.c)
     - opacity_cases 2 and 3 added (changes in grid_init.c and update_grid.c,
       original opacity stuff was moved there from input.c) */
/* This is a code copied from Lucy 2004 paper on t-dependent supernova
   explosions. */

#include "exspec.h"
#include "sn3d.h"
#include "threadprivate.h"
#include "light_curve.h"
#include "spectrum.h"
#include "vectors.h"
#include "input.h"
#include "packet_init.h"
#include "time_init.h"

static PKT pkt[MPKTS];


/* Main - top level routine. */
int main(int argc, char** argv)
{
  FILE *packets_file;
  char filename[100];

  int my_rank = 0;
  int p = 1;

  nprocs = p;              /// Global variable which holds the number of MPI processes
  rank_global = my_rank;   /// Global variable which holds the rank of the active MPI process
  tid = 0;
  nthreads = 1;
  sprintf(filename,"exgamma.txt");
  if ((output_file = fopen(filename, "w")) == NULL)
  {
    printf("Cannot open %s.\n",filename);
    abort();
  }
  setvbuf(output_file, NULL, _IOLBF, 1);

  printout("Begining do_exspec.\n");

  /// Get input stuff
  printout("time before input %d\n",time(NULL));
  input(0);
  printout("time after input %d\n",time(NULL));
  nprocs = nprocs_exspec;

  /// Read binary packet files and create ASCII packets files out of them
  /*
  npkts=MPKTS;
  for (i = 0; i < nprocs; i++)
  {
    /// Read in the next bunch of packets to work on
    sprintf(filename,"packets%d_%d.tmp",0,i);
    printout("%s\n",filename);
    if ((packets_file = fopen(filename, "rb")) == NULL)
    //sprintf(filename,"packets%.2d_%.4d.out",0,i);
    //if ((packets_file = fopen(filename, "r")) == NULL)
    {
      printf("Cannot open packets file %s\n",filename);
      abort();
    }
    fread(&pkt[0], sizeof(PKT), npkts, packets_file);
    //read_packets(packets_file);
    /// Close the current file.
    fclose(packets_file);


    /// Read in the next bunch of packets to work on
    sprintf(filename,"packets%.2d_%.4d.out",0,i);
    printout("%s\n",filename);
    if ((packets_file = fopen(filename, "w")) == NULL)
    {
      printf("Cannot open packets file %s\n",filename);
      abort();
    }
    write_packets(packets_file);
    //read_packets(packets_file);
    /// Close the current file.
    fclose(packets_file);
  }
  abort();
  */

  /// Override some variables with values appropriate for gamma-ray spectra
  do_emission_res = 0;         /// We don't record information on gamma packet last interactions, thus create no emission/absorption files.
  nu_min_r = 0.05 * MEV / H;   /// Lower frequency boundary for gamma spectra
  nu_max_r = 4 * MEV / H;      /// Upper frequency boundary for gamma spectra
  for (int outer_iteration = 0; outer_iteration < n_out_it; outer_iteration++)
  {
    /// Initialise the grid. Call routine that sets up the initial positions
    /// and sizes of the grid cells.
    //grid_init();
    time_init();

    if ((epkts = malloc((nprocs*npkts)*sizeof(EPKT))) == NULL)
    {
      printout("[fatal] input: not enough memory to initalise escaping packets data structure ... abort\n");
      abort();
    }

    /// Loop over all packets in all the packets files of the simulation and check if
    /// a packet made it out as a rpkt or not. Escaping r-packets are stored in the
    /// epkts array, which is then used for the binning.
    int j = 0;
    for (int i = 0; i < nprocs; i++)
    {
      /// Read in the next bunch of packets to work on
      //sprintf(filename,"packets%d_%d.tmp",0,i);
      sprintf(filename,"packets%.2d_%.4d.out",0,i);
      printout("%s, %d %d\n",filename,i,nprocs);
      //if ((packets_file = fopen(filename, "rb")) == NULL)
      if ((packets_file = fopen(filename, "r")) == NULL)
      {
        printf("Cannot open packets file %s\n",filename);
        abort();
      }
      //fread(&pkt[0], sizeof(PKT), npkts, packets_file);
      read_packets(packets_file, pkt);
      fclose(packets_file);

      for (int ii = 0; ii < npkts; ii++)
      {
        PKT *pkt_ptr = &pkt[ii];
        if (pkt_ptr->type == TYPE_ESCAPE && pkt_ptr->escape_type == TYPE_GAMMA)
        {
          //printout("add packet %d\n",j);
          /// We know that a packet escaped at "escape_time". However, we have
          /// to allow for travel time. Use the formula in Leon's paper. The extra
          /// distance to be travelled beyond the reference surface is ds = r_ref (1 - mu).
          const double t_arrive = pkt_ptr->escape_time - (dot(pkt_ptr->pos, pkt_ptr->dir) / CLIGHT_PROP);
          epkts[j].arrive_time = t_arrive;

          /// Now do the cmf time.
          const double t_arrive_cmf = pkt_ptr->escape_time * sqrt(1. - (vmax * vmax / CLIGHTSQUARED));
          epkts[j].arrive_time_cmf = t_arrive_cmf;

          epkts[j].dir[0] = pkt_ptr->dir[0];
          epkts[j].dir[1] = pkt_ptr->dir[1];
          epkts[j].dir[2] = pkt_ptr->dir[2];
          epkts[j].nu_rf = pkt_ptr->nu_rf;
          epkts[j].e_rf = pkt_ptr->e_rf;
          epkts[j].e_cmf = pkt_ptr->e_cmf;
          epkts[j].emissiontype = pkt_ptr->emissiontype;
          epkts[j].absorptionfreq = pkt_ptr->absorptionfreq;
          epkts[j].absorptiontype = pkt_ptr->absorptiontype;
          epkts[j].trueemissionvelocity = pkt_ptr->trueemissionvelocity;
          j++;
        }
      }
    }
    nepkts = j;


    /// Extract angle-averaged spectra and light curves
    FILE *lc_file;
    if ((lc_file = fopen("gamma_light_curve.out", "w")) == NULL)
    {
      printout("Cannot open gamma_light_curve.out\n");
      abort();
    }
    FILE *spec_file;
    if ((spec_file = fopen("gamma_spec.out", "w")) == NULL)
    {
      printout("Cannot open spec.out\n");
      abort();
    }

    gather_spectrum(-1);
    gather_light_curve();
    write_spectrum(spec_file, NULL, NULL, NULL);
    write_light_curve(lc_file, -1);

    fclose(lc_file);
    fclose(spec_file);

    printout("finished angle-averaged stuff\n");

    /// Extract LOS dependent spectra and light curves
    if (model_type != RHO_1D_READ)
    {
      if ((lc_file = fopen("gamma_light_curve_res.out", "w")) == NULL)
      {
        printout("Cannot open gamma_light_curve_res.out\n");
        abort();
      }
      if ((spec_file = fopen("gamma_spec_res.out", "w")) == NULL)
      {
        printout("Cannot open gamma_spec_res.out\n");
        abort();
      }
      for (int i = 0; i < MABINS; i++)
      {
        gather_spectrum_res(i);
        gather_light_curve_res(i);

        write_spectrum(spec_file, NULL, NULL, NULL);
        write_light_curve(lc_file,i);

        printout("Did %d of %d angle bins.\n",i+1,MABINS);
      }
      fclose(lc_file);
      fclose(spec_file);
    }
  }

  //fclose(ldist_file);
  //fclose(output_file);

  printout("simulation finished at %d\n",time(NULL));
  fclose(output_file);

  return 0;
}


extern inline int printout(const char *restrict format, ...);
extern inline void gsl_error_handler_printout(const char *reason, const char *file, int line, int gsl_errno);
extern inline FILE *fopen_required(const char *filename, const char *mode);


/*void *my_malloc(size_t size)
{
  char *adr;
  #ifdef POWER6
    adr = &heap[heapcounter];
    heapcounter += size;
    if (heapcounter >= heapsize) adr = NULL;
  #else
//    adr = &heap[heapcounter];
//    heapcounter += size;
//    if (heapcounter >= heapsize) adr = NULL;
    adr = malloc(size);
  #endif
  return adr;
}*/
