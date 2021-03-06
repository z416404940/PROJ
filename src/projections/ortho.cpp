#define PJ_LIB__
#include <errno.h>
#include "proj.h"
#include "proj_internal.h"
#include <math.h>

PROJ_HEAD(ortho, "Orthographic") "\n\tAzi, Sph";

namespace { // anonymous namespace
enum Mode {
    N_POLE = 0,
    S_POLE = 1,
    EQUIT  = 2,
    OBLIQ  = 3
};
} // anonymous namespace

namespace { // anonymous namespace
struct pj_opaque {
    double  sinph0;
    double  cosph0;
    enum Mode mode;
};
} // anonymous namespace

#define EPS10 1.e-10

static PJ_XY forward_error(PJ *P, PJ_LP lp, PJ_XY xy) {
    proj_errno_set(P, PJD_ERR_TOLERANCE_CONDITION);
    proj_log_trace(P, "Coordinate (%.3f, %.3f) is on the unprojected hemisphere",
                   proj_todeg(lp.lam), proj_todeg(lp.phi));
    return xy;
}

static PJ_XY ortho_s_forward (PJ_LP lp, PJ *P) {           /* Spheroidal, forward */
    PJ_XY xy;
    struct pj_opaque *Q = static_cast<struct pj_opaque*>(P->opaque);
    double  coslam, cosphi, sinphi;

    xy.x = HUGE_VAL; xy.y = HUGE_VAL;

    cosphi = cos(lp.phi);
    coslam = cos(lp.lam);
    switch (Q->mode) {
    case EQUIT:
        if (cosphi * coslam < - EPS10)
            return forward_error(P, lp, xy);
        xy.y = sin(lp.phi);
        break;
    case OBLIQ:
        sinphi = sin(lp.phi);
        if (Q->sinph0 * sinphi + Q->cosph0 * cosphi * coslam < - EPS10)
            return forward_error(P, lp, xy);
        xy.y = Q->cosph0 * sinphi - Q->sinph0 * cosphi * coslam;
        break;
    case N_POLE:
        coslam = - coslam;
                /*-fallthrough*/
    case S_POLE:
        if (fabs(lp.phi - P->phi0) - EPS10 > M_HALFPI)
            return forward_error(P, lp, xy);
        xy.y = cosphi * coslam;
        break;
    }
    xy.x = cosphi * sin(lp.lam);
    return xy;
}


static PJ_LP ortho_s_inverse (PJ_XY xy, PJ *P) {           /* Spheroidal, inverse */
    PJ_LP lp;
    struct pj_opaque *Q = static_cast<struct pj_opaque*>(P->opaque);
    double sinc;

    lp.lam = HUGE_VAL; lp.phi = HUGE_VAL;

    const double rh = hypot(xy.x, xy.y);
    sinc = rh;
    if (sinc > 1.) {
        if ((sinc - 1.) > EPS10) {
            proj_errno_set(P, PJD_ERR_TOLERANCE_CONDITION);
            proj_log_trace(P, "Point (%.3f, %.3f) is outside the projection boundary");
            return lp;
        }
        sinc = 1.;
    }
    const double cosc = sqrt(1. - sinc * sinc); /* in this range OK */
    if (fabs(rh) <= EPS10) {
        lp.phi = P->phi0;
        lp.lam = 0.0;
    } else {
        switch (Q->mode) {
        case N_POLE:
            xy.y = -xy.y;
            lp.phi = acos(sinc);
            break;
        case S_POLE:
            lp.phi = - acos(sinc);
            break;
        case EQUIT:
            lp.phi = xy.y * sinc / rh;
            xy.x *= sinc;
            xy.y = cosc * rh;
            goto sinchk;
        case OBLIQ:
            lp.phi = cosc * Q->sinph0 + xy.y * sinc * Q->cosph0 /rh;
            xy.y = (cosc - Q->sinph0 * lp.phi) * rh;
            xy.x *= sinc * Q->cosph0;
        sinchk:
            if (fabs(lp.phi) >= 1.)
                lp.phi = lp.phi < 0. ? -M_HALFPI : M_HALFPI;
            else
                lp.phi = asin(lp.phi);
            break;
        }
        lp.lam = (xy.y == 0. && (Q->mode == OBLIQ || Q->mode == EQUIT))
             ? (xy.x == 0. ? 0. : xy.x < 0. ? -M_HALFPI : M_HALFPI)
                           : atan2(xy.x, xy.y);
    }
    return lp;
}



PJ *PROJECTION(ortho) {
    struct pj_opaque *Q = static_cast<struct pj_opaque*>(pj_calloc (1, sizeof (struct pj_opaque)));
    if (nullptr==Q)
        return pj_default_destructor(P, ENOMEM);
    P->opaque = Q;

    if (fabs(fabs(P->phi0) - M_HALFPI) <= EPS10)
        Q->mode = P->phi0 < 0. ? S_POLE : N_POLE;
    else if (fabs(P->phi0) > EPS10) {
        Q->mode = OBLIQ;
        Q->sinph0 = sin(P->phi0);
        Q->cosph0 = cos(P->phi0);
    } else
        Q->mode = EQUIT;
    P->inv = ortho_s_inverse;
    P->fwd = ortho_s_forward;
    P->es = 0.;

    return P;
}

