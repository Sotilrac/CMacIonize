/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2016 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * CMacIonize is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CMacIonize is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file TemperatureCalculator.cpp
 *
 * @brief TemperatureCalculator implementation.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "TemperatureCalculator.hpp"
#include "Abundances.hpp"
#include "ChargeTransferRates.hpp"
#include "DensityGrid.hpp"
#include "DensityGridTraversalJobMarket.hpp"
#include "DensityValues.hpp"
#include "IonizationStateCalculator.hpp"
#include "LineCoolingData.hpp"
#include "PhysicalConstants.hpp"
#include "RecombinationRates.hpp"
#include "WorkDistributor.hpp"
#include <cmath>

/**
 * @brief Constructor.
 *
 * @param luminosity Total ionizing luminosity of all photon sources (in s^-1).
 * @param abundances Abundances.
 * @param pahfac PAH heating factor.
 * @param crfac Cosmic ray heating factor.
 * @param crlim Upper limit on the neutral fraction below which cosmic ray
 * heating is applied to a cell.
 * @param crscale Scale height of the cosmic ray heating term (0 for a constant
 * heating term; in m).
 * @param line_cooling_data LineCoolingData use to calculate cooling due to line
 * emission.
 * @param recombination_rates RecombinationRates used to calculate ionic
 * fractions.
 * @param charge_transfer_rates ChargeTransferRates used to calculate ionic
 * fractions.
 * @param log Log to write logging info to.
 */
TemperatureCalculator::TemperatureCalculator(
    double luminosity, const Abundances &abundances, double pahfac,
    double crfac, double crlim, double crscale,
    const LineCoolingData &line_cooling_data,
    const RecombinationRates &recombination_rates,
    const ChargeTransferRates &charge_transfer_rates, Log *log)
    : _luminosity(luminosity), _abundances(abundances), _pahfac(pahfac),
      _crfac(crfac), _crlim(crlim), _crscale(crscale),
      _line_cooling_data(line_cooling_data),
      _recombination_rates(recombination_rates),
      _charge_transfer_rates(charge_transfer_rates) {

  if (log) {
    log->write_status("Set up TemperatureCalculator with total luminosity ",
                      _luminosity, " s^-1, PAH factor ", _pahfac,
                      ", and cosmic ray factor ", _crfac, " (limit: ", _crlim,
                      ", scale height: ", _crscale, " m).");
  }
}

/**
 * @brief Function that calculates the cooling and heating rate for a given
 * cell, together with the ionization balance.
 *
 * The process occurs in four steps: first we compute the ionization balance of
 * hydrogen and helium at the given temperature, using the same algorithm that
 * is used in IonizationStateCalculator. Once we know the neutral fractions of
 * hydrogen and helium, we also know the number of free electrons (since
 * coolants contribute a negligible amount of electrons due to their low
 * abundances). This allows us to compute heating terms in the second step,
 * which involve the heating integrals, but also the number density of free
 * electrons.
 *
 * In the third step, we use our knowledge about the densities of electrons and
 * neutral and ionized hydrogen and helium to compute the ionization balance for
 * the coolants. These balances are set by the mean ionizing intensities and
 * recombination rates at the given temperature, but also involve charge
 * transfer ionization and recombination due to interactions with hydrogen and
 * helium.
 *
 * In the fourth and final step, we use our knowledge of the ionization state of
 * the coolants to compute actual cooling rates.
 *
 * @param h0 Variable to store the hydrogen neutral fraction in.
 * @param he0 Variable to store the helium neutral fraction in.
 * @param gain Total energy gain due to heating.
 * @param loss Total energy loss due to cooling.
 * @param T Temperature (in K).
 * @param cell Cell for which we compute the ionization equilibrium and cooling
 * and heating.
 * @param j Mean ionizing intensity integrals (in s^-1).
 * @param abundances Abundances.
 * @param h Heating integrals (in J s^-1).
 * @param pahfac Normalization factor for PAH heating.
 * @param crfac Normalization factor for cosmic ray heating.
 * @param crscale Scale height of the cosmic ray heating term (0 for a constant
 * heating term; in m).
 * @param data LineCoolingData used to calculate line cooling.
 * @param rates RecombinationRates used to calculate ionic fractions.
 * @param ctr ChargeTransferRates used to calculate ionic fractions.
 */
void TemperatureCalculator::ioneng(
    double &h0, double &he0, double &gain, double &loss, double T,
    DensityGrid::iterator &cell, const double j[NUMBER_OF_IONNAMES],
    const Abundances &abundances, const double h[NUMBER_OF_HEATINGTERMS],
    double pahfac, double crfac, double crscale, const LineCoolingData &data,
    const RecombinationRates &rates, const ChargeTransferRates &ctr) {

  /// step 0: initialize some variables

  // get a reference to the ionization variables of the cell
  // this will be used to set the ionic fractions of the coolants, and to get
  // access to the number density
  IonizationVariables &ionization_variables = cell.get_ionization_variables();

  // get the recombination rates of all elements at the selected temperature
  const double alphaH = rates.get_recombination_rate(ION_H_n, T);
  const double alphaHe = rates.get_recombination_rate(ION_He_n, T);
  const double alphaC[2] = {rates.get_recombination_rate(ION_C_p1, T),
                            rates.get_recombination_rate(ION_C_p2, T)};
  const double alphaN[3] = {rates.get_recombination_rate(ION_N_n, T),
                            rates.get_recombination_rate(ION_N_p1, T),
                            rates.get_recombination_rate(ION_N_p2, T)};
  const double alphaO[2] = {rates.get_recombination_rate(ION_O_n, T),
                            rates.get_recombination_rate(ION_O_p1, T)};
  const double alphaNe[2] = {rates.get_recombination_rate(ION_Ne_n, T),
                             rates.get_recombination_rate(ION_Ne_p1, T)};
  const double alphaS[3] = {rates.get_recombination_rate(ION_S_p1, T),
                            rates.get_recombination_rate(ION_S_p2, T),
                            rates.get_recombination_rate(ION_S_p3, T)};

  // mean intensity integrals
  const double jH = j[ION_H_n];
  const double jHe = j[ION_He_n];
  const double jCp1 = j[ION_C_p1];
  const double jCp2 = j[ION_C_p2];
  const double jNn = j[ION_N_n];
  const double jNp1 = j[ION_N_p1];
  const double jNp2 = j[ION_N_p2];
  const double jOn = j[ION_O_n];
  const double jOp1 = j[ION_O_p1];
  const double jNen = j[ION_Ne_n];
  const double jNep1 = j[ION_Ne_p1];
  const double jSp1 = j[ION_S_p1];
  const double jSp2 = j[ION_S_p2];
  const double jSp3 = j[ION_S_p3];

  // heating integrals
  const double hH = h[HEATINGTERM_H];
  const double hHe = h[HEATINGTERM_He];

  // number density in the cell
  const double n = ionization_variables.get_number_density();

  // some frequently used expressions involving the temperature
  // we precompute them here to increase the efficiency
  const double T4 = T * 1.e-4;
  const double sqrtT = std::sqrt(T);
  const double logT = std::log(T);

  // helium abundance. Used to scale the helium number density.
  const double AHe = abundances.get_abundance(ELEMENT_He);

  /// step 1: get the ionization equilibrium for hydrogen and helium

  IonizationStateCalculator::find_H0(alphaH, alphaHe, jH, jHe, n, AHe, T, h0,
                                     he0);

  // the ionization equilibrium gives us the electron density (we neglect free
  // electrons coming from ionization of coolants)
  const double ne = n * (1. - h0 + AHe * (1. - he0));

  // make sure the electron density is a number
  cmac_assert(ne == ne);

  // we also need the number densities of H+ and He+
  const double nhp = n * (1. - h0);
  const double nhep = (1. - he0) * n * AHe;

  // we precompute some frequently used products of number densities
  const double nenhp = ne * nhp;
  const double nenhep = ne * nhep;

  /// step 2: heating
  // the heating consists of 4 terms:
  //  - heating by ionization of hydrogen and helium
  //  - on the spot heating by absorption by hydrogen of He Lyman alpha
  //    radiation
  //  - PAH heating (if active)
  //  - cosmic ray heating (if active)

  // ionization heating
  gain = n * (hH * h0 + hHe * AHe * he0);

  // He Lyman alpha on the spot heating
  // Wood, Mathis & Ercolano (2004), equation 25
  // NOTE that this is a different expression from the one in Kenny's code!
  // Kenny's expression is in units cm^3 s^-1, we multiplied by 1.e-6 to convert
  // to m^3 s^-1
  const double alpha_e_2sP = 4.17e-20 * std::pow(T4, -0.861);
  // Wood, Mathis & Ercolano (2004), equation 17
  // we extracted the factor 10^4 from the square root and multiplied it with
  // the constant 0.77
  const double pHots = 1. / (1. + 77. / sqrtT * he0 / h0);
  // the constant factor is the energy gain due to a helium Lyman alpha photon
  // being absorbed by hydrogen: (21.2 eV - 13.6 eV) = 1.21765423e-18 J
  gain += pHots * 1.21765423e-18 * alpha_e_2sP * nenhep;

  // PAH heating
  // the numerical factors were estimated from Weingartner, J. C. & Draine, B.
  // T. 2001, ApJS, 134, 263 (http://adsabs.harvard.edu/abs/2001ApJS..134..263W)
  // as the net heating-cooling rate for a full black body star (tables 4 and 5)
  // we multiplied Kenny's value with 1.e-12 to convert densities to m^-3
  // we then multiplied with 0.1 to convert to J m^-3s^-1
  gain += 1.5e-37 * n * ne * pahfac;

  // cosmic ray heating
  // erg/cm^(9/2)/s --> J/m^(9/2)/s ==> 1.2e-27 --> 1.2e-25
  // value comes from equation (53) in Wiener, J., Zweibel, E. G. & Oh, S. P.
  // 2013, ApJ, 767, 87 (http://adsabs.harvard.edu/abs/2013ApJ...767...87W)
  double heatcr = 0.;
  if (crfac > 0.) {
    heatcr = crfac * 1.2e-25 / std::sqrt(ne);
    if (crscale > 0.) {
      heatcr *= std::exp(-std::abs(cell.get_cell_midpoint().z()) / crscale);
    }
  }
  gain += heatcr;

  /// step 3: ionization balance of coolants

  // we first compute the ionic fractions of the different ions of the coolants
  // they are then used as input for the line cooling routine
  // the procedure is always the same: the total density for an element X with
  // ionization states X0, X+, X2+... is
  //   n(X) = n(X0) + n(X+) + n(X2+) + ...
  // the ionization balance for each ion is given by
  //   n(X+)rec(X+) = n(X0)ion(X+)
  // or
  //   n(X) = n(X0)ion(X+)/rec(X+) = n(X0)C(X+)
  // recombination from X2+ to X0 happens in two stages, so the recombination
  // rate from X2+ to X0 is the product of the recombination rates from X2+ to
  // X+ and from X+ to X0
  // We want the ionic fractions n(X+)/n(X), so
  //  n(X+)/n(X) = n(X0)C(X+) / (n(X0) + n(X+) + n(X2+) + ...)
  //             = n(X0)C(X+) / (n(X0) + n(X0)C(X+) + n(X+)C(X2+) + ...)
  //             = n(X0)C(X+) / (n(X0) + n(X0)C(X+) + n(X0)C(X+)C(X2+) + ...)
  //             = C(X+) / (1 + C(X+) + C(X+)C(X2+) + ...)

  // we precompute the number density of neutral hydrogen and neutral helium
  const double nh0 = n * h0;
  const double nhe0 = n * he0 * AHe;

  // carbon
  // the charge transfer recombination rates for C+ are negligble
  const double C21 = jCp1 / (ne * alphaC[0]);
  const double C32 =
      jCp2 /
      (ne * alphaC[1] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_C_p2, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_C_p2, T4));
  const double C31 = C32 * C21;
  const double sumC_inv = 1. / (1. + C21 + C31);
  ionization_variables.set_ionic_fraction(ION_C_p1, C21 * sumC_inv);
  ionization_variables.set_ionic_fraction(ION_C_p2, C31 * sumC_inv);

  // nitrogen
  const double N21 =
      (jNn + nhp * ctr.get_charge_transfer_ionization_rate_H(ION_N_n, T4)) /
      (ne * alphaN[0] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_N_n, T4));
  const double N32 =
      jNp1 /
      (ne * alphaN[1] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_N_p1, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_N_p1, T4));
  const double N43 =
      jNp2 /
      (ne * alphaN[2] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_N_p2, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_N_p2, T4));
  const double N31 = N32 * N21;
  const double N41 = N43 * N31;
  const double sumN_inv = 1. / (1. + N21 + N31 + N41);
  ionization_variables.set_ionic_fraction(ION_N_n, N21 * sumN_inv);
  ionization_variables.set_ionic_fraction(ION_N_p1, N31 * sumN_inv);
  ionization_variables.set_ionic_fraction(ION_N_p2, N41 * sumN_inv);

  // Oxygen
  const double O21 =
      (jOn + nhp * ctr.get_charge_transfer_ionization_rate_H(ION_O_n, T4)) /
      (ne * alphaO[0] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_O_n, T4));
  const double O32 =
      jOp1 /
      (ne * alphaO[1] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_O_p1, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_O_p1, T4));
  const double O31 = O32 * O21;
  const double sumO_inv = 1. / (1. + O21 + O31);
  ionization_variables.set_ionic_fraction(ION_O_n, O21 * sumO_inv);
  ionization_variables.set_ionic_fraction(ION_O_p1, O31 * sumO_inv);

  // Neon
  const double Ne21 = jNen / (ne * alphaNe[0]);
  const double Ne32 =
      jNep1 /
      (ne * alphaNe[1] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_Ne_p1, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_Ne_p1, T4));
  const double Ne31 = Ne32 * Ne21;
  const double sumNe_inv = 1. / (1. + Ne21 + Ne31);
  ionization_variables.set_ionic_fraction(ION_Ne_n, Ne21 * sumNe_inv);
  ionization_variables.set_ionic_fraction(ION_Ne_p1, Ne31 * sumNe_inv);

  // Sulphur
  const double S21 =
      jSp1 / (ne * alphaS[0] +
              nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_S_p1, T4));
  const double S32 =
      jSp2 /
      (ne * alphaS[1] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_S_p2, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_S_p2, T4));
  const double S43 =
      jSp3 /
      (ne * alphaS[2] +
       nh0 * ctr.get_charge_transfer_recombination_rate_H(ION_S_p3, T4) +
       nhe0 * ctr.get_charge_transfer_recombination_rate_He(ION_S_p3, T4));
  const double S31 = S32 * S21;
  const double S41 = S43 * S31;
  const double sumS_inv = 1. / (1. + S21 + S31 + S41);
  ionization_variables.set_ionic_fraction(ION_S_p1, S21 * sumS_inv);
  ionization_variables.set_ionic_fraction(ION_S_p2, S31 * sumS_inv);
  ionization_variables.set_ionic_fraction(ION_S_p3, S41 * sumS_inv);

  /// step 4: cooling
  // the cooling consists of three term:
  //  - cooling by recombination of coolants (C, N, O, Ne, S)
  //  - cooling due to free-free radiation (bremsstrahlung)
  //  - cooling due to recombination of hydrogen and helium

  // coolants
  // get the abundances required by LineCoolingData and feed them to that class
  double abund[12];
  abund[0] = abundances.get_abundance(ELEMENT_N) *
             (1. - ionization_variables.get_ionic_fraction(ION_N_n) -
              ionization_variables.get_ionic_fraction(ION_N_p1) -
              ionization_variables.get_ionic_fraction(ION_N_p2));
  abund[1] = abundances.get_abundance(ELEMENT_N) *
             ionization_variables.get_ionic_fraction(ION_N_n);
  abund[2] = abundances.get_abundance(ELEMENT_O) *
             (1. - ionization_variables.get_ionic_fraction(ION_O_n) -
              ionization_variables.get_ionic_fraction(ION_O_p1));
  abund[3] = abundances.get_abundance(ELEMENT_O) *
             ionization_variables.get_ionic_fraction(ION_O_n);
  abund[4] = abundances.get_abundance(ELEMENT_O) *
             ionization_variables.get_ionic_fraction(ION_O_p1);
  abund[5] = abundances.get_abundance(ELEMENT_Ne) *
             ionization_variables.get_ionic_fraction(ION_Ne_p1);
  abund[6] = abundances.get_abundance(ELEMENT_S) *
             (1. - ionization_variables.get_ionic_fraction(ION_S_p1) -
              ionization_variables.get_ionic_fraction(ION_S_p2) -
              ionization_variables.get_ionic_fraction(ION_S_p3));
  abund[7] = abundances.get_abundance(ELEMENT_S) *
             ionization_variables.get_ionic_fraction(ION_S_p1);
  abund[8] = abundances.get_abundance(ELEMENT_C) *
             (1. - ionization_variables.get_ionic_fraction(ION_C_p1) -
              ionization_variables.get_ionic_fraction(ION_C_p2));
  abund[9] = abundances.get_abundance(ELEMENT_C) *
             ionization_variables.get_ionic_fraction(ION_C_p1);
  abund[10] = abundances.get_abundance(ELEMENT_N) *
              ionization_variables.get_ionic_fraction(ION_N_p1);
  abund[11] = abundances.get_abundance(ELEMENT_Ne) *
              ionization_variables.get_ionic_fraction(ION_Ne_n);

  loss = data.get_cooling(T, ne, abund) * n;

  // free-free cooling (bremsstrahlung)

  // fit to the free-free emission Gaunt factor from Katz, N., Weinberg, D. H. &
  // Hernquist, L. 1996, ApJS, 105, 19
  // (http://adsabs.harvard.edu/abs/1996ApJS..105...19K), equation 23
  const double c = 5.5 - logT;
  const double gff = 1.1 + 0.34 * std::exp(-c * c / 3.);
  // Wood, Mathis & Ercolano (2004), equation 22
  // based on section 3.4 of Osterbrock, D. E. & Ferland, G. J. 2006,
  // Astrophysics of Gaseous Nebulae and Active Galactic Nuclei, 2nd edition
  // (http://adsabs.harvard.edu/abs/2006agna.book.....O)
  loss += 1.42e-40 * gff * sqrtT * (nenhp + nenhep);

  // cooling due to recombination of hydrogen and helium

  // we multiplied Kenny's value with 1.e-12 to convert the densities into m^-3
  // we then multiplied with 0.1 to convert them to J m^-3s^-1
  // expressions come from Black, J. H. 1981, MNRAS, 197, 553
  // (http://adsabs.harvard.edu/abs/1981MNRAS.197..553B), table 3
  // valid in the range [5,000 K; 50,000 K]
  // NOTE that the expression for helium is different from that in Kenny's code
  // (it is the same as the commented out expression in Kenny's code)
  const double Lhp =
      2.85e-40 * nenhp * sqrtT * (5.914 - 0.5 * logT + 0.01184 * std::cbrt(T));
  const double Lhep = 1.55e-39 * nenhep * std::pow(T, 0.3647);
  loss += Lhp + Lhep;
}

/**
 * @brief Calculate a new temperature for the given cell.
 *
 * This method iteratively determines a new temperature for the cell by starting
 * from an initial guess and computing cooling and heating rates until the net
 * energy change becomes negligible. For every temperature guess, we can compute
 * the ionization balance of hydrogen and helium and the coolants, which is then
 * used to obtain cooling and heating rates.
 *
 * @param jfac Normalization factor for the mean intensity integrals.
 * @param hfac Normalization factor for the heating integrals.
 * @param cell DensityGrid::iterator pointing to a cell.
 */
void TemperatureCalculator::calculate_temperature(
    double jfac, double hfac, DensityGrid::iterator &cell) const {

  // parameters that control the iteration
  // could potentially be made into real parameters for better control
  const double eps = 1.e-3;
  const unsigned int max_iterations = 100;

  IonizationVariables &ionization_variables = cell.get_ionization_variables();

  // if the ionizing intensity is 0, the gas is trivially neutral and all
  // coolants are in the ground state
  if ((ionization_variables.get_mean_intensity(ION_H_n) == 0. &&
       ionization_variables.get_mean_intensity(ION_He_n) == 0.) ||
      ionization_variables.get_number_density() == 0.) {
    ionization_variables.set_temperature(500.);

    ionization_variables.set_ionic_fraction(ION_H_n, 1.);

    ionization_variables.set_ionic_fraction(ION_He_n, 1.);

    ionization_variables.set_ionic_fraction(ION_C_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_C_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_N_n, 1.);
    ionization_variables.set_ionic_fraction(ION_N_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_N_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_O_n, 1.);
    ionization_variables.set_ionic_fraction(ION_O_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_Ne_n, 1.);
    ionization_variables.set_ionic_fraction(ION_Ne_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_S_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p2, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p3, 0.);

    return;
  }

  // if cosmic ray heating is active, check if the gas is ionized enough
  // if it is not, we just assume the gas is neutral and do not apply heating
  double h0, he0;
  if (_crfac > 0.) {
    const double alphaH =
        _recombination_rates.get_recombination_rate(ION_H_n, 8000.);
    const double alphaHe =
        _recombination_rates.get_recombination_rate(ION_He_n, 8000.);
    const double jH = jfac * ionization_variables.get_mean_intensity(ION_H_n);
    const double jHe = jfac * ionization_variables.get_mean_intensity(ION_He_n);
    const double nH = ionization_variables.get_number_density();
    const double AHe = _abundances.get_abundance(ELEMENT_He);
    IonizationStateCalculator::find_H0(alphaH, alphaHe, jH, jHe, nH, AHe, 8000.,
                                       h0, he0);
    if (_crfac > 0. && h0 > _crlim) {
      // assume fully neutral
      ionization_variables.set_temperature(500.);
      ionization_variables.set_ionic_fraction(ION_H_n, 1.);
      ionization_variables.set_ionic_fraction(ION_He_n, 1.);

      ionization_variables.set_ionic_fraction(ION_C_p1, 0.);
      ionization_variables.set_ionic_fraction(ION_C_p2, 0.);

      ionization_variables.set_ionic_fraction(ION_N_n, 1.);
      ionization_variables.set_ionic_fraction(ION_N_p1, 0.);
      ionization_variables.set_ionic_fraction(ION_N_p2, 0.);

      ionization_variables.set_ionic_fraction(ION_O_n, 1.);
      ionization_variables.set_ionic_fraction(ION_O_p1, 0.);

      ionization_variables.set_ionic_fraction(ION_Ne_n, 1.);
      ionization_variables.set_ionic_fraction(ION_Ne_p1, 0.);

      ionization_variables.set_ionic_fraction(ION_S_p1, 0.);
      ionization_variables.set_ionic_fraction(ION_S_p2, 0.);
      ionization_variables.set_ionic_fraction(ION_S_p3, 0.);
      return;
    }
  }

  // we make sure our initial temperature guess is high enough
  double T0 = ionization_variables.get_temperature();
  if (ionization_variables.get_temperature() <= 4000.) {
    T0 = 8000.;
  }

  // normalize the mean intensity integrals
  double j[NUMBER_OF_IONNAMES];
  for (int i = 0; i < NUMBER_OF_IONNAMES; ++i) {
    IonName ion = static_cast< IonName >(i);
    j[i] = jfac * ionization_variables.get_mean_intensity(ion);
  }

  // normalize the heating integrals
  double h[NUMBER_OF_HEATINGTERMS];
  for (int i = 0; i < NUMBER_OF_HEATINGTERMS; ++i) {
    HeatingTermName heating_term = static_cast< HeatingTermName >(i);
    h[i] = hfac * ionization_variables.get_heating(heating_term);
  }

  // iteratively find the equilibrium temperature by starting from a guess and
  // computing the ionization equilibrium and cooling and heating for that guess
  // based on the net cooling and heating we can then find a new temperature
  // guess, until the difference between cooling and heating drops below a
  // threshold value
  // we enforce upper and lower limits on the temperature of 10^10 and 500 K
  unsigned int niter = 0;
  double gain0 = 1.;
  double loss0 = 0.;
  h0 = 0.;
  he0 = 0.;
  while (std::abs(gain0 - loss0) > eps * gain0 && niter < max_iterations) {
    ++niter;
    const double T1 = 1.1 * T0;
    // ioneng
    double h01, he01, gain1, loss1;
    ioneng(h01, he01, gain1, loss1, T1, cell, j, _abundances, h, _pahfac,
           _crfac, _crscale, _line_cooling_data, _recombination_rates,
           _charge_transfer_rates);

    const double T2 = 0.9 * T0;
    // ioneng
    double h02, he02, gain2, loss2;
    ioneng(h02, he02, gain2, loss2, T2, cell, j, _abundances, h, _pahfac,
           _crfac, _crscale, _line_cooling_data, _recombination_rates,
           _charge_transfer_rates);

    // ioneng - this one sets h0, he0, gain0 and loss0
    ioneng(h0, he0, gain0, loss0, T0, cell, j, _abundances, h, _pahfac, _crfac,
           _crscale, _line_cooling_data, _recombination_rates,
           _charge_transfer_rates);

    const double logtt = std::log(T1 / T2);
    const double expgain = std::log(gain1 / gain2) / logtt;
    const double exploss = std::log(loss1 / loss2) / logtt;
    T0 *= std::pow(loss0 / gain0, 1. / (expgain - exploss));

    if (T0 < 4000.) {
      // gas is neutral, temperature is 500 K
      T0 = 500.;
      h0 = 1.;
      he0 = 1.;
      // force exit out of loop
      gain0 = 1.;
      loss0 = 1.;
    }

    if (T0 > 1.e10) {
      // gas is ionized, temperature is 10^10 K (should probably be a lower
      // value)
      T0 = 1.e10;
      h0 = 1.e-10;
      he0 = 1.e-10;
      // force exit out of loop
      gain0 = 1.;
      loss0 = 1.;
    }
  }
  if (niter == max_iterations) {
    cmac_warning("Maximum number of iterations reached (temperature: %g, "
                 "relative difference cooling/heating: %g, aim: %g)!",
                 T0, std::abs(loss0 - gain0) / gain0, eps);
  }

  // cap the temperature at 30,000 K, since helium charge transfer rates are
  // only valid until 30,000 K
  T0 = std::min(30000., T0);

  // update the ionic fractions and temperature
  ionization_variables.set_temperature(T0);

  // now make sure the results make physical sense: if the mean ionizing
  // intensity for hydrogen or helium was zero, then that element should be
  // completely neutral
  if (ionization_variables.get_mean_intensity(ION_H_n) == 0.) {
    h0 = 1.;
  }
  if (ionization_variables.get_mean_intensity(ION_He_n) == 0.) {
    he0 = 1.;
  }

  ionization_variables.set_ionic_fraction(ION_H_n, h0);
  ionization_variables.set_ionic_fraction(ION_He_n, he0);

  // if hydrogen is completely neutral, then we assume that all coolants are
  // neutral as well
  if (h0 == 1.) {
    ionization_variables.set_ionic_fraction(ION_C_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_C_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_N_n, 1.);
    ionization_variables.set_ionic_fraction(ION_N_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_N_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_O_n, 1.);
    ionization_variables.set_ionic_fraction(ION_O_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_Ne_n, 1.);
    ionization_variables.set_ionic_fraction(ION_Ne_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_S_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p2, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p3, 0.);
  }

  // if hydrogen is completely ionized, then we assume that all coolants are
  // in very high ionization states as well
  if (h0 <= 1.e-10) {
    ionization_variables.set_ionic_fraction(ION_C_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_C_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_N_n, 0.);
    ionization_variables.set_ionic_fraction(ION_N_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_N_p2, 0.);

    ionization_variables.set_ionic_fraction(ION_O_n, 0.);
    ionization_variables.set_ionic_fraction(ION_O_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_Ne_n, 0.);
    ionization_variables.set_ionic_fraction(ION_Ne_p1, 0.);

    ionization_variables.set_ionic_fraction(ION_S_p1, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p2, 0.);
    ionization_variables.set_ionic_fraction(ION_S_p3, 0.);
  }
}

/**
 * @brief Calculate a new temperature for each cell in the given block after
 * shooting the given number of photons.
 *
 * This is done in parallel.
 *
 * @param totweight Total weight of all photons that were used.
 * @param grid DensityGrid on which to operate.
 * @param block Block that should be traversed by the local MPI process.
 */
void TemperatureCalculator::calculate_temperature(
    double totweight, DensityGrid &grid,
    std::pair< unsigned long, unsigned long > &block) const {

  // get the normalization factors for the ionizing intensity and heating
  // integrals (they depend on the total weight of the photons)
  double jfac = _luminosity / totweight;
  // the integral calculation uses the photon frequency (in Hz)
  // we want to convert this to the photon energy (in Joule)
  // we do this by multiplying with the Planck constant (in Js)
  double hfac =
      jfac * PhysicalConstants::get_physical_constant(PHYSICALCONSTANT_PLANCK);

  WorkDistributor<
      DensityGridTraversalJobMarket< TemperatureCalculatorFunction >,
      DensityGridTraversalJob< TemperatureCalculatorFunction > >
      workers;
  TemperatureCalculatorFunction do_calculation(*this, jfac, hfac);
  DensityGridTraversalJobMarket< TemperatureCalculatorFunction > jobs(
      grid, do_calculation, block);
  workers.do_in_parallel(jobs);
}
