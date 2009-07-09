#include <lal/ComputeFstat.h>

#include <ea.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <libsync.h>

#include <stdio.h>

#include "cupp/cell/cuda_stub.h"

#include "kernel_t.h"




static void
test_spe_ComputeFStat ( __ea REAL8 *Fstat,                 /**< [out] Fstatistic + Fa, Fb */
	       struct p_spin temp,
	       const __ea MultiSFTVector *multiSFTs,    /**< normalized (by DOUBLE-sided Sn!) data-SFTs of all IFOs */
	       __ea ComputeFBuffer *cfBuffer,            /**< CF-internal buffering structure */
	       const REAL8 deltaF
	       )
{
	struct dim3 threadIdx;

	const UINT4 Dterms = 8;


	Fcomponents retF = _empty_Fcomponents;
	__ea MultiSSBtimes *multiSSB = NULL;
	__ea MultiAMCoeffs *multiAMcoef = NULL;
	REAL8 Ad, Bd, Cd, Dd_inv, Ed;

	multiSSB = cfBuffer->multiSSB;
	multiAMcoef = cfBuffer -> multiAMcoef;
	Ad = multiAMcoef->Mmunu.Ad;
	Bd = multiAMcoef->Mmunu.Bd;
	Cd = multiAMcoef->Mmunu.Cd;
	Dd_inv = 1.0 / (Ad * Bd - Cd * Cd );
	Ed = 0;


	Fcomponents temp_FcX[50][2];

	int i;
	for (i=0; i<blockIdx.x; ++i) {
		temp.fkdot[0] += deltaF;
	}


	UINT4 spdnOrder;		/* maximal spindown-orders */
	/* find highest non-zero spindown-entry */
	for ( spdnOrder = PULSAR_MAX_SPINS - 1;  spdnOrder > 0 ; spdnOrder --  ) {
		if ( temp.fkdot[spdnOrder] ) break;
	}


	typedef __ea SFTVector* sftvector_ptr_ea;
	__ea sftvector_ptr_ea * multiSFTs_data = multiSFTs->data;
	
	typedef __ea AMCoeffs* AMCoeffs_ptr_ea;
	__ea AMCoeffs_ptr_ea * AMCoeffs_temp = multiAMcoef->data;
	
	const UINT4 numDetectors = multiSFTs->length;

	START
	{

		const UINT4 X = threadIdx.y;
		const UINT4 alpha = threadIdx.x;

		temp_FcX[threadIdx.x][threadIdx.y] = _empty_Fcomponents;
		__ea SFTVector* multiSFTs_data_data = multiSFTs_data[X];
		
		Fcomponents *FaFb = &temp_FcX[threadIdx.x][threadIdx.y];
		__ea const SFTVector *sfts = multiSFTs_data_data;
		__ea const SSBtimes *tSSB = multiSSB->data[X];
		__ea const AMCoeffs *amcoe = AMCoeffs_temp[X];

		const UINT4 numSFTs = sfts->length;
		if (alpha >= numSFTs) continue;


		COMPLEX16 Fa, Fb;


		const REAL8 Tsft = 1.0 / sfts->data[0].deltaF;
		const REAL8 dFreq = sfts->data[0].deltaF;
		const INT4 freqIndex0 = (UINT4) ( sfts->data[0].f0 / dFreq + 0.5); /* lowest freqency-index */
		const INT4 freqIndex1 = freqIndex0 + sfts->data[0].data->length;


		Fa.re = 0.0f;
		Fa.im = 0.0f;
		Fb.re = 0.0f;
		Fb.im = 0.0f;

		const __ea REAL4 *a_al = amcoe->a->data + alpha;
		const __ea REAL4 *b_al = amcoe->b->data + alpha;
		const __ea REAL8 *DeltaT_al = tSSB->DeltaT->data + alpha;
		const __ea REAL8 *Tdot_al = tSSB->Tdot->data + alpha;
		const __ea SFTtype *SFT_al = sfts->data + alpha;


		REAL4 a_alpha, b_alpha;

		INT4 kstar;		/* central frequency-bin k* = round(xhat_alpha) */
		INT4 k0, k1;

		__ea COMPLEX8 *Xalpha = SFT_al->data->data; /* pointer to current SFT-data */
     

		REAL4 s_alpha, c_alpha;	/* sin(2pi kappa_alpha) and (cos(2pi kappa_alpha)-1) */
		REAL4 realQ, imagQ;	/* Re and Im of Q = e^{-i 2 pi lambda_alpha} */
		REAL4 realXP, imagXP;	/* Re/Im of sum_k X_ak * P_ak */

		REAL8 lambda_alpha, kappa_max, kappa_star;

		/* ----- calculate kappa_max and lambda_alpha */
		{
			UINT4 s; 		/* loop-index over spindown-order */
			REAL8 phi_alpha, Dphi_alpha, DT_al;
			REAL8 Tas;	/* temporary variable to calculate (DeltaT_alpha)^s */
		
			/* init for s=0 */
			phi_alpha = 0.0;
			Dphi_alpha = 0.0;
			DT_al = (*DeltaT_al);
			Tas = 1.0;		/* DeltaT_alpha ^ 0 */
		
			for (s=0; s <= spdnOrder; s++) {
				REAL8 fsdot = temp.fkdot[s];
				Dphi_alpha += fsdot * Tas * inv_fact[s]; 	/* here: DT^s/s! */
				Tas *= DT_al;				/* now: DT^(s+1) */
				phi_alpha += fsdot * Tas * inv_fact[s+1];
			} /* for s <= spdnOrder */
		
			/* Step 3: apply global factors to complete Dphi_alpha */
			Dphi_alpha *= Tsft * (*Tdot_al);		/* guaranteed > 0 ! */
			lambda_alpha = phi_alpha - 0.5 * Dphi_alpha;
		
			/* real- and imaginary part of e^{-i 2 pi lambda_alpha } */
			temp_sin_cos_2PI_LUT  ( &imagQ, &realQ, - lambda_alpha );
		
			kstar = (INT4) (Dphi_alpha);	/* k* = floor(Dphi_alpha) for positive Dphi */
			kappa_star = Dphi_alpha - 1.0 * kstar;	/* remainder of Dphi_alpha: >= 0 ! */
			kappa_max = kappa_star + 1.0 * Dterms - 1.0;
		
			k0 = kstar - Dterms + 1;
			k1 = k0 + 2 * Dterms - 1;
		} /* compute kappa_star, lambda_alpha */

		temp_sin_cos_2PI_LUT ( &s_alpha, &c_alpha, kappa_star );
		c_alpha -= 1.0f;

		__ea COMPLEX8 *Xalpha_l = Xalpha + k0 - freqIndex0;  /* first frequency-bin in sum */

		realXP = 0;
		imagXP = 0;

		if ( ( kappa_star > LD_SMALL4 ) && (kappa_star < 1.0 - LD_SMALL4) ) {
			REAL4 Sn = (*Xalpha_l).re;
			REAL4 Tn = (*Xalpha_l).im;
			REAL4 pn = kappa_max;
			REAL4 qn = pn;
			REAL4 U_alpha, V_alpha;
		
			UINT4 l;
			for ( l = 1; l < 2*Dterms; l ++ ) {
				Xalpha_l ++;
			
				pn = pn - 1.0f; 			/* p_(n+1) */
				Sn = pn * Sn + qn * (*Xalpha_l).re;	/* S_(n+1) */
				Tn = pn * Tn + qn * (*Xalpha_l).im;	/* T_(n+1) */
				qn *= pn;				/* q_(n+1) */
			} /* for l <= 2*Dterms */
			U_alpha = Sn / qn;
			V_alpha = Tn / qn;
		
		
			realXP = s_alpha * U_alpha - c_alpha * V_alpha;
			imagXP = c_alpha * U_alpha + s_alpha * V_alpha;
		} else {
			UINT4 ind0;
			if ( kappa_star <= LD_SMALL4 ) ind0 = Dterms - 1;
			else ind0 = Dterms;
			realXP = TWOPI_FLOAT * Xalpha_l[ind0].re;
			imagXP = TWOPI_FLOAT * Xalpha_l[ind0].im;
		}

		const REAL4 realQXP = realQ * realXP - imagQ * imagXP;
		const REAL4 imagQXP = realQ * imagXP + imagQ * realXP;

		a_alpha = (*a_al);
		b_alpha = (*b_al);

		Fa.re += a_alpha * realQXP;
		Fa.im += a_alpha * imagQXP;

		Fb.re += b_alpha * realQXP;
		Fb.im += b_alpha * imagQXP;

		temp_FcX[threadIdx.x][threadIdx.y].Fa.re = Fa.re;
		temp_FcX[threadIdx.x][threadIdx.y].Fa.im = Fa.im;
		temp_FcX[threadIdx.x][threadIdx.y].Fb.re = Fb.re;
		temp_FcX[threadIdx.x][threadIdx.y].Fb.im = Fb.im;

		/* Fa = sum_X Fa_X */
		retF.Fa.re += temp_FcX[threadIdx.x][threadIdx.y].Fa.re;
		retF.Fa.im += temp_FcX[threadIdx.x][threadIdx.y].Fa.im;
		
		/* Fb = sum_X Fb_X */
		retF.Fb.re += temp_FcX[threadIdx.x][threadIdx.y].Fb.re;
		retF.Fb.im += temp_FcX[threadIdx.x][threadIdx.y].Fb.im;

	}
	END

	const REAL8 norm = OOTWOPI;

	retF.Fa.re *= norm;
	retF.Fa.im *= norm;
	retF.Fb.re *= norm;
	retF.Fb.im *= norm;

	retF.F = Dd_inv * (   Bd * ( SQ(retF.Fa.re) + SQ(retF.Fa.im) )
	                    + Ad * ( SQ(retF.Fb.re) + SQ(retF.Fb.im) )
	                    - 2.0 * Cd * ( retF.Fa.re * retF.Fb.re + retF.Fa.im * retF.Fb.im )
	                  );


	Fstat[blockIdx.x] = retF.F;
} /* ComputeFStat() */












static BOOLEAN sin_firstCall;


#define LUT_RES         64      /* resolution of lookup-table */
#define LUT_RES_F	(1.0 * LUT_RES)
#define OO_LUT_RES	(1.0 / LUT_RES)

#define X_TO_IND	(1.0 * LUT_RES * OOTWOPI )
#define IND_TO_X	(LAL_TWOPI * OO_LUT_RES)
static int
temp_sin_cos_2PI_LUT (REAL4 *sin2pix, REAL4 *cos2pix, REAL8 x)
{

  REAL8 xt;
  INT4 i0;
  REAL8 d, d2;
  REAL8 ts, tc;
  REAL8 dummy;

  static REAL4 sinVal[LUT_RES+1], cosVal[LUT_RES+1];

  /* the first time we get called, we set up the lookup-table */
  if ( sin_firstCall )
    {
      UINT4 k;
      for (k=0; k <= LUT_RES; k++)
        {
          sinVal[k] = sin( LAL_TWOPI * k * OO_LUT_RES );
          cosVal[k] = cos( LAL_TWOPI * k * OO_LUT_RES );
        }
      sin_firstCall = FALSE;
    }

  xt = modf(x, &dummy);/* xt in (-1, 1) */

  if ( xt < 0.0 )
    xt += 1.0;			/* xt in [0, 1 ) */

  i0 = (INT4)( xt * LUT_RES_F + 0.5 );	/* i0 in [0, LUT_RES ] */
  d = d2 = LAL_TWOPI * (xt - OO_LUT_RES * i0);
  d2 *= 0.5 * d;

  ts = sinVal[i0];
  tc = cosVal[i0];

  /* use Taylor-expansions for sin/cos around LUT-points */
  (*sin2pix) = ts + d * tc - d2 * ts;
  (*cos2pix) = tc - d * ts - d2 * tc;

  return XLAL_SUCCESS;

} /* sin_cos_2PI_LUT() */




// static inline void kernel (__ea datatype* arr, __ea datatype* result) {
// 	struct dim3 threadIdx;
// 
// 	START
// 
// 	atomic_add (arr[threadIdx.x + blockIdx.x*blockDim.x], (unsigned int)result);
// 
// 	END
// }


int main () {

while (1==1) {
	int stack_used = 0;
	__ea char* stack_ptr = (__ea char*) spu_read_in_mbox();
	__ea int* done_ptr = spu_read_in_mbox();

	int i;

	char stack[2*sizeof(struct dim3) + 256];

	for (i=0; i<2*sizeof(struct dim3) + 256; ++i) {
		stack[i] = stack_ptr[i];
	}

	// 1. get grid- and blockdim
	gridDim  = *(struct dim3*) stack;
	blockDim = *(struct dim3*) (stack + sizeof(struct dim3));

	stack_used = 2*sizeof(struct dim3);

	// 2. get the arguments out of the stack
	arg1T arg1;
	for (i=0; i<sizeof(__ea REAL8*); ++i) {
		arg1.dummy[i] = stack[stack_used + i];
	}
	stack_used += sizeof(__ea REAL8*);

	arg2T arg2;
	for (i=0; i<sizeof(struct p_spin); ++i) {
		arg2.dummy[i] = stack[stack_used + i];
	}
	stack_used += sizeof(struct p_spin);

	arg3T arg3;
	for (i=0; i<sizeof(__ea MultiSFTVector*); ++i) {
		arg3.dummy[i] = stack[stack_used + i];
	}
	stack_used += sizeof(__ea MultiSFTVector*);

	arg4T arg4;
	for (i=0; i<sizeof(__ea ComputeFBuffer*); ++i) {
		arg4.dummy[i] = stack[stack_used + i];
	}
	stack_used += sizeof(__ea ComputeFBuffer*);

	arg5T arg5;
	for (i=0; i<sizeof( REAL8); ++i) {
		arg5.dummy[i] = stack[stack_used + i];
	}
	stack_used += sizeof(REAL8);

	sin_firstCall = TRUE;

	while (1==1) {
		const int done = atomic_read( (unsigned int)done_ptr);

		int todo = (gridDim.x - done)/(6*2);
		if (todo <8) todo = 8;

		const int start = atomic_add_return (todo, (unsigned int) done_ptr);

		if (start >= gridDim.x) break;

		int end = start + todo;
		if (end >= gridDim.x) end = gridDim.x;

		for (blockIdx.x=start; blockIdx.x < end; ++blockIdx.x) {
		for (blockIdx.y=0; blockIdx.y<gridDim.y; ++blockIdx.y) {
		for (blockIdx.z=0; blockIdx.z<gridDim.z; ++blockIdx.z) {
	
	// 		sin_firstCall = TRUE;
	
			test_spe_ComputeFStat (arg1.arg, arg2.arg, arg3.arg, arg4.arg, arg5.arg);
	
		}
		}
		}


	}

	__cache_flush();
	spu_write_out_mbox(0);
}

	return 0;
}

