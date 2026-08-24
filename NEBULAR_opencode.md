ARTIS — Nebular‑phase spectrum  (step‑by‑step README)
1. Quick “copy‑paste” workflow  
# 1️⃣  Get a build environment
#    (gcc 14+, Open MPI, and the pixi env are recommended)
export MAKEFLAGS="--check-symlink-times --jobs=$(nproc --all)"

# 2️⃣  Select the nebular‑NLTE preset
ln -sf artisoptions_nltenebular.h artisoptions.h   # symlink (keeps in sync)

# 3️⃣  Build the two executables
make REPRODUCIBLE=ON MAX_NODE_SIZE=2 FASTMATH=OFF -j$(nproc) sn3d exspec

# 4️⃣  Prepare a test‑run folder
cd tests
source ./setup_nebular_1d_3dgrid.sh          # creates tests/nebular_1d_3dgrid_testrun/
cd ..
rm -f artisoptions.h                         # remove the symlink we just created
cp tests/nebular_1d_3dgrid_testrun/artisoptions.h .   # copy preset into repo root

# 5️⃣  Run the radiation‑transfer simulation
cd tests/nebular_1d_3dgrid_testrun
mpirun -np 4 --oversubscribe ./sn3d -o job0        # → job0/output_0-0.txt …
md5sum -c results_md5_job0.txt

# 6️⃣  Resume / final run
cp input-resume.txt input.txt                      # keep restart state
mpirun -np 4 --oversubscribe ./sn3d -o job1
rm *.tmp
mpirun -np 1 ./exspec                              # → spectra / light curves
python3 ../../scripts/mergeangleres.py
rm -f light_curve_res_*.out spec_res_*.out specpol_res_*.out
md5sum -c results_md5_final.txt
Why the rm/cp dance?  
artisoptions.h is normally a symlink to a preset. cp writes through the symlink, which would overwrite the tracked preset. Removing the symlink first, then copying the preset, avoids that.
2. Prerequisites
Item	Minimum version
Compiler	gcc 14 (or clang, nvc++, hipcc)
MPI	mpicxx supplied by Open MPI
Eigen	pruned copy in third_party/ (already checked out)
Python 3 (for exspec post‑processing)	≥ 3.9
artistools (optional, for plotting)	pip package artistools
Set the MPI compiler:
export OMPI_CXX=g++      # if you have several C++ compilers
3. Choosing a compile‑time preset
The file artisoptions.h must exist and point at one of the artisoptions_*.h presets.  
For a nebular NLTE run use:
ln -sf artisoptions_nltenebular.h artisoptions.h
Other useful presets:
Preset
artisoptions_classic.h
artisoptions_kilonova_lte.h
artisoptions_christinenonthermal.h
artisoptions_nltenebular.h
artisoptions_nltewithoutnonthermal.h
artisoptions_nltephotospheric_dynamic_ion_range.h
All options are documented in artisoptions_doc.md (see the fenced C++ block).  
Never copy the preset – keep it as a symlink so that a branch change automatically updates artisoptions.h.
4. Building
The Makefile supports many configuration flags; the most common for a nebular run are:
make REPRODUCIBLE=ON MAX_NODE_SIZE=2 FASTMATH=OFF -j$(nproc) sn3d exspec
Flag	Effect
REPRODUCIBLE=ON	-DREPRODUCIBLE=true -ffp-contract=off -DEIGEN_DONT_VECTORIZE; also forces FASTMATH=OFF
FASTMATH=OFF	-DEIGEN_FAST_MATH=0 – guarantees bit‑identical results across CPUs
MAX_NODE_SIZE=N	passes -DMAX_NODE_SIZE=N; the build directory gets a _maxnodesizeN suffix
TESTMODE=ON	adds sanitizers, extra asserts; not needed for production
GPU=ON / STDPAR=ON	for GPU off‑loading (nvc++/hipcc) – not required for CPU runs
The build puts objects into build/<compiler>-<version>_<arch>/<options>/ and creates symlinks sn3d, exspec, unittests in the repo root that point at the newest binary.
If you change a compile‑time option, run make clean first, then rebuild.
5. Input‑file layout for a nebular run
The code expects a set of plain‑text files in the run directory (or in data/). The most important ones are:
File	Content
model.txt	Spatial grid, mass, temperature, density, etc.
abundances.txt	Elemental abundances (mass fractions)
transitiondata.txt	Radiative & collisional rates (line data)
input-newrun.txt (or input.txt)	Simulation control: MPKTS, Tmin, Tmax, ntimes, o …
input-resume.txt	Restart state; copied to input.txt when input.txt is missing
gridsave (optional)	Restart dump from a previous run
The setup script tests/setup_nebular_1d_3dgrid.sh creates a minimal but complete set of these files (model, abundances, transition data, and a default input-newrun.txt). Running that script is the easiest way to get a working input bundle.
Tip: Do not delete input.txt between runs unless you truly want a fresh start; the code will copy input-newrun.txt automatically and log a line.
6. Running the simulation (sn3d)
mpirun -np <N> --oversubscribe ./sn3d -o <jobfolder>
- -o job0 (or job1, …) moves per‑job output files into a sub‑folder job0/, job1/, …  
- Without -o the log files land directly in the run directory (output_<rank>-<thread>.txt).
What happens inside sn3d
Phase
Initialisation – read model, set up grids, allocate NLTE/NT arrays
Loop over timesteps – for each timestep: <br>1. Update hydrodynamics (if applicable) <br>2. Solve electron temperature Te (energy balance) <br>3. Spencer‑Fano (non‑thermal if NT_ON) <br>4. NLTE level populations (rate‑matrix solve) <br>5. Packet propagation & estimator accumulation
Final output – write gridsave, packet files, spectra accumulators
Key runtime options (from artisoptions_nltenebular.h)
Option	Meaning
NT_ON (bool, line 102)	Turns the non‑thermal (Cosmic‑ray / alpha‑particle) module on/off. Set true for nebular runs with non‑thermal electrons.
NT_SOLVE_SPENCERFANO (bool, line 104)	Use the full Spencer‑Fano solver instead of the simple work‑function approximation. Recommended true when NT_ON.
NLTEITER (int, line 21)	Maximum number of NLTE iteration loops per timestep (default 30).
NLTE_USE_GTH_SOLVER (bool, line 100)	If true, uses the Grassmann‑Taksar‑Heyman (GTH) state‑elimination solver – more robust for large dynamic range.
STRICT_POPULATION_CHECKING (bool, line 90)	Adds extra checks for negative populations / inversions; on by default in the nebular preset.
TEMPERATURE_SOLVER_ACCURACY (double, line 123)	Convergence tolerance for the temperature‑coupling loop (1e‑3 default).
DETAILED_BF_ESTIMATORS_ON (bool, line 80)	Switch on detailed bound‑free rate estimators (important for NLTE level populations).
DETAILED_LINE_ESTIMATORS_ON (bool, line 78)	Switch on detailed line estimators.
FIRST_NLTE_RADFIELD_TIMESTEP (int, line 70)	First timestep at which the radiative field is included in the NLTE rate calculations.
7. Post‑processing with exspec
After one or more sn3d runs have produced the packet files (pkt*.tmp, gridsave), combine them into spectra and light curves:
mpirun -np 1 ./exspec          # single‑process post‑processor
exspec reads the accumulated estimators from all ranks, merges angle‑dependent bins (via scripts/mergeangleres.py), and writes:
Output file
linestat.out
spec_res_*.out
light_curve_res_*.out
specpol_res_*.out (if POL_ON)
gamma_light_curve.out (if KEEP_ESCAPED_GAMMAS)
After exspec finishes, the script scripts/mergeangleres.py merges the angle bins into a final set (spec_res_final.out, etc.) that are the files checked by CI.
8. Verifying the run – checksums
CI stores reference md5 checksums in:
- tests/<testname>_inputfiles/results_md5_job0.txt
- tests/<testname>_inputfiles/results_md5_final.txt
After a successful run:
md5sum -c results_md5_job0.txt
md5sum -c results_md5_final.txt
If a checksum fails, it usually means:
1. A different compile‑time preset or option was used (e.g. FASTMATH ON).  
2. The run was performed with a different number of MPI ranks or threads (non‑reproducible accumulation).  
3. A numerical change that is physically intended (e.g. turning on a new solver option).
When a change does alter the results deliberately, the maintainer must regenerate the reference checksums via the “Update checksums” workflow (a maintainer‑only CI job). Mention the expected change in the commit message.
9. Physics — NLTE level‑population solver
9.1. What the solver does
The NLTE (Non‑Local Thermodynamic Equilibrium) solver finds the steady‑state level populations for each ion under the assumption of statistical equilibrium:
\[
\sum_{j} N_j R_{ji} = \sum_{j} N_i R_{ij}
\]
where \(N_j\) are the level populations and \(R_{ij}\) the total rate (radiative + collisional) from level i to j. The solver:
1. Assembles a rate matrix \(A\) (size = total NLTE levels in the element).  
- Diagonal entries = negative sum of all outgoing rates from that level.  
- Off‑diagonal entries = incoming rates (bound‑bound lines, bound‑free continua, photo‑ionisation, recombination, non‑thermal ionisation if NT_ON).
2. Adds a normalisation row (the sum of all populations = 1) so the linear system can be solved.  
3. Solves \(A \mathbf{N} = \mathbf{b}\) for the population vector \(\mathbf{N}\).  
- Default: LU decomposition (simple, fast).  
- Alternative: GTH algorithm (NLTE_USE_GTH_SOLVER = true) – exploits the matrix being the transpose of a Markov‑chain generator; gives component‑wise relative accuracy even when populations span many orders of magnitude, no equilibration needed.
4. Applies strict checking (if STRICT_POPULATION_CHECKING) to reject unphysical solutions (negative populations, strong inversions). In that case the solver either falls back to LTE populations or strips the problematic ion stages (NLTE_LIMIT_ION_STAGES_AFTER_FAILURE).
The ** NLTE ionisation and population solver is described by Shingles et al. (2020, MNRAS, 492, 2029)**, section 2.3.
9.2. Inputs the solver uses
Source	What it provides
model.txt + abundances.txt	Electron temperature (T_e), density, elemental composition.
transitiondata.txt	Radiative rates (photo‑ionisation, line absorption), collisional excitation/de‑excitation coefficients (from ratecoeff.cc).
NT_ON + Spencer‑Fano output	Non‑thermal electron energy degradation → extra ionisation/excitation rates (ntcoll_bf, ntcoll_bb in the rate matrix).
DETAILED_BF_ESTIMATORS_ON	Per‑continuum bound‑free rate estimators that are accumulated during the packet propagation and then folded into the rate matrix.
RADFIELDBINS_*	Frequency‑binned radiation field used for photo‑ionisation rates.
All these rates are gathered per cell (nonemptymgi) and then solved independently for each element/ion group.
9.3. Restart data
The NLTE populations are written to gridsave (binary) and read back on subsequent runs. The file contains the full population vector for every non‑empty model cell. Restart allows a simulation to continue from the same ionisation state without recomputing the full rate matrix from scratch.
10. Physics — Spencer‑Fano non‑thermal electron transport
10.1. Origin
The Spencer‑Fano (SF) equation (Spencer & Fano, 1954, *Phys. Rev.* 93, 1172) describes the energy loss of a fast lepton (electron, positron, alpha) moving through a partially ionised plasma. The integral form (KF92 equation 7, extended by Shingles et al. 2020) is:
\[
\frac{d}{dE}\!\left S(E) \, y(E) \right + \int_{E}^{E_{\max}} S(E') \, y(E') \, \frac{dE'}{E'} = -\,L(E)
\]
where  
- \(S(E)\) – stopping power (energy loss per unit path length) due to collisions with thermal electrons.  
- \(y(E)\) – degradation spectrum: probability that an electron of initial energy \(E_0\) will have energy \(E\) after some distance.  
- \(L(E)\) – loss term that includes Coulomb losses and excitation/ionisation of atoms/ions.
The solution \(y(E)\) yields fractions of the total deposited energy that go into:
- Heating (goes into raising the electron temperature).  
- Ionisation (creates new ions).  
- Excitation (excites bound electrons; later used by the NLTE solver).
10.2. Discretisation in ARTIS
- Energy grid: SFPTS points uniformly (or logarithmically) between SF_EMIN and SF_EMAX (set at the top of nonthermal.cc; currently ~100 points spanning a few hundred eV to a few keV, but the exact values are not configurable per‑preset).  
- Upper‑triangle matrix stores the coefficients of the discretised SF equation; only the upper triangle is kept (uppertriangular(i,j)).  
- Matrix assembly (in solve_spencerfano, nonthermal.cc:2678‑2709):
- Diagonal terms: continuous Coulomb loss rate electron_loss_rate(E_i, n_e).  
- Excitation terms: summed per “excitation band” (group of transitions sharing the same energy width).  
- Ionisation terms: added for each ion stage using the ion‑stage specific ionisation rate (sfmatrix_add_ionisation).  
- Auger contribution (SF_AUGER_CONTRIBUTION_ON): an extra source term that allows an ion to lose more than one electron in a single impact (relevant for high‑Z ions).
- Solution: a back‑substitution on the upper‑triangular system (sfmatrix_solve) yields the vector yfunc.  
- Post‑processing (analyse_sf_solution): computes the three fractions (frac_heating, frac_ionisation, frac_excitation) and stores the electron‑per‑ion ratio nneperion_when_solved for convergence‑checking.
10.3. Convergence & triggering
The SF solver is not called at every timestep; it is triggered when:
Condition
timestep < globals::num_lte_timesteps + 1
Deposition rate ≤ 0
nne_per_ion changed less than NT_MAX_FRACDIFF_NNEPERION_BETWEEN_SOLUTIONS (default 0.05) and the last solution was within SF_MAX_TIMESTEPS_BETWEEN_SOLUTIONS timesteps
Otherwise
The convergence check simply tests whether the electron fraction has drifted beyond the prescribed tolerance; no iterative resolution of the SF equation itself is required because the linear system has a unique solution (the matrix is diagonally dominant after the Coulomb terms are added).
10.4. Output stored in gridsave
The restart file contains (per cell):
Quantity
SFPTS, SF_EMIN, SF_EMAX
nneperion_when_solved
frac_heating, frac_ionisation, frac_excitation
eff_ionpot (effective ionisation potential)
Auger probabilities & fractions
NT excitation list size + per‑excitation frac_deposition, ratecoeffperdeposition, alltransindex
These data are read back on restart, so the non‑thermal module can continue from a previous solution without re‑solving the whole SF equation unless the electron fraction has changed enough.
11. Reproducibility checklist
Item	Why it matters
REPRODUCIBLE=ON	Forces -ffp-contract=off -DEIGEN_DONT_VECTORIZE and FASTMATH=OFF.
FASTMATH=OFF (automatic when REPRODUCIBLE=ON)	Prevents -ffast-math from altering floating‑point order.
MPI rank count	The accumulation of estimators is rank‑wise; different ranks give different sums.
Threading	OpenMP/STDPAR creates separate generator states per thread → non‑reproducible.
Compiler & version	-Werror, -march=native, etc. affect instruction selection.
MAX_NODE_SIZE	Changes the linear‑system size used in the GTH/LU solvers; must be consistent.
NLTEITER	Maximum NLTE iterations per timestep; more iterations can change final populations.
NT_SOLVE_SPENCERFANO	On/off switches the SF solver; obviously changes results.
When all of the above are satisfied, the md5 checksums in results_md5_*.txt should match the reference files word‑for‑word.
12. Quick “cheat‑sheet” command line
# -------------------------------------------------
# 0. Environment (once)
export MAKEFLAGS="--check-symlink-times --jobs=$(nproc --all)"
ln -sf artisoptions_nltenebular.h artisoptions.h

# 1. Build
make REPRODUCIBLE=ON MAX_NODE_SIZE=2 FASTMATH=OFF -j$(nproc) sn3d exspec

# 2. Prepare test folder
cd tests
source ./setup_nebular_1d_3dgrid.sh       # creates test run dir
cd ..
rm -f artisoptions.h
cp tests/nebular_1d_3dgrid_testrun/artisoptions.h .

# 3. Run sn3d (job0)
cd tests/nebular_1d_3dgrid_testrun
mpirun -np 4 --oversubscribe ./sn3d -o job0
md5sum -c results_md5_job0.txt

# 4. Resume / final run
cp input-resume.txt input.txt
mpirun -np 4 --oversubscribe ./sn3d -o job1
rm -f *.tmp
mpirun -np 1 ./exspec
python3 ../../scripts/mergeangleres.py
rm -f light_curve_res_*.out spec_res_*.out specpol_res_*.out
md5sum -c results_md5_final.txt

# 5. (Optional) regenerate checksums if you intentionally changed physics
#    – only a maintainer can run the “Update checksums” workflow.
# -------------------------------------------------
13. Where to find more details
Topic	File(s)
Full option list	artisoptions_doc.md
Spencer‑Fano implementation	nonthermal.cc (lines 2618‑2716)
NLTE rate‑matrix assembly	nltepop.cc (lines 1‑150 + later sections)
Rate coefficient library	ratecoeff.cc/.h
Grid indices & empty‑cell tests	grid.h / grid.cc
Restart / gridsave format	nonthermal.cc write_restart_data / read_restart_data
Unit‑test references	unittests.cc
🎯 Bottom line
1. Symlink the nebular NLTE preset, build with REPRODUCIBLE=ON.  
2. Run sn3d with mpirun; the code internally couples NLTE level populations (rate‑matrix solve) and Spencer‑Fano non‑thermal electron transport.  
3. Post‑process with exspec → spectra/light curves.  
4. Compare the md5 checksums to the reference files; any intentional change must be documented and the references regenerated.


NLTE level‑population solver (ARTIS)
Purpose – Determine the steady‑state populations of all bound levels of every ion in each model cell under the assumption of statistical equilibrium.  The result feeds the radiation field, line opacities, and the non‑thermal module.
1.  Rate‑matrix assembly  (nltepop.cc ≈ lines 60‑200)
For a given element and a given cell (nonemptymgi) the solver builds a square matrix A whose dimension equals the number of NLTE levels of that element (ground + excited, optionally a super‑level).  
- Diagonal  A_ii  = – ∑ (outgoing rates from level i).
- Off‑diagonal A_ij = incoming rate from level j to i (radiative bound‑bound, bound‑free photo‑ionisation, recombination, collisional excitation/de‑excitation).
The matrix is stored in a RateMatrices struct that reserves space once (max_nlte_dimension²) and later resizes to the actual used_nlte_dimension.
2.  Rate contributors
Source	What it adds
Radiative bound‑bound	line absorption / emission using transitiondata.txt
Radiative bound‑free	photo‑ionisation cross‑sections, frequency‑binned radiation field (RADFIELDBINS_*)
Collisional	excitation, de‑excitation, ionisation, recombination from ratecoeff.cc
Non‑thermal (if NT_ON)	extra ionisation/excitation rates from the Spencer‑Fano solution (ntcoll_bf, ntcoll_bb)
Detailed estimators (if DETAILED_BF_ESTIMATORS_ON)	per‑continuum bound‑free rates accumulated during packet propagation
Saha / GTH constraint	a normalisation row ensures ∑ N_i = 1
3.  Solver choice
- Default (LU decomposition) – standard Gaussian elimination with a normalisation row. Fast for moderate dimensions (< ≈ 200).
- GTH (Grassmann‑Taksar‑Heyman) solver – activated with NLTE_USE_GTH_SOLVER = true. Exploits the fact that the rate matrix is the transpose of a Markov‑chain generator; gives component‑wise relative accuracy even when populations span many orders of magnitude, no equilibration needed. Elements with FORCE_SAHA_ION_BALANCE always use LU because the Saha rows break the generator structure.
The solver is called from solve_nlte_pops_element() (nltepop.cc ≈ line ~300) which receives the cell index, the current timestep, and the iteration counter.
4.  Convergence & iteration loop
Inside update_grid.cc (solve_Te_nltepops, line 187) the following loop runs per timestep:
for nlte_iter = 0 … NLTEITER   // NLTEITER = 30 in the nebular preset
    solve_nlte_pops_element(...)   // assembles A, solves for populations
    check convergence (fracdiff, population limits)
    if converged or nlte_iter == NLTEITER break
end
- STRICT_POPULATION_CHECKING (default true in the nebular preset) validates that no population is negative and that no level inversion exceeds the factor STRICT_POPULATION_CHECKING_INVERSION_FACTOR_SOLVER_FAIL (≈ 50).  
- On failure the solver can either (a) fallback to LTE populations, or (b) progressively strip the problematic ion stages via NLTE_LIMIT_ION_STAGES_AFTER_FAILURE.
5.  Output
- Populations are written to the binary gridsave file (per cell, per element).  
- On restart the stored populations are read back, so the solver does not need to recompute from scratch unless the electron temperature or ionising radiation field changes.
6.  Key compile‑time options (from artisoptions_nltenebular.h)
Option
NLTEITER (30)
NLTE_USE_GTH_SOLVER (false)
STRICT_POPULATION_CHECKING (true)
NLTE_LIMIT_ION_STAGES_AFTER_FAILURE (true)
STRICT_POPULATION_CHECKING_INVERSION_FACTOR_SOLVER_FAIL (50)
DETAILED_BF_ESTIMATORS_ON (true)
FIRST_NLTE_RADFIELD_TIMESTEP (12)
7.  Quick checklist for a reproducible nebular run
- Use REPRODUCIBLE=ON (forces FASTMATH=OFF).  
- Keep the same NLTEITER, NLTE_USE_GTH_SOLVER, and strict‑checking flags as the reference case.  
- Run with the same MPI rank count (e.g. 4) and no OpenMP/STDPAR threads, because threading changes the accumulation order.
Bottom line – The NLTE solver builds a per‑cell rate matrix that couples radiative, collisional, and non‑thermal processes, solves the linear system (LU or GTH), enforces a normalisation condition, and optionally applies strict population checks. Its output (level populations) is the backbone for subsequent radiation‑transfer estimating and for the non‑thermal Spencer‑Fano module.
▣  Build · Nemotron 3.5 Lightning Free · 38.9s
For each element such rate matrices are built?
+ Thought: 2.6s
Yes – a separate rate matrix is built for every element (and for every model cell that contains that element).  
Here’s how it works in the ARTIS source:
Where	What happens
nltepop.cc – solve_nlte_pops_element() (≈ line 300)	The function is called once per non‑empty model cell (nonemptymgi). Inside it iterates over the elements present in that cell (get_nelements()).
Per‑element loop	For each element the code creates (or re‑uses) a RateMatrices object and fills its vectors (rad_bb, coll_bb, ntcoll_bb, rad_bf, coll_bf, ntcoll_bf, autoion, summed_rates). The dimension of the matrix equals the number of NLTE levels of that element (ground + excited levels, optionally a super‑level).
Rates assembled	• Radiative bound‑bound (lines from transitiondata.txt)<br>• Radiative bound‑free (photo‑ionisation using the binned radiation field)<br>• Collisional (excitation, de‑excitation, ionisation, recombination from ratecoeff.cc)<br>• Non‑thermal (if NT_ON) – extra ionisation/excitation from the Spencer‑Fano solution<br>• Detailed bound‑free estimators (if DETAILED_BF_ESTIMATORS_ON)
Normalisation row	A final row is added so that the sum of all level populations equals 1. This makes the system solvable (the matrix is singular without it).
Solver	The linear system A·N = b is solved either by LU decomposition (default) or by the GTH algorithm (NLTE_USE_GTH_SOLVER = true).
Result	The population vector N is stored back into globals::nltepops_allcells for that cell and element, and written to the restart file (gridsave).
Because the matrix size and the set of rates differ from element to element (different ions, different number of levels, different atomic data), each element gets its own matrix every timestep (or every NLTE iteration). The code deliberately re‑uses the pre‑allocated RateMatrices storage to avoid repeated large allocations, but the contents are recomputed per element per cell.
In short:  
- One matrix per element per cell per NLTE iteration.  
- The matrix size = total NLTE levels of that element (ground + excited + optional super‑level).  
- The matrix is assembled from radiative, collisional, and (if enabled) non‑thermal rates, then solved for the level populations.
▣  Build · Nemotron 3.5 Lightning Free · 18.8s
hat are NLTE levels and LTE levels? 
+ Thought: 4.9s
NLTE levels vs. LTE levels in ARTIS
Concept	What it means physically
LTE (Local Thermodynamic Equilibrium) levels	In a region where collisions dominate, the level populations are given by the Boltzmann distribution (for excited levels) and the Saha ionisation equation, all at the local electron temperature (T_e) and electron density (n_e).  No explicit rate‑matrix calculation is needed – the populations are derived from a few thermodynamic quantities.
NLTE (Non‑Local Thermodynamic Equilibrium) levels	The radiation field is often far from a black‑body, and photo‑ionisation, resonant scattering, or non‑thermal processes can strongly depopulate or populate certain levels.  The populations are determined by statistical equilibrium – a balance between all radiative and collisional rates feeding each level.
Which levels are “NLTE”?	In the code the ground state plus a set of excited states are flagged as NLTE.  The number of such levels per ion is given by the compile‑time function ION_NLEVELS_EXCITED_NLTE(element_z, ionstage) (e.g. 197 for Fe II in the nebular preset).  If a super‑level is used (to compress many close‑lying levels into one effective level) it is also counted in total_nlte_levels.  Levels that are not flagged remain LTE‑type (or are simply ignored).
Relationship	• If NLTE_ON is off, every level population is LTE (computed from T_e, density, and ionisation balance).  <br>• If NLTE_ON is on, the solver replaces the LTE populations with NLTE populations for the flagged levels; the remaining (usually the ground state or very tightly bound levels) may still be described by LTE or be forced to match the NLTE solution depending on the preset options (STRICT_POPULATION_CHECKING, NLTE_LIMIT_ION_STAGES_AFTER_FAILURE, etc.).
Quick “look‑under‑the‑hood” code pointers
File / function	What you’ll see
globals.h / globals.cc	int total_nlte_levels; – total number of NLTE levels across the model.
input.cc::setup_nlte_levels() (≈ line 1810)	Loops over elements/ions, calls elem_has_nlte_levels_search(), fills elem.has_nlte_levels and increments total_nlte_levels.
nltepop.h struct RateMatrices	Holds the per‑element rate‑matrix data; dimension = used_nlte_dimension² where used_nlte_dimension = number of NLTE levels for the current element.
nltepop.cc::solve_nlte_pops_element()	Builds the matrix, chooses LU vs GTH, solves A·N = b, returns the population vector that is stored back into nltepops_allcells.
artisoptions_nltenebular.h line 21 NLTEITER and line 100 NLTE_USE_GTH_SOLVER	Compile‑time knobs that control the maximum NLTE iterations and the solver type.
Bottom line
- LTE levels → populations from Boltzmann/Saha, no rate‑matrix, used when NLTE is off.
- NLTE levels → a selected set of excited (and possibly ground) levels whose populations are obtained by solving a statistical‑equilibrium rate matrix that includes radiative, collisional, and (optionally) non‑thermal processes.
In practice a nebular ARTIS run with NT_ON and NLTE_ON will have both: the NLTE solver updates the populations of the flagged levels, while any levels not flagged keep their LTE values (or are simply discarded).
▣  Build · Nemotron 3.5 Lightning Free · 42.7s