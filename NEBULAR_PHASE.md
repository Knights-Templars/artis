# Run ARTIS for a supernova nebular phase

This guide describes a small supernova calculation from 170 to 230 days.
It uses the `nebular_1d_3dgrid` test as a worked example.

The model has three spherical shells from the W7 supernova model.
ARTIS maps these shells to a Cartesian propagation grid.
The example uses non-local thermodynamic equilibrium (NLTE) populations and non-thermal deposition.

## Quick start

Run these commands from the repository root.

```sh
cd tests
source ./setup_nebular_1d_3dgrid.sh
cd ..

rm -f artisoptions.h
cp tests/nebular_1d_3dgrid_testrun/artisoptions.h .

export OMPI_CXX=g++
make clean
make REPRODUCIBLE=ON MAX_NODE_SIZE=2 FASTMATH=OFF sn3d exspec

cp sn3d exspec tests/nebular_1d_3dgrid_testrun/
cd tests/nebular_1d_3dgrid_testrun

mpirun -np 4 --oversubscribe ./sn3d -o job0
md5sum -c results_md5_job0.txt

cp input-resume.txt input.txt
mpirun -np 4 --oversubscribe ./sn3d -o job1

rm *.tmp
mpirun -np 1 ./exspec
python3 ../../scripts/mergeangleres.py
rm -f light_curve_res_*.out spec_res_*.out specpol_res_*.out
md5sum -c results_md5_final.txt
```

The setup script downloads approximately 15 MB of atomic data.
It keeps the archive at `tests/atomicdata_feconi.tar.xz`.

Do not copy through an `artisoptions.h` symlink.
The copy operation can change the tracked preset file.
Remove the symlink before the copy operation.

### State of the example folder

The present `tests/nebular_1d_3dgrid_testrun` folder contains the first job.
Its log ends with `RESTART_NEEDED` after timestep 7.
Its `input.txt` requests a restart at timestep 7.

The folder also contains spectra that `exspec` made from these partial packet files.
These files do not match `results_md5_final.txt`.
Run the resume job before you use the final checksum file.

The present folder does not contain the `job0` and `job1` output folders.
Run the complete quick-start sequence to create them.

## Program entry points

The simulation starts at `main()` in `sn3d.cc`.
The spectrum calculation starts at `main()` in `exspec.cc`.

`sn3d` accepts these useful options:

- `-o JOBFOLDER` puts the log and cell output files in `JOBFOLDER`.
- `-w HOURS` makes a restart before the wall time expires.
- `-h` prints the command help.

`exspec` currently requires one Message Passing Interface (MPI) process.
It reads packet files from all processes of the `sn3d` run.

## Data that the calculation needs

ARTIS searches the run folder, `data/`, and `artis/data/` for each input file.
The setup script creates an `artis` symlink to the repository root.

The worked example needs these main files:

| File | Purpose | Main reader |
|---|---|---|
| `input.txt` | The timestep range, time limits, restart state, and process count | `read_parameterfile()` in `input.cc` |
| `model.txt` | The shell boundaries, density, initial energy, and nuclide mass fractions | `grid::read_ejecta_model()` in `grid.cc` |
| `abundances.txt` | The elemental mass fractions of each model cell | `read_elem_abundances()` in `grid.cc` |
| `compositiondata.txt` | The included elements, ions, level limits, and atomic masses | `read_atomicdata()` in `input.cc` |
| `adata.txt` | The atomic levels | `read_atomicdata()` in `input.cc` |
| `transitiondata.txt` | The bound-bound transitions | `read_atomicdata()` in `input.cc` |
| `phixsdata_v2.txt` | The photoionisation cross-sections | `read_atomicdata()` in `input.cc` |
| `recombrates.txt` | Optional recombination rate scale factors | `ratecoefficients_init()` in `ratecoeff.cc` |
| `collion.txt` | The collisional ionisation data for the non-thermal solver | `nonthermal.cc` |
| `collion-auger.txt` | The Auger ionisation data | `nonthermal.cc` |
| `binding_energies.txt` | The electron binding energies | `nonthermal.cc` |
| decay files | The decay properties and gamma-ray lines | `decay.cc` |

The atomic archive supplies most of these files.
For this example, it supplies data for iron, cobalt, and nickel.

### The simple model

The example `model.txt` starts with three values:

```text
3
0.000231481
```

The first value gives three spherical model cells.
The second value gives the model snapshot time in days.
Each remaining line gives one shell.

The default 1D columns are the cell identifier, outer velocity, and logarithmic density.
The later columns give `X_Fegroup`, `X_Ni56`, `X_Co56`, and `X_Fe52` in this file.
The input reader can also use a header line with explicit column names.

`abundances.txt` has one line for each model cell.
The first field is the cell identifier.
The later fields are elemental mass fractions in atomic-number order.

`compositiondata.txt` selects Fe, Co, and Ni for this test.
It also sets the included ion stages and the maximum level count.

Check these model conditions before a new run:

- Use positive and outward-increasing shell velocities.
- Use the same cell identifiers in `model.txt` and `abundances.txt`.
- Make each elemental mass fraction consistent with its tracked isotope mass fractions.
- Include each model element in `compositiondata.txt` and the atomic data.
- Give the density at the snapshot time on the second line of `model.txt`.

### The time grid

The worked `input-newrun.txt` has ten timesteps from 170 to 230 days.
It first runs timesteps 0 through 7 because `timestep_finish` is 8.
The restart file runs the remaining timesteps.

The first four timesteps use an LTE ionisation balance.
The test preset enables the NLTE and non-thermal calculations after this initial interval.
The setup script changes both detailed radiation-field thresholds to timestep 7.

`input.txt` is positional.
Keep all placeholder lines and their order.
Use `input-newrun.txt` as the annotated source for a new run.

## Compile-time nebular options

The base preset is `artisoptions_nltenebular.h`.
The setup script copies it and changes values for a short test.

The important features are:

- `NT_ON` enables non-thermal energy deposition.
- `NT_SOLVE_SPENCERFANO` enables the Spencer-Fano solution.
- `NT_EXCITATION_ON` enables non-thermal excitation.
- `MULTIBIN_RADFIELD_MODEL_ON` enables the binned radiation-field model.
- `DETAILED_BF_ESTIMATORS_ON` enables detailed bound-free estimators.
- `NLTEITER` sets the maximum NLTE iteration count.
- `MPKTS` sets the packet count for each MPI process.
- `GRID_TYPE_OVERRIDE` can map a 1D model to a 3D propagation grid.

The test uses one million packets for each MPI process.
Four processes therefore propagate four million packets.
Increase this count for a science calculation and test the Monte Carlo convergence.

## Function execution path

The following path gives the main control flow.

```text
sn3d.cc: main()
  read_parameterfile()
  read_atomicdata()
  grid::read_ejecta_model()
  ratecoefficients_init()
  setup_timesteps()
  grid::init_grid()
  packet_init()
  for each timestep:
    do_timestep()
      update_grid()
        decay::update_abundances()
        update_grid_cell() for each nonempty model cell
          solve temperatures and the radiation field
          solve the ion populations
          solve Spencer-Fano when the options permit it
          calculate opacities and cooling rates
      mpi_communicate_grid_properties()
      zero_estimators()
      update_packets()
        sort packets by their cell cache
        do_packet() for each active packet
      mpi_reduce_estimators()
      write_deposition_file()
      write_partial_lightcurve_spectra()
      write final packet files at the last timestep
```

`do_packet()` selects a function from the packet type:

- A radioactive pellet calls `update_pellet()`.
- A gamma-ray packet calls `gammapkt::do_gamma()`.
- A radiation packet calls `do_rpkt()`.
- A deposited non-thermal packet calls a function in `nonthermal.cc`.
- A thermal packet calls a function in `kpkt.cc`.

The radiation packet path handles line, continuum, and electron interactions.
The macro-atom functions select internal transitions and deactivation channels.

The estimator dependency is important.
Packet propagation in timestep N collects estimators for the grid update in timestep N+1.
`mpi_reduce_estimators()` sums these values across all MPI processes.
`update_grid()` then uses the summed values for the next cell solution.

The first timestep has no estimator history.
ARTIS assigns initial temperatures and uses the initial LTE treatment.

### MPI execution

Each MPI process owns `MPKTS` packets.
Each process can propagate its packets through every propagation cell.

The grid update uses a different division.
Each process solves only its assigned model cells.
ARTIS then communicates the new cell properties to all processes.

The test has three nonempty model cells and four MPI processes.
The output therefore has three `estimators_*.out` files.
The packet output has four `packets00_*.out` files.

## From packets to a spectrum

At the final timestep, `sn3d` writes one packet file for each MPI process.
For example, process zero writes `packets00_0000.out`.

`exspec` executes this path:

```text
exspec.cc: main()
  read_parameterfile()
  read_atomicdata()
  grid::read_ejecta_model()
  setup_timesteps()
  read_text_packets() for each sn3d process
  do_direction_bin()
    add_to_lc_res()
    add_to_spec_res()
    write_light_curve()
    write_spectra()
```

`spec.out` contains an angle-averaged flux density at 1 Mpc.
The first row contains zero and the timestep midpoint values in days.
Each later row contains a frequency in hertz and one flux value per timestep.

`light_curve.out` contains three columns.
They are the midpoint in days, rest-frame luminosity, and comoving-frame luminosity.
The luminosity unit is solar luminosity.

`emission.out`, `emissiontrue.out`, and `absorption.out` separate the process contributions.
Use these files to identify the ions and processes that make a spectral feature.

The 1D input model uses a 3D Cartesian propagation grid in this test.
This configuration produces direction bins.
`mergeangleres.py` combines the direction files for analysis tools.

## Verify the result

Use all checks in this section.
A checksum match proves exact reproduction of the test.
It does not prove that a different physical model is correct.

### 1. Check completion and errors

```sh
rg '\[error\]|failed to converge|RESTART_NEEDED|No need for restart|sn3d finished' \
  output_0-0.txt job0/output_0-0.txt job1/output_0-0.txt

rg '\[error\]|\[warning\]|consistency check|exspec finished' exspec.txt
```

The final run must contain `No need for restart`.
An intermediate job can contain `RESTART_NEEDED`.

Read each warning in its physical context.
The current short test clamps some radiation temperatures.
It also forces one electron temperature to the configured maximum.
These warnings show the limits of this reduced test model.

### 2. Check exact test reproduction

Use the reproducible build command from the quick start.
Then run both checksum checks at the specified points.

```sh
md5sum -c results_md5_job0.txt
md5sum -c results_md5_final.txt
```

Reference checksums come from an arm64 runner with g++ 15.
A local mismatch can identify a real numerical change.
Continuous integration gives the final repository result.

### 3. Check spectrum and light-curve consistency

`exspec` integrates `spec.out` over frequency.
It compares that result with `light_curve.out`.
It writes a warning if the spectrum exceeds the light curve by more than 0.1 percent.

The spectrum covers only `NU_MIN_R` through `NU_MAX_R`.
Its integral can be less than the light curve when packets fall outside this interval.

### 4. Check energy deposition

Inspect `deposition.out` for each timestep.
Compare the Monte Carlo deposition columns with their discrete or analytic counterparts.

For a high packet count, the differences must decrease with the expected Monte Carlo noise.
Large systematic differences can show a model, decay-data, or packet-count problem.

Also compare the bolometric luminosity with the deposited radioactive power.
Allow for stored energy, expansion losses, and the finite time response.

### 5. Check the cell solutions

Inspect these files:

- `estimators_*.out` contains temperatures, electron densities, heating rates, and cooling rates.
- `nlte_*.out` contains the level populations.
- `radfield_*.out` contains the binned radiation field.
- `grid.out` contains the model-to-propagation-grid mapping.

Check for finite and positive temperatures, densities, and populations.
Check the heating and cooling balance for each converged cell.
Check the electron density and temperature changes between adjacent timesteps.

Sharp changes can be physical at an ionisation transition.
They can also identify an NLTE convergence failure.

### 6. Check Monte Carlo convergence

Repeat the model with at least two packet counts.
Keep the random seed and all other inputs constant.

Compare these quantities after a suitable wavelength rebin:

- the bolometric luminosity;
- the integrated flux of important lines;
- the continuum level;
- the cell temperatures and ion fractions.

Use larger bins than the raw spectrum bins.
Raw-bin differences mostly measure packet noise.

### 7. Check the physical model

Compare the main line ratios and luminosities with an independent calculation or observation.
For a Type Ia supernova, inspect the Fe-group forbidden-line complexes first.

Confirm that the model epoch, distance scale, reddening correction, and wavelength convention agree.
ARTIS writes frequency-space flux at 1 Mpc in `spec.out`.
Apply the correct conversion before comparison with an observed wavelength spectrum.

## Use the test as a new model template

Copy the test folder to a new run folder.
Keep the atomic files and replace these model files:

- `model.txt`;
- `abundances.txt`;
- `compositiondata.txt` when the element or ion set changes;
- `input-newrun.txt` and `input-resume.txt` for the new time range.

Change the local `artisoptions.h` only when the compile-time physics must change.
Build `sn3d` and `exspec` with that exact file.

Start with a small packet count for an input check.
Then increase the packet count for the final spectrum.
Keep the low-count result separate from the science result.
