#include "sn3d.h"
#include "gamma.h"
#include "kpkt.h"
#include "macroatom.h"
#include "packet_prop.h"
#include "rpkt.h"

void packet_prop(PKT *restrict const pkt_ptr, const double t1, const double t2, const int nts)
// Master routine for moving packets around. When it called,
//   it is given the time at start of inverval and at end - when it finishes,
//   everything the packet does during this time should be sorted out.
{
  double t_current = t1;

  /* 0 the scatter counter for the packet. */
  pkt_ptr->scat_count = 0;

  while (t_current >= 0)
  {
    /* Start by sorting out what sort of packet it is.*/
    //printout("start of packet_prop loop %d\n", pkt_ptr->type );
    const int pkt_type = pkt_ptr->type; // avoid dereferencing multiple times

    if (pkt_type == TYPE_GAMMA)
    {
      /*It's a gamma-ray packet.*/
      /* Call do_gamma. */
      //printout("gamma propagation\n");
      t_current = do_gamma(pkt_ptr, t_current, t2);
	    /* This returns a flag if the packet gets to t2 without
      changing to something else. If the packet does change it
      returns the time of change and sets everything for the
      new packet.*/
      if (t_current >= 0)
      {
        #ifdef _OPENMP
          #pragma omp atomic
        #endif
        time_step[nts].gamma_dep += pkt_ptr->e_cmf; ///***??
      }
    }
    else if (pkt_type == TYPE_RPKT)
    {
      /*It's an r-packet. */
      //printout("r-pkt propagation\n");
      t_current = do_rpkt(pkt_ptr, t_current, t2);
//       if (modelgrid[cell[pkt_ptr->where].modelgridindex].thick == 1)
//         t_change_type = do_rpkt_thickcell( pkt_ptr, t_current, t2);
//       else
//         t_change_type = do_rpkt( pkt_ptr, t_current, t2);

      if (pkt_ptr->type == TYPE_ESCAPE)
      {
        #ifdef _OPENMP
          #pragma omp atomic
        #endif
        time_step[nts].cmf_lum += pkt_ptr->e_cmf;
      }
    }
    else if (pkt_type == TYPE_EMINUS)
    {
      /*It's an electron - convert to k-packet*/
      //printout("e-minus propagation\n");
      pkt_ptr->type = TYPE_KPKT;
      #ifndef FORCE_LTE
        //kgammadep[pkt_ptr->where] += pkt_ptr->e_cmf;
      #endif
      //pkt_ptr->type = TYPE_PRE_KPKT;
      //pkt_ptr->type = TYPE_GAMMA_KPKT;
      //if (tid == 0) k_stat_from_eminus += 1;
      k_stat_from_eminus += 1;
    }
    else if (pkt_type == TYPE_KPKT || pkt_type == TYPE_PRE_KPKT || pkt_type == TYPE_GAMMA_KPKT)
    {
      /*It's a k-packet - convert to r-packet (low freq).*/
      //printout("k-packet propagation\n");

      //t_change_type = do_kpkt(pkt_ptr, t_current, t2);
      const int cellindex = pkt_ptr->where;
      if (pkt_type == TYPE_PRE_KPKT || modelgrid[cell[cellindex].modelgridindex].thick == 1)
        t_current = do_kpkt_bb(pkt_ptr, t_current, t2);
      else if (pkt_type == TYPE_KPKT)
        t_current = do_kpkt(pkt_ptr, t_current, t2, nts);
      else
      {
        printout("kpkt not of type TYPE_KPKT or TYPE_PRE_KPKT\n");
        abort();
        //t_change_type = do_kpkt_ffonly(pkt_ptr, t_current, t2);
      }
    }
    else if (pkt_type == TYPE_MA)
    {
      /*It's an active macroatom - apply transition probabilities*/
      //printout("MA-packet handling\n");

      t_current = do_ma(pkt_ptr, t_current, t2, nts);
    }
    else
    {
      printout("Unknown packet type - abort\n");
      abort();
    }

  }
}
