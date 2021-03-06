//
//  LR1CellIto.cpp
//
//  Implementation of the LR1 Model, with UCLA Ito
//  Created by Julian Landaw on 12/25/16.
//  Copyright © 2016 Julian Landaw. All rights reserved.
//
#include <math.h>
#include <fstream>
#include "OHaraCell.h"

#ifndef OHaraCell_cpp
#define OHaraCell_cpp

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
OHaraCell<ncells>::OHaraCell()
{
    for (int i = 0; i < ncells; i++) {
        v[i] = -87.5;
        nai[i] = 7;
        nass[i] = nai[i];
        ki[i] = 145;
        kss[i] = ki[i];
        cai[i] = 1.0e-4;
        cass[i] = cai[i];
        cansr[i] = 1.2;
        cajsr[i] = cansr[i];
        m[i] = 0;
        hf[i] = 1;
        hs[i] = 1;
        j[i] = 1;
        hsp[i] = 1;
        jp[i] = 1;
        mL[i] = 0;
        hL[i] = 1;
        hLp[i] = 1;
        a[i] = 0;
        iF[i] = 1;
        iS[i] = 1;
        ap[i] = 0;
        iFp[i] = 1;
        iSp[i] = 1;
        d[i] = 0;
        ff[i] = 1;
        fs[i] = 1;
        fcaf[i] = 1;
        fcas[i] = 1;
        jca[i] = 1;
        nca[i] = 0;
        ffp[i] = 1;
        fcafp[i] = 1;
        xrf[i] = 0;
        xrs[i] = 0;
        xs1[i] = 0;
        xs2[i] = 0;
        xk1[i] = 1;
        Jrelnp[i] = 0;
        Jrelp[i] = 0;
        CaMKt[i] = 0;
        
        celltype[i] = 1; //EPI
        
        diffcurrent[i] = 0.0;
        
        itofac[i] = 1.0;
        inafac[i] = 1.0;
        ikrfac[i] = 1.0;
        iksfac[i] = 1.0;
	ik1fac[i] = 1.0;
        icalfac[i] = 1.0;
        nacafac[i] = 1.0;
        yshift[i] = 0.0;
        
        vssfac[i] = 1.0;
        tauffac[i] = 1.0;
        taufshift[i] = 0.0;
        // SK variables, default is there is no SK current
        
        iskfac[i] = 0.0;
        skh[i] = 0.0006;
        skn[i] = 4;
        alphask[i] = 1.0;
        xsk[i] = 0.0;
        
        #ifdef clampnai
        naiclamp[i] = true;
        #else
        naiclamp[i] = false;
        #endif
        #ifdef clampki
        kiclamp[i] = true;
        #else
        kiclamp[i] = false;
        #endif
    }
}

template <int ncells>
bool OHaraCell<ncells>::iterate(const int id, double dt, double st, double dv_max) {
    double dv, INa, INaL, dm, dhf, dhs, dj, dhsp, djp, dmL, dhL, dhLp;
    double Isk, dxsk, Ito, da, diF, diS, dap, diFp, diSp;
    double ICaL, ICaNa, ICaK, dd, dff, dfs, dfcaf, dfcas, djca, dffp, dfcafp, dnca;
    double IKr, dxrf, dxrs;
    double IKs, dxs1, dxs2;
    double IK1, dxk1;
    double INaCa_i, INaCa_ss, INaK, IKb, INab, ICab, IpCa;
    
    comp_ina(id, dt, INa, INaL, dm, dhf, dhs, dj, dhsp, djp, dmL, dhL, dhLp);
    Isk = comp_isk(id, dt, dxsk);
    Ito = comp_ito(id, dt, da, diF, diS, dap, diFp, diSp);
    comp_ical(id, dt, ICaL, ICaNa, ICaK, dd, dff, dfs, dfcaf, dfcas, djca, dffp, dfcafp, dnca);
    IKr = comp_ikr(id, dt, dxrf, dxrs);
    IKs = comp_iks(id, dt, dxs1, dxs2);
    IK1 = comp_ik1(id, dt, dxk1);
    comp_inaca(id, INaCa_i, INaCa_ss);
    INaK = comp_inak(id);
    IKb = comp_ikb(id);
    INab = comp_inab(id);
    ICab = comp_icab(id);
    IpCa = comp_ipca(id);
    
    dv = (diffcurrent[id] - (INa + INaL + Ito + Isk + ICaL + ICaNa + ICaK + IKr + IKs + IK1 + INaCa_i + INaCa_ss + INaK + INab + ICab + IKb + IpCa + st))*dt;
    
    //Comment this out to remove adaptive time-stepping
    if (dv_max > 0 && dv*dv > dv_max*dv_max) {return false;}
    
    double CaMKb, CaMKa, JdiffNa, JdiffK, Jdiff, bt, a_rel, Jrel_inf;
    double tau_rel, btp, a_relp, Jrel_infp, tau_relp;
    double fJrelp, Jrel, Jupnp, Jupp, fJupp, Jleak, Jup, Jtr;
    double Bcai, Bcass, dcansr, Bcajsr, dcajsr;
    
    CaMKb = CaMKo*(1.0-CaMKt[id])/(1.0+KmCaM/cass[id]);
    CaMKa = CaMKb+CaMKt[id];
    CaMKt[id] += dt*(aCaMK*CaMKb*(CaMKb+CaMKt[id]) - bCaMK*CaMKt[id]);

    JdiffNa = (nass[id] - nai[id])/2.0;
    JdiffK = (kss[id] - ki[id])/2.0;
    Jdiff = (cass[id] - cai[id])/0.2;

    bt = 4.75;
    a_rel = 0.5*bt;
    Jrel_inf = a_rel*(-ICaL)/(1.0+pow(1.5/cajsr[id],8.0));
    if (celltype[id] == 2)
    {
        Jrel_inf*=1.7;
    }
    tau_rel = bt/(1.0+0.0123/cajsr[id]);
    if (tau_rel<0.005)
    {
        tau_rel=0.005;
    }
    
    btp = 1.25*bt;
    a_relp = 0.5*btp;
    Jrel_infp = a_relp*(-ICaL)/(1.0+pow(1.5/cajsr[id],8.0));
    if (celltype[id] == 2)
    {
        Jrel_infp*=1.7;
    }
    tau_relp = btp/(1.0+0.0123/cajsr[id]);
    if (tau_relp < 0.005)
    {
        tau_relp=0.005;
    }
    fJrelp=(1.0/(1.0+KmCaMK/CaMKa));
    Jrel = (1.0 - fJrelp)*Jrelnp[id] + fJrelp*Jrelp[id];
    
    Jrelnp[id] = Jrel_inf - (Jrel_inf - Jrelnp[id])*exp(-dt/tau_rel);
    Jrelp[id] = Jrel_infp - (Jrel_infp - Jrelp[id])*exp(-dt/tau_relp);

    Jupnp = 0.004375*cai[id]/(cai[id] + 0.00092);
    Jupp = 2.75*0.004375*cai[id]/(cai[id]+0.00092-0.00017);
    if (celltype[id] == 1)
    {
        Jupnp*=1.3;
        Jupp*=1.3;
    }
    fJupp = (1.0/(1.0+KmCaMK/CaMKa));
    Jleak = 0.0039375*cansr[id]/15.0;
    Jup = (1.0 - fJupp)*Jupnp + fJupp*Jupp - Jleak;

    Jtr = (cansr[id] - cajsr[id])/100.0;

    if (naiclamp[id] == false) {
        nai[id] += dt*(-(INa+INaL+3.0*INaCa_i+3.0*INaK+INab)*Acap/(F*vmyo) + JdiffNa*(vssfac[id]*vss)/vmyo);
        nass[id] += dt*(-(ICaNa+3.0*INaCa_ss)*Acap/(F*(vssfac[id]*vss)) - JdiffNa);
    }

    if (kiclamp[id] == false) {
        ki[id] += dt*(-(Ito+Isk+IKr+IKs+IK1+IKb+st-2.0*INaK)*Acap/(F*vmyo)+JdiffK*(vssfac[id]*vss)/vmyo);
        kss[id] += dt*(-(ICaK)*Acap/(F*(vssfac[id]*vss)) - JdiffK);
    }

    if (celltype[id] == 1)
    {
        Bcai = 1.0/(1.0+1.3*cmdnmax*kmcmdn/pow(kmcmdn+cai[id],2.0)+trpnmax*kmtrpn/pow(kmtrpn+cai[id],2.0));
    }
    else
    {
        Bcai = 1.0/(1.0+cmdnmax*kmcmdn/pow(kmcmdn+cai[id],2.0)+trpnmax*kmtrpn/pow(kmtrpn+cai[id],2.0));
    }
    cai[id] += dt*(Bcai*(-(IpCa+ICab-2.0*INaCa_i)*Acap/(2.0*F*vmyo) - Jup*vnsr/vmyo+Jdiff*(vssfac[id]*vss)/vmyo));

    Bcass = 1.0/(1.0+BSRmax*KmBSR/pow(KmBSR+cass[id],2.0) + BSLmax*KmBSL/pow(KmBSL + cass[id],2.0));
    cass[id] += dt*(Bcass*(-(ICaL-2.0*INaCa_ss)*Acap/(2.0*F*(vssfac[id]*vss))+Jrel*vjsr/(vssfac[id]*vss)-Jdiff));

    cansr[id] += dt*(Jup-Jtr*vjsr/vnsr);

    Bcajsr = 1.0/(1.0+csqnmax*kmcsqn/pow(kmcsqn+cajsr[id],2.0));
    cajsr[id] += dt*(Bcajsr*(Jtr - Jrel));
    
    v[id] += dv;
    m[id] = dm;
    hf[id] = dhf;
    hs[id] = dhs;
    j[id] = dj;
    hsp[id] = dhsp;
    jp[id] = djp;
    mL[id] = dmL;
    hL[id] = dhL;
    hLp[id] = dhLp;
    a[id] = da;
    iF[id] = diF;
    iS[id] = diS;
    ap[id] = dap;
    iFp[id] = diFp;
    iSp[id] = diSp;
    d[id] = dd;
    ff[id] = dff;
    fs[id] = dfs;
    fcaf[id] = dfcaf;
    fcas[id] = dfcas;
    jca[id] = djca;
    ffp[id] = dffp;
    fcafp[id] = dfcafp;
    nca[id] = dnca;
    xrf[id] = dxrf;
    xrs[id] = dxrs;
    xs1[id] = dxs1;
    xs2[id] = dxs2;
    xk1[id] = dxk1;
    xsk[id] = dxsk;
    
    return true;
}

template <int ncells>
void OHaraCell<ncells>::stepdt (const int id, double dt, double st) {
    if (id > -1 && id < ncells) {
        bool success = iterate(id, dt, st, DV_MAX);
        if (!success) {
            for (int i = 0; i < ADAPTIVE; i++) {
                iterate(id, dt/ADAPTIVE, st, -1);
            }
        }
    }
}

#ifndef mshift
#define mshift 0
#endif
#ifndef hshift
#define hshift 0
#endif
//#define jshift 10

template <int ncells>
void OHaraCell<ncells>::comp_ina (int id, double dt, double& INa, double& INaL, double& dm, double& dhf, double& dhs, double& dj, double& dhsp, double& djp, double& dmL, double& dhL, double& dhLp) //Fast and Slow Sodium Current
{
    double ENa, CaMKa, CaMKb, mss, tm, hss, thf, ths, Ahf, Ahs, h, jss, tj, hssp, thsp;
    double hp, tjp, GNa, fINap, mLss, tmL, hLss, thL, hLssp, thLp;
    double GNaL, fINaLp;
    
    ENa = (1.0/frt)*log(nao/nai[id]);
    
    CaMKb = CaMKo*(1.0 - CaMKt[id])/(1.0+KmCaM/cass[id]);
    CaMKa = CaMKb + CaMKt[id];
    
    mss = 1.0/(1.0+exp((-((v[id] + mshift)+39.57))/9.871));
    tm =1.0/(6.765*exp(((v[id] + mshift)+11.64)/34.77)+8.552*exp(-((v[id] + mshift)+77.42)/5.955));
    dm = mss - (mss - m[id])*exp(-dt/tm); //dm
    
    hss = 1.0/(1.0+exp(((v[id] + hshift)+82.90)/6.086));
    thf = 1.0/(1.432e-5*exp(-((v[id] + hshift)+1.196)/6.285)+6.149*exp(((v[id] + hshift)+0.5096)/20.27));
    ths = 1.0/(0.009794*exp(-((v[id] + hshift)+17.95)/28.05)+0.3343*exp(((v[id] + hshift)+5.730)/56.66));
    Ahf = 0.99;
    Ahs = 1.0-Ahf;
    
    dhf = hss - (hss - hf[id])*exp(-dt/thf); //dhf
    dhs = hss - (hss - hs[id])*exp(-dt/ths); //dhs
    
    h = Ahf*hf[id] + Ahs*hs[id];
    jss = hss;
    tj = 2.038+1.0/(0.02136*exp(-((v[id] + hshift)+100.6)/8.281)+0.3052*exp(((v[id] + hshift)+0.9941)/38.45));
    dj = jss - (jss - j[id])*exp(-dt/tj); //dj
    
    hssp = 1.0/(1.0+exp(((v[id] + hshift)+89.1)/6.086));
    thsp = 3.0*ths;
    dhsp = hssp - (hssp - hsp[id])*exp(-dt/thsp); //dhsp
    
    hp = Ahf*hf[id] + Ahs*hsp[id];
    tjp = 1.46*tj;
    djp = jss - (jss - jp[id])*exp(-dt/tjp); //djp
    
    GNa = 75.0;
    fINap = (1.0/(1.0+KmCaMK/CaMKa));
    INa = inafac[id]*GNa*(v[id] - ENa)*m[id]*m[id]*m[id]*((1.0 - fINap)*h*j[id] + fINap*hp*jp[id]);

    mLss = 1.0/(1.0+exp((-((v[id] + mshift)+42.85))/5.264));
    tmL = tm;
    dmL = mLss - (mLss - mL[id])*exp(-dt/tmL); //dmL
    
    hLss = 1.0/(1.0+exp(((v[id] + hshift)+87.61)/7.488));
    thL = 200.0;
    dhL = hLss - (hLss - hL[id])*exp(-dt/thL); //dhL
    
    hLssp = 1.0/(1.0+exp(((v[id] + hshift)+93.81)/7.488));
    thLp = 3.0*thL;
    dhLp = hLssp - (hLssp - hLp[id])*exp(-dt/thLp); //dhLp
    
    GNaL=0.0075;
    if (celltype[id]==1)
    {
        GNaL*=0.6;
    }
    fINaLp = (1.0/(1.0+KmCaMK/CaMKa));
    INaL = GNaL*(v[id] - ENa)*mL[id]*((1.0-fINaLp)*hL[id] + fINaLp*hLp[id]);
}

#ifdef noslowito
#define gtof 0.11
#endif

template <int ncells>
double OHaraCell<ncells>::comp_ito (int id, double dt, double& da, double& diF, double& diS, double& dap, double& diFp, double& diSp)
{
    #ifdef noslowito
    #ifdef UCLAito 
    double ek, rt1, rt2, rt3, xtos_inf, ytos_inf, rs_inf, txs, tys;
    double xtof_inf, ytof_inf, rt4, rt5, txf, tyf, xitof;
    
    diF = 0; diS = 0; diFp = 0; diSp = 0; // this variables become unnecessary,
                                          // we use only the variables a and ap (replacing xtof and ytof)
    ek = (1.0/frt)*log(ko/ki[id]);
    rt1 = -(v[id] + 3.0)/15.0;
    rt2 = (v[id] + 33.5 - yshift[id])/10.0;
    rt3 = (v[id] + 60.0)/10.0;
    xtos_inf = 1.0/(1.0 + exp(rt1));
    ytos_inf = 1.0/(1.0 + exp(rt2));
    rs_inf = 1.0/(1.0 + exp(rt2));
    txs = 9.0/(1.0 + exp(-rt1)) + 0.5;
    tys = 3000.0/(1.0 + exp(rt3)) + 30.0;
    //xitos = gtos*xtos[id]*(ytos[id] + 0.5*rs_inf)*(v[id] - ek);
    //dxtos = xtos_inf - (xtos_inf - xtos[id])*exp(-dt/txs);
    //dytos = ytos_inf - (ytos_inf - ytos[id])*exp(-dt/tys);
    
    xtof_inf = xtos_inf;
    ytof_inf = ytos_inf;
    rt4 = -(v[id]/30.0)*(v[id]/30.0);
    rt5 = (v[id] + 33.5 - yshift[id])/10.0;
    txf = 3.5*exp(rt4) + 1.5;
    tyf = 20.0/(1.0 + exp(rt5)) + 20.0;
    //xitof = itofac[id]*gtof*xtof[id]*ytof[id]*(v[id] - ek);
    xitof = itofac[id]*gtof*a[id]*ap[id]*(v[id] - ek);
    da = xtof_inf - (xtof_inf - a[id])*exp(-dt/txf);
    dap = ytof_inf - (ytof_inf - ap[id])*exp(-dt/tyf);
    
    return xitof;
    
    #else
    // Domaine formulation
    double ekdv, rvdv, azdv, bzdv, tauzdv, zssdv, aydv, bydv, tauydv, yssdv, xitof;
    
    diF = 0; diS = 0; diFp = 0; diSp = 0; // this variables become unnecessary,
                                          // we use only the variables a and ap (replacing xtof and ytof)
    ekdv = (1.0/frt)*log(ko/ki[id]); 
    rvdv = exp(v[id]/100.0); 
    azdv = (10.0*exp((v[id]-40.0)/25.0))/(1.0+exp((v[id]-40.0)/25.0)); 
    bzdv = (10.0*exp(-(v[id]+90.0)/25.0))/(1.0+exp(-(v[id]+90.0)/25.0)); 
    tauzdv = 1.0/(azdv+bzdv); 
    zssdv = azdv/(azdv+bzdv); 
    da = zssdv-(zssdv-a[id])*exp(-dt/tauzdv); 

    aydv = 0.015/(1.0+exp((v[id]+60.0)/5.0)); 
    bydv = (0.1*exp((v[id]+25.0)/5.0))/(1.0+exp((v[id]+25.0)/5.0)); 
    tauydv = 1.0/(aydv+bydv); 
    yssdv = aydv/(aydv+bydv); 
    dap = yssdv-(yssdv-ap[id])*exp(-dt/tauydv); 
    xitof = itofac[id]*gtof*a[id]*a[id]*a[id]*ap[id]*rvdv*(v[id]-ekdv);
    return xitof;
    #endif
    
    #else
    double EK, CaMKb, CaMKa, ass, ta, iss, delta_epi, tiF, tiS, AiF, AiS, i, assp, dti_develop, dti_recover;
    double tiFp, tiSp, ip, Gto, fItop, Ito;
    
    EK = (1.0/frt)*log(ko/ki[id]);
    CaMKb = CaMKo*(1.0 - CaMKt[id])/(1.0+KmCaM/cass[id]);
    CaMKa = CaMKb + CaMKt[id];
    
    ass = 1.0/(1.0+exp((-(v[id]-14.34))/14.82));
    ta = 1.0515/(1.0/(1.2089*(1.0+exp(-(v[id]-18.4099)/29.3814)))+3.5/(1.0+exp((v[id]+100.0)/29.3814)));
    da = ass - (ass - a[id])*exp(-dt/ta); //da
    iss = 1.0/(1.0+exp((v[id]+43.94)/5.711));
    
    if (celltype[id] == 1)
    {
        delta_epi=1.0-(0.95/(1.0+exp((v[id]+70.0)/5.0)));
    }
    else
    {
        delta_epi=1.0;
    }
    tiF = 4.562+1.0/(0.3933*exp((-(v[id]+100.0))/100.0)+0.08004*exp((v[id]+50.0)/16.59));
    tiS = 23.62+1.0/(0.001416*exp((-(v[id]+96.52))/59.05)+1.780e-8*exp((v[id]+114.1)/8.079));
    tiF*=delta_epi;
    tiS*=delta_epi;
    AiF = 1.0/(1.0+exp((v[id]-213.6)/151.2));
    AiS = 1.0 - AiF;
    diF = iss - (iss - iF[id])*exp(-dt/tiF); //diF
    diS = iss - (iss - iS[id])*exp(-dt/tiS); //diS
    
    i = AiF*iF[id]*itofac[id] + AiS*iS[id];
    assp = 1.0/(1.0+exp((-(v[id]-24.34))/14.82));
    dap = assp - (assp - ap[id])*exp(-dt/ta); //dap
    
    dti_develop = 1.354+1.0e-4/(exp((v[id]-167.4)/15.89)+exp(-(v[id]-12.23)/0.2154));
    dti_recover = 1.0-0.5/(1.0+exp((v[id]+70.0)/20.0));
    tiFp = dti_develop*dti_recover*tiF;
    tiSp = dti_develop*dti_recover*tiS;
    diFp = iss - (iss-iFp[id])*exp(-dt/tiFp); //diFp
    diSp = iss - (iss-iSp[id])*exp(-dt/tiSp); //diSp
    
    ip = AiF*iFp[id] + AiS*iSp[id];
    Gto = 0.02;
    
    if (celltype[id] == 1)
    {
        Gto*=4.0;
    }
    if (celltype[id] == 2)
    {
        Gto*=4.0;
    }
   
    fItop=(1.0/(1.0+KmCaMK/CaMKa));
    Ito = Gto*(v[id] - EK)*((1.0 - fItop)*a[id]*i + fItop*ap[id]*ip);
    
    return Ito;
    #endif
}

template <int ncells>
void OHaraCell<ncells>::comp_ical(int id, double dt, double& ICaL, double& ICaNa, double& ICaK, double& dd, double& dff, double& dfs, double& dfcaf, double& dfcas, double& djca, double& dffp, double& dfcafp, double& dnca) 
{
    double CaMKb, CaMKa, vffrt, vfrt, dss, td, fss, tff, tfs, Aff, Afs, f, fcass, tfcaf, tfcas, Afcaf, Afcas; 
    double fca, tjca, fjca, tffp, fp, tfcafp, fcap, Kmn, k2n, km2n, anca, PhiCaL, PhiCaNa, PhiCaK, zca, PCa, PCap;
    double PCaNa, PCaK, PCaNap, PCaKp, fICaLp;
    
    vfrt = v[id]*frt;
    vffrt = vfrt*F;
    
    CaMKb = CaMKo*(1.0 - CaMKt[id])/(1.0+KmCaM/cass[id]);
    CaMKa = CaMKb + CaMKt[id];
    
    dss = 1.0/(1.0+exp((-(v[id]+3.940))/4.230));
    td = 0.6+1.0/(exp(-0.05*(v[id]+6.0))+exp(0.09*(v[id]+14.0)));
    dd = dss - (dss - d[id])*exp(-dt/td); //dd
    
    fss = 1.0/(1.0+exp((v[id]+19.58)/3.696));
    tff = 7.0+1.0/(0.0045*exp(-((v[id] - taufshift[id])+20.0)/10.0)+0.0045*exp(((v[id] - taufshift[id])+20.0)/10.0));
    tff = tauffac[id]*tff;
    tfs = 1000.0+1.0/(0.000035*exp(-(v[id]+5.0)/4.0)+0.000035*exp((v[id]+5.0)/6.0));
    Aff = 0.6;
    Afs = 1.0-Aff;
    dff = fss - (fss-ff[id])*exp(-dt/tff); //dff
    dfs = fss - (fss-fs[id])*exp(-dt/tfs); //dfs
    
    f = Aff*ff[id] + Afs*fs[id];
    fcass = fss;
    tfcaf = 7.0+1.0/(0.04*exp(-(v[id]-4.0)/7.0)+0.04*exp((v[id]-4.0)/7.0));
    tfcas = 100.0+1.0/(0.00012*exp(-v[id]/3.0)+0.00012*exp(v[id]/7.0));
    Afcaf = 0.3+0.6/(1.0+exp((v[id]-10.0)/10.0));
    Afcas = 1.0-Afcaf;
    dfcaf = fcass - (fcass-fcaf[id])*exp(-dt/tfcaf); //dfcaf
    dfcas = fcass - (fcass-fcas[id])*exp(-dt/tfcas); //dfcas
    
    fca = Afcaf*fcaf[id] + Afcas*fcas[id];
    tjca = 75.0;
    djca = fcass - (fcass - jca[id])*exp(-dt/tjca); //djca
    
    tffp=2.5*tff;
    dffp = fss - (fss - ffp[id])*exp(-dt/tffp); //dffp
    fp = Aff*ffp[id] + Afs*fs[id];
    tfcafp = 2.5*tfcaf;
    dfcafp = fcass - (fcass - fcafp[id])*exp(-dt/tfcafp); //dfcafp
    fcap = Afcaf*fcafp[id] + Afcas*fcas[id];
    Kmn = 0.002;
    k2n = 1000.0;
    km2n = jca[id]*1.0;
    anca = 1.0/(k2n/km2n+pow(1.0+Kmn/cass[id],4.0));
    dnca = anca*k2n/km2n - (anca*k2n/km2n-nca[id])*exp(-km2n*dt); //dnca
    
    PhiCaL = 4.0*vffrt*(cass[id]*exp(2.0*vfrt) - 0.341*cao)/(exp(2.0*vfrt) - 1.0);
    PhiCaNa = 1.0*vffrt*(0.75*nass[id]*exp(1.0*vfrt)-0.75*nao)/(exp(1.0*vfrt) - 1.0);
    PhiCaK = 1.0*vffrt*(0.75*kss[id]*exp(1.0*vfrt) - 0.75*ko)/(exp(1.0*vfrt) - 1.0);
    zca=2.0;
    PCa=0.0001;
    if (celltype[id] == 1)
    {
        PCa*=1.2;
    }
    if (celltype[id] == 2)
    {
        PCa*=2.5;
    }
    PCap = 1.1*PCa;
    PCaNa = 0.00125*PCa;
    PCaK = 3.574e-4*PCa;
    PCaNap = 0.00125*PCap;
    PCaKp = 3.574e-4*PCap;
    fICaLp = (1.0/(1.0+KmCaMK/CaMKa));
    
    ICaL=(1.0-fICaLp)*PCa*PhiCaL*d[id]*(f*(1.0 - nca[id]) + jca[id]*fca*nca[id]) + fICaLp*PCap*PhiCaL*d[id]*(fp*(1.0 - nca[id]) + jca[id]*fcap*nca[id]);
    ICaNa=(1.0-fICaLp)*PCaNa*PhiCaNa*d[id]*(f*(1.0 - nca[id]) + jca[id]*fca*nca[id]) + fICaLp*PCaNap*PhiCaNa*d[id]*(fp*(1.0 - nca[id]) + jca[id]*fcap*nca[id]);
    ICaK=(1.0-fICaLp)*PCaK*PhiCaK*d[id]*(f*(1.0 - nca[id]) + jca[id]*fca*nca[id]) + fICaLp*PCaKp*PhiCaK*d[id]*(fp*(1.0 - nca[id]) + jca[id]*fcap*nca[id]);
    ICaL = icalfac[id]*ICaL;
    ICaNa = icalfac[id]*ICaNa;
    ICaK = icalfac[id]*ICaK;
}

template <int ncells>
double OHaraCell<ncells>::comp_ikr (int id, double dt, double& dxrf, double& dxrs)
{
    double EK, xrss, txrf, txrs, Axrf, Axrs, xr, rkr, GKr, IKr;
    
    EK = (1.0/frt)*log(ko/ki[id]);
    xrss = 1.0/(1.0+exp((-(v[id]+8.337))/6.789));
    txrf = 12.98+1.0/(0.3652*exp((v[id]-31.66)/3.869)+4.123e-5*exp((-(v[id]-47.78))/20.38));
    txrs = 1.865+1.0/(0.06629*exp((v[id]-34.70)/7.355)+1.128e-5*exp((-(v[id]-29.74))/25.94));
    Axrf = 1.0/(1.0+exp((v[id]+54.81)/38.21));
    Axrs = 1.0-Axrf;
    dxrf = xrss - (xrss - xrf[id])*exp(-dt/txrf);  //dxrf
    dxrs = xrss - (xrss - xrs[id])*exp(-dt/txrs); //dxrs
    
    xr = Axrf*xrf[id] + Axrs*xrs[id];
    rkr = 1.0/(1.0+exp((v[id]+55.0)/75.0))*1.0/(1.0+exp((v[id]-10.0)/30.0));
    GKr = 0.046;
    if (celltype[id] == 1)
    {
        GKr*=1.3;
    }
    if (celltype[id] == 2)
    {
        GKr*=0.8;
    }
    IKr = ikrfac[id]*GKr*sqrt(ko/5.4)*xr*rkr*(v[id] - EK);
    
    return IKr;
}

template <int ncells>
double OHaraCell<ncells>::comp_iks(int id, double dt, double& dxs1, double& dxs2)
{
    double EKs, xs1ss, txs1, xs2ss, txs2, KsCa, GKs, IKs;
    
    EKs = (1.0/frt)*log((ko+0.01833*nao)/(ki[id]+0.01833*nai[id]));
    
    xs1ss=1.0/(1.0+exp((-(v[id]+11.60))/8.932));
    txs1=817.3+1.0/(2.326e-4*exp((v[id]+48.28)/17.80)+0.001292*exp((-(v[id]+210.0))/230.0));
    dxs1 = xs1ss - (xs1ss - xs1[id])*exp(-dt/txs1); //dxs1
    
    xs2ss=xs1ss;
    txs2=1.0/(0.01*exp((v[id]-50.0)/20.0)+0.0193*exp((-(v[id]+66.54))/31.0));
    dxs2 = xs2ss - (xs2ss - xs2[id])*exp(-dt/txs2); //dxs2
    
    KsCa=1.0+0.6/(1.0+pow(3.8e-5/cai[id],1.4));
    GKs=0.0034;
    if (celltype[id] == 1)
    {
        GKs*=1.4;
    }
    IKs = iksfac[id]*GKs*KsCa*xs1[id]*xs2[id]*(v[id] - EKs);
    
    return IKs;
}

template <int ncells>
double OHaraCell<ncells>::comp_ik1 (int id, double dt, double& dxk1)
{
    double EK, xk1ss, txk1, rk1, GK1, IK1;
    
    EK = (1.0/frt)*log(ko/ki[id]);
    
    xk1ss=1.0/(1.0+exp(-(v[id]+2.5538*ko+144.59)/(1.5692*ko+3.8115)));
    txk1=122.2/(exp((-(v[id]+127.2))/20.36)+exp((v[id]+236.8)/69.33));
    dxk1 = xk1ss - (xk1ss - xk1[id])*exp(-dt/txk1); //dxk1
    
    rk1=1.0/(1.0+exp((v[id]+105.8-2.6*ko)/9.493));
    GK1=0.1908;
    if (celltype[id] == 1)
    {
        GK1*=1.2;
    }
    if (celltype[id] == 2)
    {
        GK1*=1.3;
    }
    
    IK1 = ik1fac[id]*GK1*sqrt(ko)*rk1*xk1[id]*(v[id] - EK);
    
    return IK1;
}

template <int ncells>
void OHaraCell<ncells>::comp_inaca(int id, double& INaCa_i, double& INaCa_ss) {
    double kna1, kna2, kna3, kasymm, wna, wca, wnaca, kcaon, kcaoff, qna, qca, hca, hna, h1, h2, h3, h4;
    double h5, h6, h7, h8, h9, h10, h11, h12, k1, k2, k3p, k3pp, k3, k4p, k4pp, k4, k5, k6, k7, k8;
    double x1, x2, x3, x4, E1, E2, E3, E4, KmCaAct, allo, zna, zk, zca, JncxNa, JncxCa, Gncx;
    
    kna1=15.0;
    kna2=5.0;
    kna3=88.12;
    kasymm=12.5;
    wna=6.0e4;
    wca=6.0e4;
    wnaca=5.0e3;
    kcaon=1.5e6;
    kcaoff=5.0e3;
    qna=0.5224;
    qca=0.1670;
    hca=exp(qca*v[id]*frt);
    hna=exp(qna*v[id]*frt);
    h1=1.0+nai[id]/kna3*(1+hna);
    h2=(nai[id]*hna)/(kna3*h1);
    h3=1.0/h1;
    h4=1.0+nai[id]/kna1*(1+nai[id]/kna2);
    h5=nai[id]*nai[id]/(h4*kna1*kna2);
    h6=1.0/h4;
    h7=1.0+nao/kna3*(1.0+1.0/hna);
    h8=nao/(kna3*hna*h7);
    h9=1.0/h7;
    h10=kasymm+1.0+nao/kna1*(1.0+nao/kna2);
    h11=nao*nao/(h10*kna1*kna2);
    h12=1.0/h10;
    k1=h12*cao*kcaon;
    k2=kcaoff;
    k3p=h9*wca;
    k3pp=h8*wnaca;
    k3=k3p+k3pp;
    k4p=h3*wca/hca;
    k4pp=h2*wnaca;
    k4=k4p+k4pp;
    k5=kcaoff;
    k6=h6*cai[id]*kcaon;
    k7=h5*h2*wna;
    k8=h8*h11*wna;
    x1=k2*k4*(k7+k6)+k5*k7*(k2+k3);
    x2=k1*k7*(k4+k5)+k4*k6*(k1+k8);
    x3=k1*k3*(k7+k6)+k8*k6*(k2+k3);
    x4=k2*k8*(k4+k5)+k3*k5*(k1+k8);
    E1=x1/(x1+x2+x3+x4);
    E2=x2/(x1+x2+x3+x4);
    E3=x3/(x1+x2+x3+x4);
    E4=x4/(x1+x2+x3+x4);
    KmCaAct=150.0e-6;
    allo=1.0/(1.0+pow(KmCaAct/cai[id],2.0));
    zna = 1.0;
    zk = 1.0;
    zca = 2.0;
    JncxNa=3.0*(E4*k7-E1*k8)+E3*k4pp-E2*k3pp;
    JncxCa=E2*k2-E1*k1;
    Gncx=0.0008;
    if (celltype[id] == 1)
    {
        Gncx*=1.1;
    }
    if (celltype[id] == 2)
    {
        Gncx*=1.4;
    }
    INaCa_i=nacafac[id]*0.8*Gncx*allo*(zna*JncxNa+zca*JncxCa);

    h1=1+nass[id]/kna3*(1+hna);
    h2=(nass[id]*hna)/(kna3*h1);
    h3=1.0/h1;
    h4=1.0+nass[id]/kna1*(1+nass[id]/kna2);
    h5=nass[id]*nass[id]/(h4*kna1*kna2);
    h6=1.0/h4;
    h7=1.0+nao/kna3*(1.0+1.0/hna);
    h8=nao/(kna3*hna*h7);
    h9=1.0/h7;
    h10=kasymm+1.0+nao/kna1*(1+nao/kna2);
    h11=nao*nao/(h10*kna1*kna2);
    h12=1.0/h10;
    k1=h12*cao*kcaon;
    k2=kcaoff;
    k3p=h9*wca;
    k3pp=h8*wnaca;
    k3=k3p+k3pp;
    k4p=h3*wca/hca;
    k4pp=h2*wnaca;
    k4=k4p+k4pp;
    k5=kcaoff;
    k6=h6*cass[id]*kcaon;
    k7=h5*h2*wna;
    k8=h8*h11*wna;
    x1=k2*k4*(k7+k6)+k5*k7*(k2+k3);
    x2=k1*k7*(k4+k5)+k4*k6*(k1+k8);
    x3=k1*k3*(k7+k6)+k8*k6*(k2+k3);
    x4=k2*k8*(k4+k5)+k3*k5*(k1+k8);
    E1=x1/(x1+x2+x3+x4);
    E2=x2/(x1+x2+x3+x4);
    E3=x3/(x1+x2+x3+x4);
    E4=x4/(x1+x2+x3+x4);
    KmCaAct=150.0e-6;
    allo=1.0/(1.0+pow(KmCaAct/cass[id],2.0));
    JncxNa=3.0*(E4*k7-E1*k8)+E3*k4pp-E2*k3pp;
    JncxCa=E2*k2-E1*k1;
    INaCa_ss=nacafac[id]*0.2*Gncx*allo*(zna*JncxNa+zca*JncxCa);

    //INaCa=INaCa_i+INaCa_ss;
    
    //return INaCa;
}

template <int ncells>
double OHaraCell<ncells>::comp_inak(int id) {
    double k1p, k1m, k2p, k2m, k3p, k3m, k4p, k4m, Knai0, Knao0, delta, Knai, Knao, Kki, Kko;
    double MgADP, MgATP, Kmgatp, H, eP, Khp, Knap, Kxkur, P, a1, b1, a2, b2, a3, b3, a4, b4;
    double x1, x2, x3, x4, E1, E2, E3, E4, zk, zna, JnakNa, JnakK, Pnak, INaK;
    
    k1p=949.5;
    k1m=182.4;
    k2p=687.2;
    k2m=39.4;
    k3p=1899.0;
    k3m=79300.0;
    k4p=639.0;
    k4m=40.0;
    Knai0=9.073;
    Knao0=27.78;
    delta=-0.1550;
    Knai=Knai0*exp(delta*v[id]*frt/3.0);
    Knao=Knao0*exp((1.0-delta)*v[id]*frt/3.0);
    Kki=0.5;
    Kko=0.3582;
    MgADP=0.05;
    MgATP=9.8;
    Kmgatp=1.698e-7;
    H=1.0e-7;
    eP=4.2;
    Khp=1.698e-7;
    Knap=224.0;
    Kxkur=292.0;
    P=eP/(1.0+H/Khp+nai[id]/Knap+ki[id]/Kxkur);
    a1=(k1p*pow(nai[id]/Knai,3.0))/(pow(1.0+nai[id]/Knai,3.0)+pow(1.0+ki[id]/Kki,2.0)-1.0);
    b1=k1m*MgADP;
    a2=k2p;
    b2=(k2m*pow(nao/Knao,3.0))/(pow(1.0+nao/Knao,3.0)+pow(1.0+ko/Kko,2.0)-1.0);
    a3=(k3p*pow(ko/Kko,2.0))/(pow(1.0+nao/Knao,3.0)+pow(1.0+ko/Kko,2.0)-1.0);
    b3=(k3m*P*H)/(1.0+MgATP/Kmgatp);
    a4=(k4p*MgATP/Kmgatp)/(1.0+MgATP/Kmgatp);
    b4=(k4m*pow(ki[id]/Kki,2.0))/(pow(1.0+nai[id]/Knai,3.0)+pow(1.0+ki[id]/Kki,2.0)-1.0);
    x1=a4*a1*a2+b2*b4*b3+a2*b4*b3+b3*a1*a2;
    x2=b2*b1*b4+a1*a2*a3+a3*b1*b4+a2*a3*b4;
    x3=a2*a3*a4+b3*b2*b1+b2*b1*a4+a3*a4*b1;
    x4=b4*b3*b2+a3*a4*a1+b2*a4*a1+b3*b2*a1;
    E1=x1/(x1+x2+x3+x4);
    E2=x2/(x1+x2+x3+x4);
    E3=x3/(x1+x2+x3+x4);
    E4=x4/(x1+x2+x3+x4);
    zk = 1.0;
    zna = 1.0;
    JnakNa=3.0*(E1*a3-E2*b3);
    JnakK=2.0*(E4*b1-E3*a1);
    Pnak=30;
    if (celltype[id] == 1)
    {
        Pnak*=0.9;
    }
    if (celltype[id] == 2)
    {
        Pnak*=0.7;
    }
    INaK = Pnak*(zna*JnakNa + zk*JnakK);
    
    return INaK;
}

template <int ncells>
double OHaraCell<ncells>::comp_ikb(int id) 
{
    double EK, xkb, GKb, IKb;
    
    EK = (1.0/frt)*log(ko/ki[id]);
    
    xkb=1.0/(1.0+exp(-(v[id]-14.48)/18.34));
    GKb=0.003;
    if (celltype[id] == 1)
    {
        GKb*=0.6;
    }
    IKb = GKb*xkb*(v[id] - EK);
    
    return IKb;
}

template <int ncells>
double OHaraCell<ncells>::comp_inab(int id) 
{
    double vfrt, vffrt, PNab, INab;
    vfrt = v[id]*frt;
    vffrt = vfrt*F;
    
    PNab=3.75e-10;
    
    INab=PNab*vffrt*(nai[id]*exp(vfrt)-nao)/(exp(vfrt)-1.0);
    
    return INab;
}

template <int ncells>
double OHaraCell<ncells>::comp_icab(int id) 
{
    double vfrt, vffrt, PCab, ICab;
    vfrt = v[id]*frt;
    vffrt = vfrt*F;
    
    PCab=2.5e-8;
    
    ICab = PCab*4.0*vffrt*(cai[id]*exp(2.0*vfrt) - 0.341*cao)/(exp(2.0*vfrt)-1.0);
    
    return ICab;
}

template <int ncells>
double OHaraCell<ncells>::comp_ipca(int id) 
{
    double GpCa, IpCa;
    
    GpCa = 0.0005;
    IpCa = GpCa*cai[id]/(0.0005+cai[id]);
    
    return IpCa;
}

#define gsk 0.005

template <int ncells>
double OHaraCell<ncells>::comp_isk (int id, double dt, double& dxsk)
{
    double ek, z, isk, ccomb, rat, tausk;
    ek = (1.0/frt)*log(ko/ki[id]);
    ccomb = alphask[id]*cass[id] + (1.0 - alphask[id])*cai[id]; // linear combination of cs and ci
    
    rat = pow(skh[id]/ccomb,skn[id]);
    
    z = 1.0/(1.0 + rat); //steady state activation gate
#ifndef TAUSK
    tausk = 0.000001;
#else
    tausk = 5.0 + 25.0/(1.0 + (ccomb/0.0001));
#endif
    
    dxsk = z - (z - xsk[id])*exp(-dt/tausk);
    
    isk = iskfac[id]*gsk*xsk[id]*(v[id] - ek);
    
    return isk;
}

template <int ncells>
void OHaraCell<ncells>::setcell (int id, OHaraCell<1>* newcell)
{
    v[id] = newcell->v[0];
    nai[id] = newcell->nai[0];
    nass[id] = newcell->nass[0];
    ki[id] = newcell->ki[0];
    kss[id] = newcell->kss[0];
    cai[id] = newcell->cai[0];
    cass[id] = newcell->cass[0];
    cansr[id] = newcell->cansr[0];
    cajsr[id] = newcell->cajsr[0];
    m[id] = newcell->m[0];
    hf[id] = newcell->hf[0];
    hs[id] = newcell->hs[0];
    j[id] = newcell->j[0];
    hsp[id] = newcell->hsp[0];
    jp[id] = newcell->jp[0];
    mL[id] = newcell->mL[0];
    hL[id] = newcell->hL[0];
    hLp[id] = newcell->hLp[0];
    a[id] = newcell->a[0];
    iF[id] = newcell->iF[0];
    iS[id] = newcell->iS[0];
    ap[id] = newcell->ap[0];
    iFp[id] = newcell->iFp[0];
    iSp[id] = newcell->iSp[0];
    d[id] = newcell->d[0];
    ff[id] = newcell->ff[0];
    fs[id] = newcell->fs[0];
    fcaf[id] = newcell->fcaf[0];
    fcas[id] = newcell->fcas[0];
    jca[id] = newcell->jca[0];
    nca[id] = newcell->nca[0];
    ffp[id] = newcell->ffp[0];
    fcafp[id] = newcell->fcafp[0];
    xrf[id] = newcell->xrf[0];
    xrs[id] = newcell->xrs[0];
    xs1[id] = newcell->xs1[0];
    xs2[id] = newcell->xs2[0];
    xk1[id] = newcell->xk1[0];
    Jrelnp[id] = newcell->Jrelnp[0];
    Jrelp[id] = newcell->Jrelp[0];
    CaMKt[id] = newcell->CaMKt[0];
    
    celltype[id] = newcell->celltype[0];
    
    diffcurrent[id] = newcell->diffcurrent[0];
    
    itofac[id] = newcell->itofac[0];
    inafac[id] = newcell->inafac[0];
    ikrfac[id] = newcell->ikrfac[0];
    iksfac[id] = newcell->iksfac[0];
    ik1fac[id] = newcell->ik1fac[0];
    
    icalfac[id] = newcell->icalfac[0];
    nacafac[id] = newcell->nacafac[0];
    yshift[id] = newcell->yshift[0];
    vssfac[id] = newcell->vssfac[0]; //
    
    iskfac[id] = newcell->iskfac[0];
    skh[id] = newcell->skh[0];
    skn[id] = newcell->skn[0];
    alphask[id] = newcell->alphask[0];
    xsk[id] = newcell->xsk[0];
    tauffac[id] = newcell->tauffac[0];
    taufshift[id] = newcell->taufshift[0];
    naiclamp[id] = newcell->naiclamp[0]; //
    kiclamp[id] = newcell->kiclamp[0]; //
  
}

template <int ncells>
void OHaraCell<ncells>::getcell (int id, OHaraCell<1>* newcell)
{
    newcell->v[0] = v[id];
    newcell->nai[0] = nai[id];
    newcell->nass[0] = nass[id];
    newcell->ki[0] = ki[id];
    newcell->kss[0] = kss[id];
    newcell->cai[0] = cai[id];
    newcell->cass[0] = cass[id];
    newcell->cansr[0] = cansr[id];
    newcell->cajsr[0] = cajsr[id];
    newcell->m[0] = m[id];
    newcell->hf[0] = hf[id];
    newcell->hs[0] = hs[id];
    newcell->j[0] = j[id];
    newcell->hsp[0] = hsp[id];
    newcell->jp[0] = jp[id];
    newcell->mL[0] = mL[id];
    newcell->hL[0] = hL[id];
    newcell->hLp[0] = hLp[id];
    newcell->a[0] = a[id];
    newcell->iF[0] = iF[id];
    newcell->iS[0] = iS[id];
    newcell->ap[0] = ap[id];
    newcell->iFp[0] = iFp[id];
    newcell->iSp[0] = iSp[id];
    newcell->d[0] = d[id];
    newcell->ff[0] = ff[id];
    newcell->fs[0] = fs[id];
    newcell->fcaf[0] = fcaf[id];
    newcell->fcas[0] = fcas[id];
    newcell->jca[0] = jca[id];
    newcell->nca[0] = nca[id];
    newcell->ffp[0] = ffp[id];
    newcell->fcafp[0] = fcafp[id];
    newcell->xrf[0] = xrf[id];
    newcell->xrs[0] = xrs[id];
    newcell->xs1[0] = xs1[id];
    newcell->xs2[0] = xs2[id];
    newcell->xk1[0] = xk1[id];
    newcell->Jrelnp[0] = Jrelnp[id];
    newcell->Jrelp[0] = Jrelp[id];
    newcell->CaMKt[0] = CaMKt[id];
    
    newcell->celltype[0] = celltype[id];
    
    newcell->diffcurrent[0] = diffcurrent[id];
    
    newcell->itofac[0] = itofac[id];
    newcell->inafac[0] = inafac[id];
    newcell->ikrfac[0] = ikrfac[id];
    newcell->iksfac[0] = iksfac[id];
    newcell->ik1fac[0] = ik1fac[id];
    
    newcell->icalfac[0] = icalfac[id];
    newcell->nacafac[0] = nacafac[id];
    newcell->yshift[0] = yshift[id];
    newcell->vssfac[0] = vssfac[id];
    
    newcell->iskfac[0] = iskfac[id];
    newcell->skh[0] = skh[id];
    newcell->skn[0] = skn[id];
    newcell->alphask[0] = alphask[id];
    newcell->xsk[0] = xsk[id];
    newcell->tauffac[0] = tauffac[id];
    newcell->taufshift[0] = taufshift[id];
    
    newcell->naiclamp[0] = naiclamp[id];
    newcell->kiclamp[0] = kiclamp[id];
}

template <int ncells>
void OHaraCell<ncells>::saveconditions(FILE* file, int id, bool header, double t) {
    if (header) {
        fprintf(file,"t\tv\tm\thf\ths\tj\thsp\tjp\tmL\thL\thLp\ta\tiF\tiS\tap\tiFp\tiSp\td\tff\tfs\tfcaf\tfcas\tjca\tnca\tffp\tfcafp\txrf\txrs\txs1\txs2\txk1\tJrelnp\tJrelp\tCaMKt\tnca\tnca\tnca\tnai\tnass\tki\tkss\tcai\tcass\tcansr\tcajsr\n");
    }
    fprintf(file,"%g\t",t);
    fprintf(file,"%.12f\t",v[id]); //2
    fprintf(file,"%.12f\t",m[id]); //3
    fprintf(file,"%.12f\t",hf[id]); //4
    fprintf(file,"%.12f\t",hs[id]); //5
    fprintf(file,"%.12f\t",j[id]); //6
    fprintf(file,"%.12f\t",hsp[id]); //7
    fprintf(file,"%.12f\t",jp[id]); //8
    fprintf(file,"%.12f\t",mL[id]); //9
    fprintf(file,"%.12f\t",hL[id]); //10
    fprintf(file,"%.12f\t",hLp[id]); //11
    fprintf(file,"%.12f\t",a[id]); //12
    fprintf(file,"%.12f\t",iF[id]); //13
    fprintf(file,"%.12f\t",iS[id]); //14
    fprintf(file,"%.12f\t",ap[id]); //15
    fprintf(file,"%.12f\t",iFp[id]); //16
    fprintf(file,"%.12f\t",iSp[id]); //17
    fprintf(file,"%.12f\t",d[id]); //18
    fprintf(file,"%.12f\t",ff[id]); //19
    fprintf(file,"%.12f\t",fs[id]); //20
    fprintf(file,"%.12f\t",fcaf[id]); //21
    fprintf(file,"%.12f\t",fcas[id]); //22
    fprintf(file,"%.12f\t",jca[id]); //23
    fprintf(file,"%.12f\t",nca[id]); //24
    fprintf(file,"%.12f\t",ffp[id]); //25
    fprintf(file,"%.12f\t",fcafp[id]); //26
    fprintf(file,"%.12f\t",xrf[id]); //27
    fprintf(file,"%.12f\t",xrs[id]); //28
    fprintf(file,"%.12f\t",xs1[id]); //29
    fprintf(file,"%.12f\t",xs2[id]); //30
    fprintf(file,"%.12f\t",xk1[id]); //31
    fprintf(file,"%.12f\t",Jrelnp[id]); //32
    fprintf(file,"%.12f\t",Jrelp[id]); //33
    fprintf(file,"%.12f\t",CaMKt[id]); //34
    fprintf(file,"%.12f\t",nca[id]); //35
    fprintf(file,"%.12f\t",nai[id]); //36
    fprintf(file,"%.12f\t",nass[id]); //37
    fprintf(file,"%.12f\t",ki[id]); //38
    fprintf(file,"%.12f\t",kss[id]); //39
    fprintf(file,"%.12f\t",cai[id]); //40
    fprintf(file,"%.12f\t",cass[id]); //41
    fprintf(file,"%.12f\t",cansr[id]); //42
    fprintf(file,"%.12f\t",cajsr[id]); //43
}



#endif // OHaraCell_cpp
