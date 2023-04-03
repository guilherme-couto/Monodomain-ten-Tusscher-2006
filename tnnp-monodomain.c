/*-----------------------------------------------------
Monodomain with the ten Tusscher model 2006
Author: Guilherme Couto
FISIOCOMP - UFJF
------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <omp.h>

/*------------------------------------------------------
Electrophysiological parameters for ten Tusscher model 2006 (https://journals.physiology.org/doi/full/10.1152/ajpheart.00109.2006)
from https://tbb.bio.uu.nl/khwjtuss/SourceCodes/HVM2/Source/Main.cc - ten Tusscher code
https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3263775/ - Benchmark
and https://github.com/rsachetto/MonoAlg3D_C/blob/master/src/models_library/ten_tusscher/ten_tusscher_2006_RS_CPU.c - Sachetto MonoAlg3D
--------------------------------------------------------*/
// Constants
double R = 8314.472;        // Gas constant -> (???) [8.314472 J/(K*mol)]
double T = 310.0;           // Temperature -> K
double F = 96485.3415;      // Faraday constant -> (???) [96.4867 C/mmol]
double RTONF = 26.713761;   // R*T/F -> (???)

// Tissue properties
double beta = 1400.0;       // Surface area-to-volume ratio -> cm^-1
double Cm = 0.185;          // Cell capacitance per unit surface area -> uF/ (???)^2 (ten Tusscher)
double sigma_long = 1.334;         // Conductivity -> mS/cm
double sigma_trans = 0.176;         // Conductivity -> mS/cm
// double sigma = 0.1;         // Conductivity (isotropic) -> mS/cm (Sachetto)
// double beta = 0.14;            // Surface area-to-volume ratio -> um^-1
// double Cm = 0.185;             // Cell capacitance per unit surface area -> uF/ (???)^2 (um???) (ten Tusscher) (Sachetto)
// double sigma = 0.00001;        // Conductivity (isotropic) -> mS/um

// Intracellular volumes
double V_C = 0.016404;      // Cellular volume -> (???) [16404 um^3]
double V_SR = 0.001094;     // Sarcoplasmic reticulum volume -> (???) [1094 um^3]
double V_SS = 0.00005468;   // Subsarcolemmal space volume -> (???) [54.68 um^3]

// External concentrations
double K_o = 5.4;           // Extracellular potassium (K+) concentration -> mM
double Na_o = 140;          // Extracellular sodium (Na+) concentration -> mM
double Ca_o = 2.0;          // Extracellular calcium (Ca++) concentration -> mM

// Parameters for currents
double G_Na = 14.838;       // Maximal I_Na (sodium current) conductance -> nS/pF
double G_K1 = 5.405;        // Maximal I_K1 (late rectifier potassium current) conductance -> nS/pF
double G_to = 0.294;        // Maximal I_to (transient outward potassium current) conductance -> nS/pF (epi and M cells)
// double G_to = 0.073;        // Maximal I_to (transient outward potassium current) conductance -> nS/pF (endo cells)
double G_Kr = 0.153;        // Maximal I_Kr (rapidly activating delayed rectifier potassium current) conductance -> nS/pF
double G_Ks = 0.392;        // Maximal I_Ks (slowly activating delayed rectifier potassium current) conductance -> nS/pF (epi and endo cells)
// double G_Ks = 0.098;        // Maximal I_Ks (slowly activating delayed rectifier potassium current) conductance -> nS/pF (M cells) (Sachetto)
double p_KNa = 0.03;        // Relative I_Ks permeability to Na+ over K+ -> dimensionless
double G_CaL = 3.98e-5;     // Maximal I_CaL (L-type calcium current) conductance -> cm/ms/uF
double k_NaCa = 1000.0;     // Maximal I_NaCa (Na+/Ca++ exchanger current) -> pA/pF
double gamma_I_NaCa = 0.35; // Voltage dependence parameter of I_NaCa -> dimensionless
double K_mCa = 1.38;        // Half-saturation constant of I_NaCa for intracellular Ca++ -> mM
double K_mNa_i = 87.5;      // Half-saturation constant of I_NaCa for intracellular Na+ -> mM
double k_sat = 0.1;         // Saturation factor for I_NaCa -> dimensionless
double alpha = 2.5;         // Factor enhancing outward nature of I_NaCa -> dimensionless
double P_NaK = 2.724;       // Maximal I_NaK (Na+/K+ pump current) -> pA/pF
double K_mK = 1.0;          // Half-saturation constant of I_NaK for Ko -> mM
double K_mNa = 40.0;        // Half-saturation constant of I_NaK for intracellular Na+ -> mM
double G_pK = 0.0146;       // Maximal I_pK (plateau potassium current) conductance -> nS/pF
double G_pCa = 0.1238;      // Maximal I_pCa (plateau calcium current) conductance -> nS/pF
double K_pCa = 0.0005;      // Half-saturation constant of I_pCa for intracellular Ca++ -> mM
double G_bNa = 0.00029;     // Maximal I_bNa (sodium background current) conductance -> nS/pF
double G_bCa = 0.000592;    // Maximal I_bCa (calcium background current) conductance -> nS/pF

// Intracellular calcium flux dynamics
double V_maxup = 0.006375;  // Maximal I_up -> mM/ms
double K_up = 0.00025;      // Half-saturation constant of I_up -> mM
double V_rel = 0.102;       // Maximal I_rel conductance -> mM/ms
double k1_prime = 0.15;     // R to O and RI to I I_rel transition rate -> mM^-2*ms^-1
double k2_prime = 0.045;    // O to I  and R to RI I_rel transition rate -> mM^-1*ms^-1
double k3 = 0.06;           // O to R and I to RI I_rel transition rate -> ms^-1
double k4 = 0.005;          // I to O and RI to I I_rel transition rate -> ms^-1
double EC = 1.5;            // Half-saturation constant of k_Ca_SR -> mM
double max_SR = 2.5;        // Maximum value of k_Ca_SR -> dimensionless
double min_SR = 1.0;        // Minimum value of k_Ca_SR -> dimensionless
double V_leak = 0.00036;    // Maximal I_leak conductance -> mM/ms
double V_xfer = 0.0038;     // Maximal I_xfer conductance -> mM/ms

// Calcium buffering dynamics
double Buf_C = 0.2;         // Total cytoplasmic buffer concentration -> mM
double K_bufc = 0.001;      // Half-saturation constant of cytoplasmic buffers -> mM
double Buf_SR = 10.0;       // Total sarcoplasmic reticulum buffer concentration -> mM
double K_bufsr = 0.3;       // Half-saturation constant of sarcoplasmic reticulum buffers -> mM
double Buf_SS = 0.4;        // Total subspace buffer concentration -> mM
double K_bufss = 0.00025;   // Half-saturation constant of subspace buffer -> mM

/*----------------------------------------
Initial Conditions for epicardium cells
from https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3263775/
-----------------------------------------*/
double V_init = -85.23;       // Initial membrane potential -> mV
double X_r1_init = 0.00621;   // Initial rapid time-dependent potassium current Xr1 gate -> dimensionless
double X_r2_init = 0.4712;    // Initial rapid time-dependent potassium current Xr2 gate -> dimensionless
double X_s_init = 0.0095;     // Initial slow time-dependent potassium current Xs gate -> dimensionless
double m_init = 0.00172;      // Initial fast sodium current m gate -> dimensionless
double h_init = 0.7444;       // Initial fast sodium current h gate -> dimensionless
double j_init = 0.7045;       // Initial fast sodium current j gate -> dimensionless
double d_init = 3.373e-5;     // Initial L-type calcium current d gate -> dimensionless
double f_init = 0.7888;       // Initial L-type calcium current f gate -> dimensionless
double f2_init = 0.9755;      // Initial L-type calcium current f2 gate -> dimensionless
double fCass_init = 0.9953;   // Initial L-type calcium current fCass gate -> dimensionless
double s_init = 0.999998;     // Initial transient outward current s gate -> dimensionless
double r_init = 2.42e-8;      // Initial transient outward current r gate -> dimensionless
double Ca_i_init = 0.000126;  // Initial intracellular Ca++ concentration -> mM
double Ca_SR_init = 3.64;     // Initial sarcoplasmic reticulum Ca++ concentration -> mM
double Ca_SS_init = 0.00036;  // Initial subspace Ca++ concentration -> mM
double R_prime_init = 0.9073; // Initial ryanodine receptor -> dimensionless
double Na_i_init = 8.604;     // Initial intracellular Na+ concentration -> mM
double K_i_init = 136.89;     // Initial intracellular K+ concentration -> mM

/*----------------------------------------
Currents functions
------------------------------------------*/
// Reversal potentials for Na+, K+ and Ca++
double E_Na(double Na_i)
{
    return RTONF * log(Na_o / Na_i);
}
double E_K(double K_i)
{
    return RTONF * log(K_o / K_i);
}
double E_Ca(double Ca_i)
{
    return 0.5 * RTONF * log(Ca_o / Ca_i);
}

// Reversal potential for Ks
double E_Ks(double K_i, double Na_i)
{
    return RTONF * log((K_o + p_KNa * Na_o) / (K_i + p_KNa * Na_i));
}

// Fast sodium (Na+) current
double I_Na(double V, double m, double h, double j, double Na_i)
{
    return G_Na * (m*m*m) * h * j * (V - E_Na(Na_i));
}
double m_inf(double V)
{
    return 1.0 / ((1.0 + exp((-56.86 - V) / 9.03))*(1.0 + exp((-56.86 - V) / 9.03)));
}
double alpha_m(double V)
{
    return 1.0 / (1.0 + exp((-60.0 - V) / 5.0));
}
double beta_m(double V)
{
    return (0.1 / (1.0 + exp((V + 35.0) / 5.0))) + (0.1 / (1.0 + exp((V - 50.0) / 200.0)));
}
double tau_m(double V)
{
    return alpha_m(V) * beta_m(V);
}
double h_inf(double V)
{
    return 1.0 / ((1.0 + exp((V + 71.55) / 7.43))*(1.0 + exp((V + 71.55) / 7.43)));
}
double alpha_h(double V)
{
    if (V >= -40.0)
    {
        return 0.0;
    }
    else
    {
        return 0.057 * exp(-(80.0 + V) / 6.8);
    }
}
double beta_h(double V)
{
    if (V >= -40)
    {
        return 0.77 / (0.13 * (1.0 + exp((V + 10.66) / (-11.1))));
    }
    else
    {
        return 2.7 * exp(0.079 * V) + 3.1e5 * exp(0.3485 * V);
    }
}
double tau_h(double V)
{
    return 1.0 / (alpha_h(V) + beta_h(V));
}
double j_inf(double V)
{
    return 1.0 / ((1.0 + exp((V + 71.55) / 7.43))*(1.0 + exp((V + 71.55) / 7.43)));
}
double alpha_j(double V)
{
    if (V >= -40.0)
    {
        return 0.0;
    }
    else
    {
        return ((-25428.0 * exp(0.2444 * V) - (6.948e-6 * exp((-0.04391) * V))) * (V + 37.78)) / (1.0 + exp(0.311 * (V + 79.23)));
    }
}
double beta_j(double V)
{
    if (V >= -40.0)
    {
        return (0.6 * exp(0.057 * V)) / (1.0 + exp(-0.1 * (V + 32.0)));
    }
    else
    {
        return (0.02424 * exp(-0.01052 * V)) / (1.0 + exp(-0.1378 * (V + 40.14)));
    }
}
double tau_j(double V)
{
    return 1.0 / (alpha_j(V) + beta_j(V));
}

// L-type Ca2+ current
double I_CaL(double V, double d, double f, double f2, double fCass, double Ca_SS)   // !!!
{
    if (V < 15.0 - 1.0e-5)
    {
        return G_CaL * d * f * f2 * fCass * 4.0 * (V - 15.0) * (F*F) * (0.25 * Ca_SS * exp(2 * (V - 15.0) * F / (R * T)) - Ca_o) / (R * T * (exp(2.0 * (V - 15.0) * F / (R * T)) - 1.0));
    }
    else if (V > 15.0 + 1.0e-5)
    {
        return G_CaL * d * f * f2 * fCass * 2.0 * F * (0.25 * Ca_SS - Ca_o);
    }
}
double d_inf(double V)
{
    return 1.0 / (1.0 + exp((-8.0 - V) / 7.5));
}
double alpha_d(double V)
{
    return (1.4 / (1.0 + exp((-35.0 - V) / 13.0))) + 0.25;
}
double beta_d(double V)
{
    return 1.4 / (1.0 + exp((V + 5.0) / 5.0));
}
double gamma_d(double V)
{
    return 1.0 / (1.0 + exp((50.0 - V) / 20.0));
}
double tau_d(double V)
{
    return alpha_d(V) * beta_d(V) + gamma_d(V);
}
double f_inf(double V)
{
    return 1.0 / (1.0 + exp((V + 20.0) / 7.0));
}
double alpha_f(double V)
{
    return 1102.5 * exp(-(((V + 27.0)*(V + 27.0))) / 225.0);
}
double beta_f(double V)
{
    return 200.0 / (1.0 + exp((13.0 - V) / 10.0));
}
double gamma_f(double V)
{
    return 180.0 / (1.0 + exp((V + 30.0) / 10.0)) + 20.0;
}
double tau_f(double V)
{
    return alpha_f(V) + beta_f(V) + gamma_f(V);
}
double f2_inf(double V)
{
    return 0.67 / (1.0 + exp((V + 35.0) / 7.0)) + 0.33;
}
double alpha_f2(double V)   // !!!
{
    return 562.0 * exp(-(((V + 27.0)*(V + 27.0))) / 240.0);
}
double beta_f2(double V)
{
    return 31.0 / (1.0 + exp((25.0 - V) / 10.0));
}
double gamma_f2(double V)   // !!!
{
    return 80.0 / (1.0 + exp((V + 30.0) / 10.0));
}
double tau_f2(double V)
{
    return alpha_f2(V) + beta_f2(V) + gamma_f2(V);
}
double fCass_inf(double Ca_SS)
{
    return 0.6 / (1.0 + ((Ca_SS / 0.05)*(Ca_SS / 0.05))) + 0.4;
}
double tau_fCass(double Ca_SS)
{
    return 80.0 / (1.0 + ((Ca_SS / 0.05)*(Ca_SS / 0.05))) + 2.0;
}

// Transient outward current
double I_to(double V, double r, double s, double K_i)
{
    return G_to * r * s * (V - E_K(K_i));
}
double r_inf(double V)
{
    return 1.0 / (1.0 + exp((20.0 - V) / 6.0));
}
double tau_r(double V)
{
    return 9.5 * exp(-(((V + 40.0)*(V + 40.0))) / 1800.0) + 0.8;
}
/* for epicardial and M cells */
double s_inf(double V)
{
    return 1.0 / (1.0 + exp((V + 20.0) / 5.0));
}
double tau_s(double V)
{
    return 85.0 * exp(-(((V + 45.0)*(V + 45.0))) / 320.0) + 5.0 / (1.0 + exp((V - 20.0) / 5.0)) + 3.0;
}
/* for endocardial cells */
/*
double s_inf(double V)
{
    return 1.0 / (1.0 + exp((V + 28.0) / 5.0));
}
double tau_s(double V)
{
    return 1000.0 * exp(-(((V + 67.0)*(V + 67.0))) / 1000.0) + 8.0;
}
*/

// Slow delayed rectifier current
double I_Ks(double V, double X_s, double K_i, double Na_i)
{
    return G_Ks * (X_s*X_s) * (V - E_Ks(K_i, Na_i));
}
double x_s_inf(double V)
{
    return 1.0 / (1.0 + exp((-5.0 - V) / 14.0));
}
double alpha_x_s(double V)
{
    return 1400.0 / sqrt(1.0 + exp((5.0 - V) / 6.0));
}
double beta_x_s(double V)
{
    return 1.0 / (1.0 + exp((V - 35.0) / 15.0));
}
double tau_x_s(double V)
{
    return alpha_x_s(V) * beta_x_s(V) + 80.0;
}

// Rapid delayed rectifier current
double I_Kr(double V, double X_r1, double X_r2, double K_i)
{
    return G_Kr * sqrt(K_o / 5.4) * X_r1 * X_r2 * (V - E_K(K_i));
}
double x_r1_inf(double V)
{
    return 1.0 / (1.0 + exp((-26.0 - V) / 7.0));
}
double alpha_x_r1(double V)
{
    return 450.0 / (1.0 + exp((-45.0 - V) / 10.0));
}
double beta_x_r1(double V)
{
    return 6.0 / (1.0 + exp((V + 30.0) / 11.5));
}
double tau_x_r1(double V)
{
    return alpha_x_r1(V) * beta_x_r1(V);
}
double x_r2_inf(double V)
{
    return 1.0 / (1.0 + exp((V + 88.0) / 24.0));
}
double alpha_x_r2(double V)
{
    return 3.0 / (1.0 + exp((-60.0 - V) / 20.0));
}
double beta_x_r2(double V)
{
    return 1.12 / (1.0 + exp((V - 60.0) / 20.0));
}
double tau_x_r2(double V)
{
    return alpha_x_r2(V) * beta_x_r2(V);
}

// Inward rectifier K+ current
double alpha_K1(double V, double K_i)
{
    return 0.1 / (1.0 + exp(0.06 * (V - E_K(K_i) - 200.0)));
}
double beta_K1(double V, double K_i)
{
    return (3.0 * exp(0.0002 * (V - E_K(K_i) + 100.0)) + exp(0.1 * (V - E_K(K_i) - 10.0))) / (1.0 + exp(-0.5 * (V - E_K(K_i))));
}
double x_K1_inf(double V, double K_i)
{
    return alpha_K1(V, K_i) / (alpha_K1(V, K_i) + beta_K1(V, K_i));
}
double I_K1(double V, double K_i)
{
    return G_K1 * x_K1_inf(V, K_i) * (V - E_K(K_i));
}

// Na+/Ca++ exchanger current
double I_NaCa(double V, double Na_i, double Ca_i)   // !!!
{
    return (k_NaCa * ((exp((gamma_I_NaCa * V * F) / (R * T)) * (Na_i*Na_i*Na_i) * Ca_o) - (exp(((gamma_I_NaCa - 1.0) * V * F) / (R * T)) * (Na_o*Na_o*Na_o) * Ca_i * alpha))) / (((K_mNa_i*K_mNa_i*K_mNa_i) + (Na_o*Na_o*Na_o)) * (K_mCa + Ca_o) * (1.0 + (k_sat * exp(((gamma_I_NaCa) * V * F) / (R * T)))));
}

// Na+/K+ pump current
double I_NaK(double V, double Na_i) // !!!
{
    return ((((p_KNa * K_o) / (K_o + K_mK)) * Na_i) / (Na_i + K_mNa)) / (1.0 + (0.1245 * exp(((-0.1) * V * F) / (R * T))) + (0.0353 * exp(((-V) * F) / (R * T))));
}

// I_pCa
double I_pCa(double V, double Ca_i)
{
    return (G_pCa * Ca_i) / (K_pCa + Ca_i);
}

// I_pK
double I_pK(double V, double K_i)
{
    return (G_pK * (V - E_K(K_i))) / (1.0 + exp((25.0 - V) / 5.98));
}

// Background currents
double I_bNa(double V, double Na_i)
{
    return G_bNa * (V - E_Na(Na_i));
}
double I_bCa(double V, double Ca_i)
{
    return G_bCa * (V - E_Ca(Ca_i));
}

// Calcium dynamics
double I_leak(double Ca_SR, double Ca_i)
{
    return V_leak * (Ca_SR - Ca_i);
}
double I_up(double Ca_i)
{
    return V_maxup / (1.0 + ((K_up*K_up) / (Ca_i*Ca_i)));
}
double k_casr(double Ca_SR)
{
    return max_SR - ((max_SR - min_SR) / (1.0 + ((EC / Ca_SR)*(EC / Ca_SR))));
}
double k1(double Ca_SR)
{
    return k1_prime / k_casr(Ca_SR);
}
double O(double Ca_SR, double Ca_SS, double R_prime)
{
    return (k1(Ca_SR) * (Ca_SS*Ca_SS) * R_prime) / (k3 + (k1(Ca_SR) * (Ca_SS*Ca_SS)));
}
double I_rel(double Ca_SR, double Ca_SS, double R_prime)
{
    return V_rel * O(Ca_SR, Ca_SS, R_prime) * (Ca_SR - Ca_SS);
}
double I_xfer(double Ca_SS, double Ca_i)
{
    return V_xfer * (Ca_SS - Ca_i);
}
double k2(double Ca_SR)
{
    return k2_prime * k_casr(Ca_SR);
}
double Ca_ibufc(double Ca_i)    // !!!
{
    return 1.0 / (1.0 + ((Buf_C * K_bufc) / ((Ca_i + K_bufc)*(Ca_i + K_bufc))));
}
double Ca_srbufsr(double Ca_SR) // !!!
{
    return 1.0 / (1.0 + ((Buf_SR * K_bufsr) / ((Ca_SR + K_bufsr)*(Ca_SR + K_bufsr))));
}
double Ca_ssbufss(double Ca_SS) // !!!
{
    return 1.0 / (1.0 + ((Buf_SS * K_bufss) / ((Ca_SS + K_bufss)*(Ca_SS + K_bufss))));
}

/*----------------------------------------
Simulation parameters
-----------------------------------------*/
double simulation_time = 200;   // End time -> ms
double dx = 0.01;               // Spatial step -> cm
double dy = 0.01;               // Spatial step -> cm
int L = 2;                      // Length of the domain (square tissue) -> cm

/*----------------------------------------
Stimulation parameters
-----------------------------------------*/
double stim_strength = -38;         // Stimulation strength -> uA/cm^2 (???)
double t_s1_begin = 0.0;            // Stimulation start time -> ms
double stim_duration = 2.0;         // Stimulation duration -> ms
double stim2_duration = 2.0;        // Stimulation duration -> ms
double t_s2_begin = 300;            // Stimulation start time -> ms
double s1_x_limit = 0.04;           // Stimulation x limit -> cm
double s2_x_max = 1.0;              // Stimulation x max -> cm
double s2_y_max = 1.0;              // Stimulation y limit -> cm
double s2_x_min = 0.0;              // Stimulation x min -> cm
double s2_y_min = 0.0;              // Stimulation y min -> cm


/*----------------------------------------
Auxiliary functions
-----------------------------------------*/
// Initialize variables
void initialize_variables(int N, double **V, double **V_temp, double **X_r1, double **X_r2, double **X_s, double **m, double **h, double **j, double **d, double **f, double **f2, double **fCass, double **s, double **r, double **Ca_i, double **Ca_SR, double **Ca_SS, double **R_prime, double **Na_i, double **K_i)
{
    for (int i = 0; i < N; i++)
    {
        for (int k = 0; k < N; k++)
        {
            V[i][k] = V_init;
            V_temp[i][k] = V_init;
            X_r1[i][k] = X_r1_init;
            X_r2[i][k] = X_r2_init;
            X_s[i][k] = X_s_init;
            m[i][k] = m_init;
            h[i][k] = h_init;
            j[i][k] = j_init;
            d[i][k] = d_init;
            f[i][k] = f_init;
            f2[i][k] = f2_init;
            fCass[i][k] = fCass_init;
            s[i][k] = s_init;
            r[i][k] = r_init;
            Ca_i[i][k] = Ca_i_init;
            Ca_SR[i][k] = Ca_SR_init;
            Ca_SS[i][k] = Ca_SS_init;
            R_prime[i][k] = R_prime_init;
            Na_i[i][k] = Na_i_init;
            K_i[i][k] = K_i_init;
        }
    }
}

void thomas_algorithm(double *d, double *solution, unsigned long N, double alpha)
{
    // Auxiliary arrays
    double *c_ = (double *)malloc((N - 1) * sizeof(double));
    double *d_ = (double *)malloc((N) * sizeof(double));

    // Coefficients
    double a = -alpha;    // subdiagonal
    double b = 1 + alpha; // diagonal (1st and last row)
    double c = -alpha;    // superdiagonal

    // 1st: update auxiliary arrays
    c_[0] = c / b;
    d_[0] = d[1] / b;

    b = 1 + 2 * alpha;

    for (int i = 1; i <= N - 2; i++)
    {
        c_[i] = c / (b - a * c_[i - 1]);
        d_[i] = (d[i + 1] - a * d_[i - 1]) / (b - a * c_[i - 1]);
    }

    b = 1 + alpha;
    d_[N - 1] = (d[N] - a * d_[N - 2]) / (b - a * c_[N - 2]);

    // 2nd: update solution
    solution[N] = d_[N - 1];

    for (int i = N - 2; i >= 0; i--)
    {
        solution[i + 1] = d_[i] - c_[i] * solution[i + 2];
    }

    // Free memory
    free(c_);
    free(d_);
}

bool time_to_stimulate(double time)
{
    if (time >= t_s1_begin && time <= t_s1_begin + stim_duration)
    {
        return true;
    }
    else if (time >= t_s2_begin && time <= t_s2_begin + stim2_duration)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------
Main function
-----------------------------------------*/
int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <number of threads>  <dt_ode (ms)>  <dt_pde (ms)>  <method>\n", argv[0]);
        exit(1);
    }
    int num_threads = atoi(argv[1]);
    double dt_ode = atof(argv[2]);
    double dt_pde = atof(argv[3]);
    char *method = argv[4];
    

    if (num_threads <= 0)
    {
        fprintf(stderr, "Number of threads must greater than 0\n");
        exit(1);
    }
    if (strcmp(method, "FE") != 0 && strcmp(method, "ADI") != 0 && strcmp(method, "FE2") != 0 && strcmp(method, "FE3") != 0)
    {
        fprintf(stderr, "Method must be FE (forward Euler) or ADI\n");
        exit(1);
    }
    printf("The number of threads is %d\n", num_threads);
    printf("The time step for ODE is %.03f\n", dt_ode);
    printf("The time step for PDE is %.03f\n", dt_pde);
    printf("The method is %s\n", method);

    // Spatial discretization (square tissue)
    int N = L / dx;

    // Allocate memory for variables
    double **V = (double **)malloc(N * sizeof(double *));
    double **V_temp = (double **)malloc(N * sizeof(double *));
    double **X_r1 = (double **)malloc(N * sizeof(double *));
    double **X_r2 = (double **)malloc(N * sizeof(double *));
    double **X_s = (double **)malloc(N * sizeof(double *));
    double **m = (double **)malloc(N * sizeof(double *));
    double **h = (double **)malloc(N * sizeof(double *));
    double **j = (double **)malloc(N * sizeof(double *));
    double **d = (double **)malloc(N * sizeof(double *));
    double **f = (double **)malloc(N * sizeof(double *));
    double **f2 = (double **)malloc(N * sizeof(double *));
    double **fCass = (double **)malloc(N * sizeof(double *));
    double **s = (double **)malloc(N * sizeof(double *));
    double **r = (double **)malloc(N * sizeof(double *));
    double **Ca_i = (double **)malloc(N * sizeof(double *));
    double **Ca_SR = (double **)malloc(N * sizeof(double *));
    double **Ca_SS = (double **)malloc(N * sizeof(double *));
    double **R_prime = (double **)malloc(N * sizeof(double *));
    double **Na_i = (double **)malloc(N * sizeof(double *));
    double **K_i = (double **)malloc(N * sizeof(double *));
    double **right = (double **)malloc(N * sizeof(double *));
    double **solution = (double **)malloc(N * sizeof(double *));
    for (int i = 0; i < N; i++)
    {
        V[i] = (double *)malloc(N * sizeof(double));
        V_temp[i] = (double *)malloc(N * sizeof(double));
        X_r1[i] = (double *)malloc(N * sizeof(double));
        X_r2[i] = (double *)malloc(N * sizeof(double));
        X_s[i] = (double *)malloc(N * sizeof(double));
        m[i] = (double *)malloc(N * sizeof(double));
        h[i] = (double *)malloc(N * sizeof(double));
        j[i] = (double *)malloc(N * sizeof(double));
        d[i] = (double *)malloc(N * sizeof(double));
        f[i] = (double *)malloc(N * sizeof(double));
        f2[i] = (double *)malloc(N * sizeof(double));
        fCass[i] = (double *)malloc(N * sizeof(double));
        s[i] = (double *)malloc(N * sizeof(double));
        r[i] = (double *)malloc(N * sizeof(double));
        Ca_i[i] = (double *)malloc(N * sizeof(double));
        Ca_SR[i] = (double *)malloc(N * sizeof(double));
        Ca_SS[i] = (double *)malloc(N * sizeof(double));
        R_prime[i] = (double *)malloc(N * sizeof(double));
        Na_i[i] = (double *)malloc(N * sizeof(double));
        K_i[i] = (double *)malloc(N * sizeof(double));
        right[i] = (double *)malloc(N * sizeof(double));
        solution[i] = (double *)malloc(N * sizeof(double));
    }
    
    double Vik, X_r1ik, X_r2ik, X_sik, mik, hik, jik, dik, fik, f2ik, fCassik, sik, rik, Ca_iik, Ca_SRik, Ca_SSik, R_primeik, Na_iik, K_iik;

    double I_total = 0.0;
    double D_long = sigma_long / (1.0 * beta);
    double D_trans = sigma_trans / (1.0 * beta);
    double zeta_long = (D_long * dt_pde) / (dx * dx); // For implicit
    double zeta_trans = (D_trans * dt_pde) / (dy * dy); // For implicit

    // Initialize variables
    initialize_variables(N, V, V_temp, X_r1, X_r2, X_s, m, h, j, d, f, f2, fCass, s, r, Ca_i, Ca_SR, Ca_SS, R_prime, Na_i, K_i);
    int step = 0;
    double tstep = 0;
    int M = simulation_time / dt_pde;
    double *time = (double *)malloc(M * sizeof(double));
    for (int i = 0; i < M; i++)
    {
        time[i] = i * dt_pde;
    }
    int M_ode = dt_pde / dt_ode;
    int n_ode = 0;
    int x_lim = s1_x_limit / dx;
    int x_max = s2_x_max / dx;
    int x_min = s2_x_min / dx;
    int y_max = N;
    int y_min = N - s2_y_max / dy;
    double I_stim;
    int i, k; // i for y-axis and k for x-axis
    double dR_prime_dt, dCa_SR_dt, dCa_SS_dt, dCa_i_dt, dNa_i_dt, dK_i_dt;

    // Convert dt_ode and dt_pde to string
    char s_dt_ode[10];
    sprintf(s_dt_ode, "%.03f", dt_ode);
    char s_dt_pde[10];
    sprintf(s_dt_pde, "%.03f", dt_pde);

    // Open the file to write for complete gif
    char fname_complete[100] = "tnnp-";
    strcat(fname_complete, method);
    strcat(fname_complete, "-");
    strcat(fname_complete, s_dt_ode);
    strcat(fname_complete, "-");
    strcat(fname_complete, s_dt_pde);
    strcat(fname_complete, ".txt");
    FILE *fp_all = NULL;
    fp_all = fopen(fname_complete, "w");
    int count = 0;

    // Open the file to write for times
    char fname_times[100] = "sim-times-";
    strcat(fname_times, method);
    strcat(fname_times, "-");
    strcat(fname_times, s_dt_ode);
    strcat(fname_times, "-");
    strcat(fname_times, s_dt_pde);
    strcat(fname_times, ".txt");
    FILE *fp_times = NULL;
    fp_times = fopen(fname_times, "w");

    // For velocity
    bool tag = true;

    // Start timer
    double start, finish, elapsed = 0.0;
    double start_ode, finish_ode, elapsed_ode = 0.0;
    double start_pde, finish_pde, elapsed_pde = 0.0;

    start = omp_get_wtime();

    // Forward Euler
    // Parallel region before time loop
    if (strcmp(method, "FE") == 0)
    {
        // Forward Euler
        #pragma omp parallel num_threads(num_threads) default(none) \
        private(i, k, I_stim, dR_prime_dt, dCa_SR_dt, dCa_SS_dt, dCa_i_dt, dNa_i_dt, dK_i_dt, I_total, n_ode, \
        Vik, X_r1ik, X_r2ik, X_sik, mik, hik, jik, dik, fik, f2ik, fCassik, sik, rik, Ca_iik, Ca_SRik, Ca_SSik, R_primeik, Na_iik, K_iik) \
        shared(N, V, V_temp, X_r1, X_r2, X_s, m, h, j, d, f, f2, fCass, s, r, Ca_i, Ca_SR, Ca_SS, \
        R_prime, Na_i, K_i, k4, V_C, V_SS, V_SR, F, Cm, dt_ode, stim_strength, \
        stim_duration, stim2_duration, t_s1_begin, t_s2_begin, x_lim, x_min, x_max, y_max, y_min, \
        zeta_long, zeta_trans, start, time, M, tag, M_ode, fp_all, fp_times, count, \
        start_ode, finish_ode, elapsed_ode, start_pde, finish_pde, elapsed_pde, simulation_time, step, tstep)
        {
            while (step < M)
            {   
                // Do just once
                #pragma omp master
                {
                    // Check ODEs time
                    start_ode = omp_get_wtime();
                }

                tstep = time[step];
                
                // ODEs - Reaction
                #pragma omp for collapse(2) nowait
                for (i = 1; i < N - 1; i++)
                {   
                    for (k = 1; k < N - 1; k++)
                    {   
                        for (n_ode = 0; n_ode < M_ode; n_ode++)
                        {
                            // Stimulus 1
                            if (tstep >= t_s1_begin && tstep <= t_s1_begin + stim_duration && k <= x_lim)
                            {
                                I_stim = stim_strength;
                            }
                            // Stimulus 2
                            else if (tstep >= t_s2_begin && tstep <= t_s2_begin + stim2_duration && k >= x_min && k <= x_max && i >= y_min && i <= y_max)
                            {
                                I_stim = stim_strength;
                            }
                            else 
                            {
                                I_stim = 0.0;
                            }

                            // Get values at current time step and space
                            Vik = V[i][k];
                            X_r1ik = X_r1[i][k];
                            X_r2ik = X_r2[i][k];
                            X_sik = X_s[i][k];
                            mik = m[i][k];
                            hik = h[i][k];
                            jik = j[i][k];
                            dik = d[i][k];
                            fik = f[i][k];
                            f2ik = f2[i][k];
                            fCassik = fCass[i][k];
                            sik = s[i][k];
                            rik = r[i][k];
                            Ca_iik = Ca_i[i][k];
                            Ca_SRik = Ca_SR[i][k];
                            Ca_SSik = Ca_SS[i][k];
                            R_primeik = R_prime[i][k];
                            Na_iik = Na_i[i][k];
                            K_iik = K_i[i][k];

                            // Update total current
                            I_total = I_stim + I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik) + I_NaK(Vik, Na_iik) + I_NaCa(Vik, Na_iik, Ca_iik) + I_pCa(Vik, Ca_iik) + I_pK(Vik, K_iik) + I_bCa(Vik, Ca_iik);

                            // Update voltage
                            V[i][k] = Vik + (-I_total) * dt_ode;
                            V_temp[i][k] = V[i][k];

                            // Update concentrations
                            dR_prime_dt = ((-k2(Ca_SSik)) * Ca_SSik * R_primeik) + (k4 * (1.0 - R_primeik));
                            dCa_i_dt = Ca_ibufc(Ca_iik) * (((((I_leak(Ca_SRik, Ca_iik) - I_up(Ca_iik)) * V_SR) / V_C) + I_xfer(Ca_SSik, Ca_iik)) - ((((I_bCa(Vik, Ca_iik) + I_pCa(Vik, Ca_iik)) - (2.0 * I_NaCa(Vik, Na_iik, Ca_iik))) * Cm) / (2.0 * V_C * F)));
                            dCa_SR_dt = Ca_srbufsr(Ca_SRik) * (I_up(Ca_iik) - (I_rel(Ca_SRik, Ca_SSik, R_primeik) + I_leak(Ca_SRik, Ca_iik)));
                            dCa_SS_dt = Ca_ssbufss(Ca_SSik) * (((((-I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik)) * Cm) / (2.0 * V_SS * F)) + ((I_rel(Ca_SRik, Ca_SSik, R_primeik) * V_SR) / V_SS)) - ((I_xfer(Ca_SSik, Ca_iik) * V_C) / V_SS));
                            dNa_i_dt = ((-(I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + (3.0 * I_NaK(Vik, Na_iik)) + (3.0 * I_NaCa(Vik, Na_iik, Ca_iik)))) / (V_C * F)) * Cm;
                            dK_i_dt = ((-((I_stim + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_pK(Vik, K_iik)) - (2.0 * I_NaK(Vik, Na_iik)))) / (V_C * F)) * Cm;

                            R_prime[i][k] = R_primeik + dR_prime_dt * dt_ode;
                            Ca_SR[i][k] = Ca_SRik + dCa_SR_dt * dt_ode;
                            Ca_SS[i][k] = Ca_SSik + dCa_SS_dt * dt_ode;
                            Ca_i[i][k] = Ca_iik + dCa_i_dt * dt_ode;
                            Na_i[i][k] = Na_iik + dNa_i_dt * dt_ode;
                            K_i[i][k] = K_iik + dK_i_dt * dt_ode;

                            // Update gating variables - Rush Larsen
                            X_r1[i][k] = x_r1_inf(Vik) - (x_r1_inf(Vik) - X_r1ik) * exp(-dt_ode / tau_x_r1(Vik));
                            X_r2[i][k] = x_r2_inf(Vik) - (x_r2_inf(Vik) - X_r2ik) * exp(-dt_ode / tau_x_r2(Vik));
                            X_s[i][k] = x_s_inf(Vik) - (x_s_inf(Vik) - X_sik) * exp(-dt_ode / tau_x_s(Vik));
                            r[i][k] = r_inf(Vik) - (r_inf(Vik) - rik) * exp(-dt_ode / tau_r(Vik));
                            s[i][k] = s_inf(Vik) - (s_inf(Vik) - sik) * exp(-dt_ode / tau_s(Vik));
                            m[i][k] = m_inf(Vik) - (m_inf(Vik) - mik) * exp(-dt_ode / tau_m(Vik));
                            h[i][k] = h_inf(Vik) - (h_inf(Vik) - hik) * exp(-dt_ode / tau_h(Vik));
                            j[i][k] = j_inf(Vik) - (j_inf(Vik) - jik) * exp(-dt_ode / tau_j(Vik));
                            d[i][k] = d_inf(Vik) - (d_inf(Vik) - dik) * exp(-dt_ode / tau_d(Vik));
                            f[i][k] = f_inf(Vik) - (f_inf(Vik) - fik) * exp(-dt_ode / tau_f(Vik));
                            f2[i][k] = f2_inf(Vik) - (f2_inf(Vik) - f2ik) * exp(-dt_ode / tau_f2(Vik));
                            fCass[i][k] = fCass_inf(Vik) - (fCass_inf(Vik) - fCassik) * exp(-dt_ode / tau_fCass(Vik));
                        }
                    }
                }

                #pragma omp master
                {
                    // Check ODE time
                    finish_ode = omp_get_wtime();
                    elapsed_ode += finish_ode - start_ode;
                }                

                // Boundary Conditions for a square tissue
                #pragma omp for nowait
                for (i = 0; i < N; i++)
                {
                    V_temp[i][0] = V_temp[i][1];
                    V_temp[i][N - 1] = V_temp[i][N - 2];
                    V_temp[0][i] = V_temp[1][i];
                    V_temp[N - 1][i] = V_temp[N - 2][i];
                }

                #pragma omp master
                {
                    // Check PDE time
                    start_pde = omp_get_wtime();
                }

                // PDEs - Diffusion
                #pragma omp barrier
                #pragma omp for collapse(2) nowait
                for (i = 1; i < N - 1; i++)
                {
                    for (k = 1; k < N - 1; k++)
                    {   
                        V[i][k] = V_temp[i][k] + (zeta_trans * ((V_temp[i - 1][k] - 2.0 * V_temp[i][k] + V_temp[i + 1][k]))) + (zeta_long * ((V_temp[i][k - 1] - 2.0 * V_temp[i][k] + V_temp[i][k + 1])));
                    }
                }

                #pragma omp master
                {
                    // Check PDE time
                    finish_pde = omp_get_wtime();
                    elapsed_pde += finish_pde - start_pde;
                }
                
                // Boundary Conditions for a square tissue
                #pragma omp for nowait
                for (i = 0; i < N; i++)
                {
                    V[i][0] = V[i][1];
                    V[i][N - 1] = V[i][N - 2];
                    V[0][i] = V[1][i];
                    V[N - 1][i] = V[N - 2][i];
                }

                #pragma omp master
                {
                    // Write to file
                    if (step % 50 == 0)
                    {
                        for (int i = 0; i < N; i++)
                        {
                            for (int k = 0; k < N; k++)
                            {
                                fprintf(fp_all, "%lf\n", V[i][k]);
                            }
                        }
                        fprintf(fp_times, "%lf\n", time[step]);
                        count++;
                    }

                    // Check S1 velocity
                    if (V[100][N-1] > 20 && tag)
                    {
                        printf("S1 velocity: %lf\n", ((20 - x_lim) / (time[step])));
                        tag = false;
                    }
                }

                // Update step
                #pragma omp master
                {
                    step++;
                }
                #pragma omp barrier
                
            }
        }
    }

    // Old version
    else if (strcmp(method, "FE2") == 0)
    {
        // Forward Euler - old version
        {
            while (step < M)
            {   
                
                // Check ODEs time
                start_ode = omp_get_wtime();

                tstep = time[step];
                
                // ODEs - Reaction
                #pragma omp parallel for collapse(2) num_threads(num_threads) default(none) \
                private(i, k, I_stim, dR_prime_dt, dCa_SR_dt, dCa_SS_dt, dCa_i_dt, dNa_i_dt, dK_i_dt, I_total, \
                Vik, X_r1ik, X_r2ik, X_sik, mik, hik, jik, dik, fik, f2ik, fCassik, sik, rik, Ca_iik, Ca_SRik, Ca_SSik, R_primeik, Na_iik, K_iik) \
                shared(N, V, V_temp, X_r1, X_r2, X_s, m, h, j, d, f, f2, fCass, s, r, Ca_i, Ca_SR, Ca_SS, \
                R_prime, Na_i, K_i, k4, V_C, V_SS, V_SR, F, Cm, dt_ode, stim_strength, \
                stim_duration, stim2_duration, t_s1_begin, t_s2_begin, x_lim, x_min, x_max, y_max, y_min, \
                zeta_long, zeta_trans, time, step, tstep)
                for (i = 1; i < N - 1; i++)
                {   
                    for (k = 1; k < N - 1; k++)
                    {   
                        // Stimulus 1
                        if (tstep >= t_s1_begin && tstep <= t_s1_begin + stim_duration && k <= x_lim)
                        {
                            I_stim = stim_strength;
                        }
                        // Stimulus 2
                        else if (tstep >= t_s2_begin && tstep <= t_s2_begin + stim2_duration && k >= x_min && k <= x_max && i >= y_min && i <= y_max)
                        {
                            I_stim = stim_strength;
                        }
                        else 
                        {
                            I_stim = 0.0;
                        }

                        // Get values at current time step and space
                        Vik = V[i][k];
                        X_r1ik = X_r1[i][k];
                        X_r2ik = X_r2[i][k];
                        X_sik = X_s[i][k];
                        mik = m[i][k];
                        hik = h[i][k];
                        jik = j[i][k];
                        dik = d[i][k];
                        fik = f[i][k];
                        f2ik = f2[i][k];
                        fCassik = fCass[i][k];
                        sik = s[i][k];
                        rik = r[i][k];
                        Ca_iik = Ca_i[i][k];
                        Ca_SRik = Ca_SR[i][k];
                        Ca_SSik = Ca_SS[i][k];
                        R_primeik = R_prime[i][k];
                        Na_iik = Na_i[i][k];
                        K_iik = K_i[i][k];

                        // Update total current
                        I_total = I_stim + I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik) + I_NaK(Vik, Na_iik) + I_NaCa(Vik, Na_iik, Ca_iik) + I_pCa(Vik, Ca_iik) + I_pK(Vik, K_iik) + I_bCa(Vik, Ca_iik);

                        // Update voltage
                        V_temp[i][k] = Vik + (-I_total) * dt_ode;

                        // Update concentrations
                        dR_prime_dt = ((-k2(Ca_SSik)) * Ca_SSik * R_primeik) + (k4 * (1.0 - R_primeik));
                        dCa_i_dt = Ca_ibufc(Ca_iik) * (((((I_leak(Ca_SRik, Ca_iik) - I_up(Ca_iik)) * V_SR) / V_C) + I_xfer(Ca_SSik, Ca_iik)) - ((((I_bCa(Vik, Ca_iik) + I_pCa(Vik, Ca_iik)) - (2.0 * I_NaCa(Vik, Na_iik, Ca_iik))) * Cm) / (2.0 * V_C * F)));
                        dCa_SR_dt = Ca_srbufsr(Ca_SRik) * (I_up(Ca_iik) - (I_rel(Ca_SRik, Ca_SSik, R_primeik) + I_leak(Ca_SRik, Ca_iik)));
                        dCa_SS_dt = Ca_ssbufss(Ca_SSik) * (((((-I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik)) * Cm) / (2.0 * V_SS * F)) + ((I_rel(Ca_SRik, Ca_SSik, R_primeik) * V_SR) / V_SS)) - ((I_xfer(Ca_SSik, Ca_iik) * V_C) / V_SS));
                        dNa_i_dt = ((-(I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + (3.0 * I_NaK(Vik, Na_iik)) + (3.0 * I_NaCa(Vik, Na_iik, Ca_iik)))) / (V_C * F)) * Cm;
                        dK_i_dt = ((-((I_stim + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_pK(Vik, K_iik)) - (2.0 * I_NaK(Vik, Na_iik)))) / (V_C * F)) * Cm;

                        R_prime[i][k] = R_primeik + dR_prime_dt * dt_ode;
                        Ca_SR[i][k] = Ca_SRik + dCa_SR_dt * dt_ode;
                        Ca_SS[i][k] = Ca_SSik + dCa_SS_dt * dt_ode;
                        Ca_i[i][k] = Ca_iik + dCa_i_dt * dt_ode;
                        Na_i[i][k] = Na_iik + dNa_i_dt * dt_ode;
                        K_i[i][k] = K_iik + dK_i_dt * dt_ode;

                        // Update gating variables - Rush Larsen
                        X_r1[i][k] = x_r1_inf(Vik) - (x_r1_inf(Vik) - X_r1ik) * exp(-dt_ode / tau_x_r1(Vik));
                        X_r2[i][k] = x_r2_inf(Vik) - (x_r2_inf(Vik) - X_r2ik) * exp(-dt_ode / tau_x_r2(Vik));
                        X_s[i][k] = x_s_inf(Vik) - (x_s_inf(Vik) - X_sik) * exp(-dt_ode / tau_x_s(Vik));
                        r[i][k] = r_inf(Vik) - (r_inf(Vik) - rik) * exp(-dt_ode / tau_r(Vik));
                        s[i][k] = s_inf(Vik) - (s_inf(Vik) - sik) * exp(-dt_ode / tau_s(Vik));
                        m[i][k] = m_inf(Vik) - (m_inf(Vik) - mik) * exp(-dt_ode / tau_m(Vik));
                        h[i][k] = h_inf(Vik) - (h_inf(Vik) - hik) * exp(-dt_ode / tau_h(Vik));
                        j[i][k] = j_inf(Vik) - (j_inf(Vik) - jik) * exp(-dt_ode / tau_j(Vik));
                        d[i][k] = d_inf(Vik) - (d_inf(Vik) - dik) * exp(-dt_ode / tau_d(Vik));
                        f[i][k] = f_inf(Vik) - (f_inf(Vik) - fik) * exp(-dt_ode / tau_f(Vik));
                        f2[i][k] = f2_inf(Vik) - (f2_inf(Vik) - f2ik) * exp(-dt_ode / tau_f2(Vik));
                        fCass[i][k] = fCass_inf(Vik) - (fCass_inf(Vik) - fCassik) * exp(-dt_ode / tau_fCass(Vik));
                    }
                }

                // Check ODE time
                finish_ode = omp_get_wtime();
                elapsed_ode += finish_ode - start_ode;

                // Boundary Conditions for a square tissue
                #pragma omp parallel for num_threads(num_threads) default(none) \
                private(i) \
                shared(V_temp, N)
                for (i = 0; i < N; i++)
                {
                    V_temp[i][0] = V_temp[i][1];
                    V_temp[i][N - 1] = V_temp[i][N - 2];
                    V_temp[0][i] = V_temp[1][i];
                    V_temp[N - 1][i] = V_temp[N - 2][i];
                }

                // Check PDE time
                start_pde = omp_get_wtime();
                
                // PDEs - Diffusion
                #pragma omp parallel for collapse(2) num_threads(num_threads) default(none) \
                private(i, k) \
                shared(N, V, V_temp, zeta_long, zeta_trans)
                for (i = 1; i < N - 1; i++)
                {
                    for (k = 1; k < N - 1; k++)
                    {
                        V[i][k] = V_temp[i][k] + (zeta_trans * ((V_temp[i - 1][k] - 2.0 * V_temp[i][k] + V_temp[i + 1][k]))) + (zeta_long * ((V_temp[i][k - 1] - 2.0 * V_temp[i][k] + V_temp[i][k + 1])));
                    }
                }

                step++;
                // Check PDE time
                finish_pde = omp_get_wtime();
                elapsed_pde += finish_pde - start_pde;
                
                
                // Boundary Conditions for a square tissue
                #pragma omp parallel for num_threads(num_threads) default(none) \
                private(i) \
                shared(N, V)
                for (i = 0; i < N; i++)
                {
                    V[i][0] = V[i][1];
                    V[i][N - 1] = V[i][N - 2];
                    V[0][i] = V[1][i];
                    V[N - 1][i] = V[N - 2][i];
                }
            }
        }
    }

    // One parallel region inside time loop
    else if (strcmp(method, "FE3") == 0)
    {
        // Forward Euler
        
        while (step < M)
        {   
            #pragma omp parallel num_threads(num_threads) default(none) \
            private(i, k, I_stim, dR_prime_dt, dCa_SR_dt, dCa_SS_dt, dCa_i_dt, dNa_i_dt, dK_i_dt, I_total, \
            Vik, X_r1ik, X_r2ik, X_sik, mik, hik, jik, dik, fik, f2ik, fCassik, sik, rik, Ca_iik, Ca_SRik, Ca_SSik, R_primeik, Na_iik, K_iik) \
            shared(N, V, V_temp, X_r1, X_r2, X_s, m, h, j, d, f, f2, fCass, s, r, Ca_i, Ca_SR, Ca_SS, \
            R_prime, Na_i, K_i, k4, V_C, V_SS, V_SR, F, Cm, dt_ode, stim_strength, \
            stim_duration, stim2_duration, t_s1_begin, t_s2_begin, x_lim, x_min, x_max, y_max, y_min, \
            zeta_long, zeta_trans, start, time, M, \
            start_ode, finish_ode, elapsed_ode, start_pde, finish_pde, elapsed_pde, simulation_time, step, tstep)
            {
                // Do just once
                #pragma omp master
                {
                    // Check ODEs time
                    start_ode = omp_get_wtime();
                }

                tstep = time[step];
                
                // ODEs - Reaction
                #pragma omp for collapse(2) nowait
                for (i = 1; i < N - 1; i++)
                {   
                    for (k = 1; k < N - 1; k++)
                    {   
                        // Stimulus 1
                        if (tstep >= t_s1_begin && tstep <= t_s1_begin + stim_duration && k <= x_lim)
                        {
                            I_stim = stim_strength;
                        }
                        // Stimulus 2
                        else if (tstep >= t_s2_begin && tstep <= t_s2_begin + stim2_duration && k >= x_min && k <= x_max && i >= y_min && i <= y_max)
                        {
                            I_stim = stim_strength;
                        }
                        else 
                        {
                            I_stim = 0.0;
                        }

                        // Get values at current time step and space
                        Vik = V[i][k];
                        X_r1ik = X_r1[i][k];
                        X_r2ik = X_r2[i][k];
                        X_sik = X_s[i][k];
                        mik = m[i][k];
                        hik = h[i][k];
                        jik = j[i][k];
                        dik = d[i][k];
                        fik = f[i][k];
                        f2ik = f2[i][k];
                        fCassik = fCass[i][k];
                        sik = s[i][k];
                        rik = r[i][k];
                        Ca_iik = Ca_i[i][k];
                        Ca_SRik = Ca_SR[i][k];
                        Ca_SSik = Ca_SS[i][k];
                        R_primeik = R_prime[i][k];
                        Na_iik = Na_i[i][k];
                        K_iik = K_i[i][k];

                        // Update total current
                        I_total = I_stim + I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik) + I_NaK(Vik, Na_iik) + I_NaCa(Vik, Na_iik, Ca_iik) + I_pCa(Vik, Ca_iik) + I_pK(Vik, K_iik) + I_bCa(Vik, Ca_iik);

                        // Update voltage
                        V_temp[i][k] = Vik + (-I_total) * dt_ode;

                        // Update concentrations
                        dR_prime_dt = ((-k2(Ca_SSik)) * Ca_SSik * R_primeik) + (k4 * (1.0 - R_primeik));
                        dCa_i_dt = Ca_ibufc(Ca_iik) * (((((I_leak(Ca_SRik, Ca_iik) - I_up(Ca_iik)) * V_SR) / V_C) + I_xfer(Ca_SSik, Ca_iik)) - ((((I_bCa(Vik, Ca_iik) + I_pCa(Vik, Ca_iik)) - (2.0 * I_NaCa(Vik, Na_iik, Ca_iik))) * Cm) / (2.0 * V_C * F)));
                        dCa_SR_dt = Ca_srbufsr(Ca_SRik) * (I_up(Ca_iik) - (I_rel(Ca_SRik, Ca_SSik, R_primeik) + I_leak(Ca_SRik, Ca_iik)));
                        dCa_SS_dt = Ca_ssbufss(Ca_SSik) * (((((-I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik)) * Cm) / (2.0 * V_SS * F)) + ((I_rel(Ca_SRik, Ca_SSik, R_primeik) * V_SR) / V_SS)) - ((I_xfer(Ca_SSik, Ca_iik) * V_C) / V_SS));
                        dNa_i_dt = ((-(I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + (3.0 * I_NaK(Vik, Na_iik)) + (3.0 * I_NaCa(Vik, Na_iik, Ca_iik)))) / (V_C * F)) * Cm;
                        dK_i_dt = ((-((I_stim + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_pK(Vik, K_iik)) - (2.0 * I_NaK(Vik, Na_iik)))) / (V_C * F)) * Cm;

                        R_prime[i][k] = R_primeik + dR_prime_dt * dt_ode;
                        Ca_SR[i][k] = Ca_SRik + dCa_SR_dt * dt_ode;
                        Ca_SS[i][k] = Ca_SSik + dCa_SS_dt * dt_ode;
                        Ca_i[i][k] = Ca_iik + dCa_i_dt * dt_ode;
                        Na_i[i][k] = Na_iik + dNa_i_dt * dt_ode;
                        K_i[i][k] = K_iik + dK_i_dt * dt_ode;

                        // Update gating variables - Rush Larsen
                        X_r1[i][k] = x_r1_inf(Vik) - (x_r1_inf(Vik) - X_r1ik) * exp(-dt_ode / tau_x_r1(Vik));
                        X_r2[i][k] = x_r2_inf(Vik) - (x_r2_inf(Vik) - X_r2ik) * exp(-dt_ode / tau_x_r2(Vik));
                        X_s[i][k] = x_s_inf(Vik) - (x_s_inf(Vik) - X_sik) * exp(-dt_ode / tau_x_s(Vik));
                        r[i][k] = r_inf(Vik) - (r_inf(Vik) - rik) * exp(-dt_ode / tau_r(Vik));
                        s[i][k] = s_inf(Vik) - (s_inf(Vik) - sik) * exp(-dt_ode / tau_s(Vik));
                        m[i][k] = m_inf(Vik) - (m_inf(Vik) - mik) * exp(-dt_ode / tau_m(Vik));
                        h[i][k] = h_inf(Vik) - (h_inf(Vik) - hik) * exp(-dt_ode / tau_h(Vik));
                        j[i][k] = j_inf(Vik) - (j_inf(Vik) - jik) * exp(-dt_ode / tau_j(Vik));
                        d[i][k] = d_inf(Vik) - (d_inf(Vik) - dik) * exp(-dt_ode / tau_d(Vik));
                        f[i][k] = f_inf(Vik) - (f_inf(Vik) - fik) * exp(-dt_ode / tau_f(Vik));
                        f2[i][k] = f2_inf(Vik) - (f2_inf(Vik) - f2ik) * exp(-dt_ode / tau_f2(Vik));
                        fCass[i][k] = fCass_inf(Vik) - (fCass_inf(Vik) - fCassik) * exp(-dt_ode / tau_fCass(Vik));
                    }
                }

                #pragma omp master
                {
                    // Check ODE time
                    finish_ode = omp_get_wtime();
                    elapsed_ode += finish_ode - start_ode;
                }
                

                // Boundary Conditions for a square tissue
                #pragma omp for nowait
                for (i = 0; i < N; i++)
                {
                    V_temp[i][0] = V_temp[i][1];
                    V_temp[i][N - 1] = V_temp[i][N - 2];
                    V_temp[0][i] = V_temp[1][i];
                    V_temp[N - 1][i] = V_temp[N - 2][i];
                }

                #pragma omp master
                {
                    // Check PDE time
                    start_pde = omp_get_wtime();
                }

                // PDEs - Diffusion
                #pragma omp barrier
                #pragma omp for collapse(2)
                for (i = 1; i < N - 1; i++)
                {
                    for (k = 1; k < N - 1; k++)
                    {
                        V[i][k] = V_temp[i][k] + (zeta_trans * ((V_temp[i - 1][k] - 2.0 * V_temp[i][k] + V_temp[i + 1][k]))) + (zeta_long * ((V_temp[i][k - 1] - 2.0 * V_temp[i][k] + V_temp[i][k + 1])));
                    }
                }

                #pragma omp master
                {
                    // Check PDE time
                    finish_pde = omp_get_wtime();
                    elapsed_pde += finish_pde - start_pde;
                }
                
                // Boundary Conditions for a square tissue
                #pragma omp for nowait
                for (i = 0; i < N; i++)
                {
                    V[i][0] = V[i][1];
                    V[i][N - 1] = V[i][N - 2];
                    V[0][i] = V[1][i];
                    V[N - 1][i] = V[N - 2][i];
                }

                /* #pragma omp master
                {
                    // Write to file
                    if (step % 50 == 0)
                    {
                        for (int i = 0; i < N; i++)
                        {
                            for (int k = 0; k < N; k++)
                            {
                                fprintf(fp_all, "%lf\n", V[i][k]);
                            }
                        }
                        fprintf(fp_times, "%lf\n", time[step]);
                        count++;
                    }

                    // Check S1 velocity
                    if (V[100][N-1] > 20 && tag)
                    {
                        printf("S1 velocity: %lf\n", ((20 - x_lim) / (time[step])));
                        tag = false;
                    }
                } */

                // Update step
                #pragma omp master
                {
                    step++;
                }
                #pragma omp barrier
            }
        }
        
    }


    // ADI
    else if (strcmp(method, "ADI") == 0)
    {
        // ADI
        #pragma omp parallel num_threads(num_threads) default(none) \
        private(i, k, I_stim, dR_prime_dt, dCa_SR_dt, dCa_SS_dt, dCa_i_dt, dNa_i_dt, dK_i_dt, I_total, n_ode, \
        Vik, X_r1ik, X_r2ik, X_sik, mik, hik, jik, dik, fik, f2ik, fCassik, sik, rik, Ca_iik, Ca_SRik, Ca_SSik, R_primeik, Na_iik, K_iik) \
        shared(N, V, V_temp, X_r1, X_r2, X_s, m, h, j, d, f, f2, fCass, s, r, Ca_i, Ca_SR, Ca_SS, \
        R_prime, Na_i, K_i, k4, V_C, V_SS, V_SR, F, Cm, dt_ode, stim_strength, \
        stim_duration, stim2_duration, t_s1_begin, t_s2_begin, x_lim, x_min, x_max, y_max, y_min, \
        zeta_long, zeta_trans, start, time, M, tag, M_ode, solution, right, fp_all, fp_times, count, \
        start_ode, finish_ode, elapsed_ode, start_pde, finish_pde, elapsed_pde, simulation_time, step, tstep)
        {
            while (step < M)
            {
        
                // Do just once
                #pragma omp master
                {
                    // Check ODE time
                    start_ode = omp_get_wtime();
                }
                
                tstep = time[step];

                // ODEs - Reaction
                
                for (n_ode = 0; n_ode < M_ode; n_ode++)
                {
                    #pragma omp for collapse(2) nowait
                    for (i = 1; i < N - 1; i++)
                    {
                        for (k = 1; k < N - 1; k++)
                        {
                            // Stimulus 1
                            if (tstep >= t_s1_begin && tstep <= t_s1_begin + stim_duration && k <= x_lim)
                            {
                                I_stim = stim_strength;
                            }
                            // Stimulus 2
                            else if (tstep >= t_s2_begin && tstep <= t_s2_begin + stim2_duration && k >= x_min && k <= x_max && i >= y_min && i <= y_max)
                            {
                                I_stim = stim_strength;
                            }
                            else 
                            {
                                I_stim = 0.0;
                            }

                            // Get values at current time step and space
                            Vik = V[i][k];
                            X_r1ik = X_r1[i][k];
                            X_r2ik = X_r2[i][k];
                            X_sik = X_s[i][k];
                            mik = m[i][k];
                            hik = h[i][k];
                            jik = j[i][k];
                            dik = d[i][k];
                            fik = f[i][k];
                            f2ik = f2[i][k];
                            fCassik = fCass[i][k];
                            sik = s[i][k];
                            rik = r[i][k];
                            Ca_iik = Ca_i[i][k];
                            Ca_SRik = Ca_SR[i][k];
                            Ca_SSik = Ca_SS[i][k];
                            R_primeik = R_prime[i][k];
                            Na_iik = Na_i[i][k];
                            K_iik = K_i[i][k];

                            // Update total current
                            I_total = I_stim + I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik) + I_NaK(Vik, Na_iik) + I_NaCa(Vik, Na_iik, Ca_iik) + I_pCa(Vik, Ca_iik) + I_pK(Vik, K_iik) + I_bCa(Vik, Ca_iik);

                            // Update voltage
                            V[i][k] = Vik + (-I_total) * dt_ode;
                            right[k][i] = V[i][k];

                            // Update concentrations
                            dR_prime_dt = ((-k2(Ca_SSik)) * Ca_SSik * R_primeik) + (k4 * (1.0 - R_primeik));
                            dCa_i_dt = Ca_ibufc(Ca_iik) * (((((I_leak(Ca_SRik, Ca_iik) - I_up(Ca_iik)) * V_SR) / V_C) + I_xfer(Ca_SSik, Ca_iik)) - ((((I_bCa(Vik, Ca_iik) + I_pCa(Vik, Ca_iik)) - (2.0 * I_NaCa(Vik, Na_iik, Ca_iik))) * Cm) / (2.0 * V_C * F)));
                            dCa_SR_dt = Ca_srbufsr(Ca_SRik) * (I_up(Ca_iik) - (I_rel(Ca_SRik, Ca_SSik, R_primeik) + I_leak(Ca_SRik, Ca_iik)));
                            dCa_SS_dt = Ca_ssbufss(Ca_SSik) * (((((-I_CaL(Vik, dik, fik, f2ik, fCassik, Ca_SSik)) * Cm) / (2.0 * V_SS * F)) + ((I_rel(Ca_SRik, Ca_SSik, R_primeik) * V_SR) / V_SS)) - ((I_xfer(Ca_SSik, Ca_iik) * V_C) / V_SS));
                            dNa_i_dt = ((-(I_Na(Vik, mik, hik, jik, Na_iik) + I_bNa(Vik, Na_iik) + (3.0 * I_NaK(Vik, Na_iik)) + (3.0 * I_NaCa(Vik, Na_iik, Ca_iik)))) / (V_C * F)) * Cm;
                            dK_i_dt = ((-((I_stim + I_K1(Vik, K_iik) + I_to(Vik, rik, sik, K_iik) + I_Kr(Vik, X_r1ik, X_r2ik, K_iik) + I_Ks(Vik, X_sik, K_iik, Na_iik) + I_pK(Vik, K_iik)) - (2.0 * I_NaK(Vik, Na_iik)))) / (V_C * F)) * Cm;

                            R_prime[i][k] = R_primeik + dR_prime_dt * dt_ode;
                            Ca_SR[i][k] = Ca_SRik + dCa_SR_dt * dt_ode;
                            Ca_SS[i][k] = Ca_SSik + dCa_SS_dt * dt_ode;
                            Ca_i[i][k] = Ca_iik + dCa_i_dt * dt_ode;
                            Na_i[i][k] = Na_iik + dNa_i_dt * dt_ode;
                            K_i[i][k] = K_iik + dK_i_dt * dt_ode;

                            // Update gating variables - Rush Larsen
                            X_r1[i][k] = x_r1_inf(Vik) - (x_r1_inf(Vik) - X_r1ik) * exp(-dt_ode / tau_x_r1(Vik));
                            X_r2[i][k] = x_r2_inf(Vik) - (x_r2_inf(Vik) - X_r2ik) * exp(-dt_ode / tau_x_r2(Vik));
                            X_s[i][k] = x_s_inf(Vik) - (x_s_inf(Vik) - X_sik) * exp(-dt_ode / tau_x_s(Vik));
                            r[i][k] = r_inf(Vik) - (r_inf(Vik) - rik) * exp(-dt_ode / tau_r(Vik));
                            s[i][k] = s_inf(Vik) - (s_inf(Vik) - sik) * exp(-dt_ode / tau_s(Vik));
                            m[i][k] = m_inf(Vik) - (m_inf(Vik) - mik) * exp(-dt_ode / tau_m(Vik));
                            h[i][k] = h_inf(Vik) - (h_inf(Vik) - hik) * exp(-dt_ode / tau_h(Vik));
                            j[i][k] = j_inf(Vik) - (j_inf(Vik) - jik) * exp(-dt_ode / tau_j(Vik));
                            d[i][k] = d_inf(Vik) - (d_inf(Vik) - dik) * exp(-dt_ode / tau_d(Vik));
                            f[i][k] = f_inf(Vik) - (f_inf(Vik) - fik) * exp(-dt_ode / tau_f(Vik));
                            f2[i][k] = f2_inf(Vik) - (f2_inf(Vik) - f2ik) * exp(-dt_ode / tau_f2(Vik));
                            fCass[i][k] = fCass_inf(Vik) - (fCass_inf(Vik) - fCassik) * exp(-dt_ode / tau_fCass(Vik));
                        }
                    }
                }

                #pragma omp master
                {
                    // Check ODE time
                    finish_ode = omp_get_wtime();
                    elapsed_ode += finish_ode - start_ode;

                    // Check PDE time
                    start_pde = omp_get_wtime();
                }

                // PDEs - Diffusion
                // 1st: Diffusion y-axis (lines)
                #pragma omp barrier
                #pragma omp for nowait
                for (i = 1; i < N - 1; i++)
                {
                    thomas_algorithm(right[i], solution[i], N - 2, zeta_trans);

                    // Pass solution
                    for (k = 1; k < N - 1; k++)
                    {
                        // Linear system - tridiagonal matrix
                        V_temp[k][i] = solution[i][k];
                    }
                }

                // 2nd: Diffusion x-axis (colums)
                #pragma omp barrier
                #pragma omp for nowait
                for (i = 1; i < N - 1; i++)
                {
                    // Linear system - tridiagonal matrix
                    thomas_algorithm(V_temp[i], V[i], N - 2, zeta_long);
                }

                #pragma omp master
                {
                    // Check PDE time
                    finish_pde = omp_get_wtime();
                    elapsed_pde += finish_pde - start_pde;
                }

                // Boundary Conditions for a square tissue
                #pragma omp for nowait
                for (i = 0; i < N; i++)
                {
                    V[i][0] = V[i][1];
                    V[i][N - 1] = V[i][N - 2];
                    V[0][i] = V[1][i];
                    V[N - 1][i] = V[N - 2][i];
                }

                #pragma omp master
                    {
                        // Write to file
                        if (step % 100 == 0)
                        {
                            for (int i = 0; i < N; i++)
                            {
                                for (int k = 0; k < N; k++)
                                {
                                    fprintf(fp_all, "%lf\n", V[i][k]);
                                }
                            }
                            fprintf(fp_times, "%lf\n", time[step]);
                            count++;
                        }

                        // Check S1 velocity
                        if (V[100][N-1] > 20 && tag)
                        {
                            printf("S1 velocity: %lf\n", ((20 - x_lim) / (time[step])));
                            tag = false;
                        }
                    }

                    // Update step
                    #pragma omp master
                    {
                        step++;
                    }
                    #pragma omp barrier 
            }
        }
    }

    // Check time
    finish = omp_get_wtime();
    elapsed = finish - start;

    FILE* fp = fopen("time-results.txt", "a");
    fprintf(fp, "%s\t|\tsim_time: %.1f \t|\tdt_ode = %.2f\t|\tdt_pde = %.2f\t|\t%d threads\t|\tODE: %.4f\t|\tPDE: %.4f \t->\ttotal time: %.4f\n", method, simulation_time, dt_ode, dt_pde, num_threads, elapsed_ode, elapsed_pde, elapsed);
    // fprintf(fp, "ODE: %.4f and PDE: %.4f\n\n", elapsed_ode, elapsed_pde);
    // printf("\nElapsed time = %e seconds\n%d time steps recorded\n", elapsed, count);
    printf("\nElapsed time = %e seconds\n", elapsed); printf("Total steps: %d\n", step);
    
    // Open the file to write for last frame
    char fname_lastframe[100] = "last-";
    strcat(fname_lastframe, method);
    strcat(fname_lastframe, "-");
    strcat(fname_lastframe, s_dt_ode);
    strcat(fname_lastframe, "-");
    strcat(fname_lastframe, s_dt_pde);
    strcat(fname_lastframe, ".txt");
    FILE *fp_last = NULL;
    fp_last = fopen(fname_lastframe, "w");

    // Write last time step to file
    for (int i = 0; i < N; i++)
    {
        for (int k = 0; k < N; k++)
        {
            fprintf(fp_last, "%lf\n", V[i][k]);
        }
    }

    // Close files
    fclose(fp_all);
    fclose(fp_times);
    fclose(fp_last);
    fclose(fp);

    free(V);
    free(V_temp);
    free(Ca_i);
    free(Ca_SR);
    free(Ca_SS);
    free(Na_i);
    free(K_i);
    free(X_r1);
    free(X_r2);
    free(X_s);
    free(r);
    free(s);
    free(m);
    free(h);
    free(j);
    free(d);
    free(f);
    free(f2);
    free(fCass);
    free(R_prime);
    free(right);
    free(solution);

    return 0;
}
