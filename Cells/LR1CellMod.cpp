//
//  LR1CellMod.cpp
//
//  Implementation of the LR1 Model, with UCLA Ito
//  Created by Julian Landaw on 12/25/16.
//  Copyright © 2016 Julian Landaw. All rights reserved.
//
#include <math.h>
#include <fstream>
#include "LR1CellMod.h"

#ifndef LR1CellMod_cpp
#define LR1CellMod_cpp

#define nai 18.0
#define nao 140.0
#define ki 145.0
#define ko 5.4
#define cao 1.8

#define gna 23.0
//#define ibarca 0.09*1.5

#ifndef ibarcafac
#define ibarcafac 1.0
#endif

#define ibarca 0.09*ibarcafac

#ifndef dshift
#define dshift 0.0
#endif

#ifndef fshift
#define fshift 0.0
#endif

#ifndef zshift
#define zshift 0.0
#endif

#ifndef yshift
#define yshift 0.0
#endif

#ifndef ikfac
#define ikfac 1.0
#endif

#ifndef ikifac
#define ikifac 1.0
#endif

#define gkr 0.282*ikfac
#define prnak 0.01833
#define gk1 0.6047*ikifac
#define gpk 0.0183
#define ibbar 0.03921

#define R 8314.0
#define frdy 96485.0
#define temp 310.0
#define zna 1.0
#define zk 1.0
#define zca 2.0
#define frt 0.037435883507803
#define rtf 26.712338705498265

#ifndef DV_MAX
#define DV_MAX 0.1
#endif

#ifndef ADAPTIVE
#define ADAPTIVE 10
#endif

#ifndef stimulus
#define stimulus -80.0
#endif

#ifndef stimduration
#define stimduration 0.5
#endif

template <int ncells>
 LR1CellMod<ncells>::LR1CellMod()
{
    for (int i = 0; i < ncells; i++) {
        v[i] = -88.654973;
        cai[i] = 0.0002;
        m[i] = 0.001;
        h[i] = 0.94;
        j[i] = 0.94;
        xr[i] = 0.01;
        d[i] = 0.01;
        f[i] = 0.95;
        z[i] = 0.0;
        y[i] = 1.0;
        diffcurrent[i] = 0.0;
        itofac[i] = 1.0;
        tauXfac[i] = 1.0;
    }
}

template <int ncells>
  bool LR1CellMod<ncells>::iterate(const int id, double dt, double st, double dv_max) {
    double dm, dh, dj, dd, df, dz, dy, dxr, dcai;
    double ina, ical, ito, ikr, ik1, ipk, ib, dv;
    
    ina = comp_ina(id, dt, dm, dh, dj);
    ical = comp_ical(id, dt, dd, df);
    ito = comp_ito(id, dt, dz, dy);
    ikr = comp_ikr(id, dt, dxr);
    ik1 = comp_ik1(id);
    ipk = comp_ipk(id);
    ib = comp_ib(id);
    dv = (diffcurrent[id] - (ikr + ik1 + ito + ina + ical + ib + ipk + st))*dt;
    
    //Comment this out to remove adaptive time-stepping
    if (dv_max > 0 && dv > dv_max) {return false;}
      
    comp_calcdyn(id, ical, dcai);
    
    v[id] += dv;
    m[id] += dm*dt;
    h[id] += dh*dt;
    j[id] += dj*dt;
    d[id] += dd*dt;
    f[id] += df*dt;
    z[id] += dz*dt;
    y[id] += dy*dt;
    xr[id] += dxr*dt;
    cai[id] += dcai*dt;
    
    return true;
}

template <int ncells>
void LR1CellMod<ncells>::stepdt (const int id, double dt, double st) {
    if (id > -1 && id < ncells) {
        bool success = iterate(id, dt, st, DV_MAX);
        if (!success) {
            for (int i = 0; i < ADAPTIVE; i++) {
                iterate(id, dt/ADAPTIVE, st, -1);
            }
        }
    }
}

template <int ncells>
  double LR1CellMod<ncells>::comp_ina (int id, double dt, double& dm, double& dh, double& dj) //Fast Sodium Current
{
    double ena, mtau, htau, jtau, mss, hss, jss, ina;
    double am, bm, ah, bh, aj, bj;
    
    ena = (rtf/zna)*log(nao/nai);
    
    am = 0.32*(v[id]+47.13)/(1-exp(-0.1*(v[id]+47.13)));
    bm = 0.08*exp(-v[id]/11.0);
    
    if (v[id] < -40.0) {
        ah = 0.135*exp((80+v[id])/-6.8);
        bh = 3.56*exp(0.079*v[id])+310000*exp(0.35*v[id]);
        aj = (-127140*exp(0.2444*v[id])-0.00003474*exp(-0.04391*v[id]))*((v[id]+37.78)/(1.0+exp(0.311*(v[id]+79.23))));
        bj = (0.1212*exp(-0.01052*v[id]))/(1.0+exp(-0.1378*(v[id]+40.14)));
    }
    else {
        ah = 0.0;
        bh = 1.0/(0.13*(1.0+exp((v[id]+10.66)/-11.1)));
        aj = 0.0;
        bj = (0.3*exp(-0.0000002535*v[id]))/(1.0+exp(-0.1*(v[id]+32.0)));
    }
    
    mtau = 1.0/(am+bm);
    htau = 1.0/(ah+bh);
    jtau = 1.0/(aj+bj);
    
    mss = am*mtau;
    hss = ah*htau;
    jss = aj*jtau;
    
    ina = gna*m[id]*m[id]*m[id]*h[id]*j[id]*(v[id]-ena);
    
    dm = (mss-(mss-m[id])*exp(-dt/mtau) - m[id])/dt;
    dh = (hss-(hss-h[id])*exp(-dt/htau) - h[id])/dt;
    dj = (jss-(jss-j[id])*exp(-dt/jtau) - j[id])/dt;
    
    return ina;
}

#define pca 5.4e-4
#define gcai 1.0
#define gcao 0.341
#define kmca 0.6

template <int ncells>
double LR1CellMod<ncells>::comp_ical(int id, double dt, double& dd, double& df) {
    double dinf, taud, tauf, dss, fss, fca, ical, vfrt;
    fca = 1.0/(1.0 + (cai[id]/kmca)*(cai[id]/kmca));
    vfrt = zca*v[id]*frt;
    
    if (fabs(vfrt) < 1e-3) {
        ical = pca*d[id]*f[id]*fca*zca*frdy*(gcai*cai[id]*exp(vfrt) - gcao*cao)/(1.0 + vfrt/2.0 + vfrt*vfrt/6.0 + vfrt*vfrt*vfrt/24.0);
    }
    else {
        ical = pca*d[id]*f[id]*fca*zca*frdy*vfrt*(gcai*cai[id]*exp(vfrt) - gcao*cao)/(exp(vfrt) - 1.0);
    }
    
    dss = 1.0/(1.0 + exp(-(v[id] + 7.0)/6.0));
    taud = 0.29 + 1.1*exp(0.052*(v[id] + 13))/(1.0 + exp(0.132*(v[id]+13)));
    fss = 0.99/(1.0 + exp((v[id] + 28.9)/4.9)) + 0.23/(1.0 + exp(-(v[id] - 40.0)/32.0));
    tauf = 22.0 + 280.0*exp(0.062*(v[id] + 28.3))/(1.0 + exp(0.25*(v[id] + 28.3)));
    //dss = 1.0/(1.0 + exp(-(v[id]+10.0)/6.24));
    //taud = dss*(1.0 - exp(-(v[id] + 10.0)/6.24))/(0.035*(v[id] + 10.0));
    //fss = 1.0/(1.0 + exp((v[id] + 35.06)/8.6)) + 0.6/(1.0 + exp((50.0-v[id])/20.0));
    //tauf = 1.0/(0.0197*exp(-(0.0337*(v[id] + 10.0)*(v[id] + 10.0))) + 0.02);
    
    dd = (dss - (dss - d[id])*exp(-dt/taud) - d[id])/dt;
    df = (fss - (fss - f[id])*exp(-dt/tauf) - f[id])/dt;
    
    return ical;
}

/*
template <int ncells>
  double LR1CellMod<ncells>::comp_ical (int id, double dt, double& dd, double& df) // L-type Calcium Current
{
    double ad, bd, af, bf, taud, tauf, dss, fss, ical;
    
    ad = 0.095*exp(-0.01*(v[id] + dshift-5.0))/(1+exp(-0.072*(v[id] + dshift-5.0)));
    bd = 0.07*exp(-0.017*(v[id] + dshift+44.0))/(1+exp(0.05*(v[id] + dshift+44.0)));
    af = 0.012*exp(-0.008*(v[id]-fshift+28.0))/(1+exp(0.15*(v[id]-fshift+28.0)));
    bf = 0.0065*exp(-0.02*(v[id]-fshift+30.0))/(1+exp(-0.2*(v[id]-fshift+30.0)));
    taud = 1.0/(ad+bd);
    tauf = 1.0/(af+bf);
    dss = ad*taud;
    fss = af*tauf;
    
    ical = ibarca*d[id]*f[id]*(v[id] - (7.7 - 13.0287*log(cai[id])));
    
    dd = (dss - (dss - d[id])*exp(-dt/taud) - d[id])/dt;
    df = (fss - (fss - f[id])*exp(-dt/tauf) - f[id])/dt;
    
    return ical;
}
*/
               
#define gito 1.1
               //

template <int ncells>
double LR1CellMod<ncells>::comp_ito (int id, double dt, double& dz, double& dy)
{
    double ito, az, bz, ay, by, tauz, tauy, zss, yss, ek;
    ek = (rtf/zk)*log(ko/ki);
    az = 10.0*exp((v[id] - 40.0)/25.0)/(1.0 + exp((v[id] - 40.0)/25.0));
    bz = 10.0*exp(-(v[id] + 90.0)/25.0)/(1.0 + exp(-(v[id] + 90.0)/25.0));
    ay = 0.015/(1.0 + exp((v[id] + 60)/5.0));
    by = 0.1*exp((v[id] + 25.0)/5.0)/(1.0 + exp((v[id] + 25.0)/5.0));
    
    tauz = 1.0/(az + bz);
    tauy = 1.0/(ay + by);
    zss = az*tauz;
    yss = ay*tauy;
    
    ito = itofac[id]*gito*z[id]*z[id]*z[id]*y[id]*exp(v[id]/100.0)*(v[id] - ek);
    
    dz = (zss-(zss-z[id])*exp(-dt/tauz) - z[id])/dt; // Really this is Xto,f
    dy = (yss-(yss-y[id])*exp(-dt/tauy) - y[id])/dt;
    
    return ito;
    
    /*
    double ek, zssdv, tauzdv, yssdv, tauydv, ito;
    ek = (rtf/zk)*log(ko/ki);
    
    zssdv = 1.0/(1.0 + exp(-(v[id] + zshift+3.0)/15));
    tauzdv = 3.5*exp(-((v[id] + zshift)/30.0)*((v[id] + zshift)/30.0))+1.5;
    
    yssdv = 1.0/(1.0+exp((v[id] - yshift+33.5)/10.0));
    tauydv = 20.0/(1.0+exp((v[id] - yshift+33.5)/10.0)) + 20.0;
    
    ito = itofac[id]*gitodv*zdv[id]*ydv[id]*(v[id]-ek);
    
    dzdv = (zssdv-(zssdv-zdv[id])*exp(-dt/tauzdv) - zdv[id])/dt; // Really this is Xto,f
    dydv = (yssdv-(yssdv-ydv[id])*exp(-dt/tauydv) - ydv[id])/dt;
    
    return ito;
     */
}

template <int ncells>
double LR1CellMod<ncells>::comp_ikr (int id, double dt, double& dxr)
{
    double ekr, r, ax, bx, tauxr, xrss,  ikr;
    
    ekr = (rtf/zk)*log((ko + prnak*nao)/(ki + prnak*nai));
    
    r = (v[id] > -100.0) ? 2.837*(exp(0.04*(v[id] + 77)) - 1.0)/((v[id] + 77.0)*exp(0.04*(v[id] + 35.0))) : 1.0;
    
    ax = 0.0005*exp(0.083*(v[id] + 50))/(1 + exp(0.057*(v[id] + 50)));
    bx = 0.0013*exp(-0.06*(v[id]+20))/(1+exp(-0.04*(v[id]+20)));
    tauxr = 1.0/(ax+bx);
    xrss = ax*tauxr; //This is the proper order, do this before tauxr is multiplied by the tauX factor
    tauxr = tauXfac[id]*tauxr;
    
    ikr = gkr*sqrt(ko/5.4)*xr[id]*r*(v[id] - ekr);
    
    dxr = (xrss - (xrss - xr[id])*exp(-dt/tauxr) - xr[id])/dt;
    
    return ikr;
}

template <int ncells>
double LR1CellMod<ncells>::comp_ik1 (int id)
{
    double ek, ak1, bk1, xk1ss, ik1;
    
    ek = (rtf/zk)*log(ko/ki);
    
    ak1 = 1.02/(1+exp(0.2385*(v[id]-ek-59.215)));
    bk1 = (0.49124*exp(0.08032*(v[id]-ek+5.476))+exp(0.06175*(v[id]-ek-594.31)))/(1.0+exp(-0.5143*(v[id]-ek+4.753)));
    xk1ss = ak1/(ak1 + bk1);
    ik1 = gk1*sqrt(ko/5.4)*xk1ss*(v[id] - ek);
    
    return ik1;
}

template <int ncells>
double LR1CellMod<ncells>::comp_ipk (int id)
{
    double ek, ipk, kp;
    
    ek = (rtf/zk)*log(ko/ki);
    kp = 1.0/(1.0+exp((7.488-v[id])/5.98));
    ipk = gpk*kp*(v[id]-ek);
    return ipk;
}

template <int ncells>
double LR1CellMod<ncells>::comp_ib (int id)
{
    return ibbar*(v[id] + 59.87);
}

template <int ncells>
void LR1CellMod<ncells>::comp_calcdyn (int id, double ical, double& dcai) // MAKE SURE THIS IS COMPUTED AFTER ALL OTHER CURRENTS
{
    dcai = -0.0001*ical + 0.07*(0.0001 - cai[id]);
}

template <int ncells>
void LR1CellMod<ncells>::setcell (int id, LR1CellMod<1>* newcell)
{
    v[id] = newcell->v[0];
    cai[id] = newcell->cai[0];
    m[id] = newcell->m[0];
    h[id] = newcell->h[0];
    j[id] = newcell->j[0];
    xr[id] = newcell->xr[0];
    d[id] = newcell->d[0];
    f[id] = newcell->f[0];
    z[id] = newcell->z[0];
    y[id] = newcell->y[0];
    diffcurrent[id] = newcell->diffcurrent[0];
    itofac[id] = newcell->itofac[0];
    tauXfac[id] = newcell->tauXfac[0];
}

template <int ncells>
void LR1CellMod<ncells>::getcell (int id, LR1CellMod<1>* newcell)
{
    newcell->v[0] = v[id];
    newcell->cai[0] = cai[id];
    newcell->m[0] = m[id];
    newcell->h[0] = h[id];
    newcell->j[0] = j[id];
    newcell->xr[0] = xr[id];
    newcell->d[0] = d[id];
    newcell->f[0] = f[id];
    newcell->z[0] = z[id];
    newcell->y[0] = y[id];
    newcell->diffcurrent[0] = diffcurrent[id];
    newcell->itofac[0] = itofac[id];
    newcell->tauXfac[0] = tauXfac[id];
}

template <int ncells>
void LR1CellMod<ncells>::saveconditions(FILE* file, int id, bool header, double t) {
    if (header) {
        fprintf(file,"t\tv\tm\th\tj\td\tf\tz\ty\txr\tcai\n");
    }
    fprintf(file,"%g\t",t);
    fprintf(file,"%.12f\t",v[id]);
    fprintf(file,"%.12f\t",m[id]);
    fprintf(file,"%.12f\t",h[id]);
    fprintf(file,"%.12f\t",j[id]);
    fprintf(file,"%.12f\t",d[id]);
    fprintf(file,"%.12f\t",f[id]);
    fprintf(file,"%.12f\t",z[id]);
    fprintf(file,"%.12f\t",y[id]);
    fprintf(file,"%.12f\t",xr[id]);
    fprintf(file,"%.12f\n",cai[id]);
}
#endif // LR1CellMod_cpp

