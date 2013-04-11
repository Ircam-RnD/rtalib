/**
 *
 *
 * @file   rta_dct.h
 * @author Jean-Philippe.Lambert@ircam.fr
 * @date   Fri Jun 15 15:29:25 2007
 * 
 * @brief  Discrete Cosine Transform (HTK and Auditory Toolbox styles)
 * 
 * Based on Rastamat by Dan Ellis.
 * See http://www.ee.columbia.edu/~dpwe/resources/matlab/rastamat
 *
 * Copyright (C) 2007 by IRCAM-Centre Georges Pompidou, Paris, France.
 * 
 */
#ifndef _RTA_DCT_H_
#define _RTA_DCT_H_ 1

#include "rta.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  rta_dct_plp = 1,    /**< dpwe type 1 - same as old spec2cep that expanded & used fft */
  rta_dct_slaney = 2, /**< orthogonal and unitary (Auditory Toolbox like) */
  rta_dct_htk = 3,    /**< orthogonal but not unitary (HTK like) */
  rta_dct_feacalc = 4 /**< type 1 with implicit repeating of first, last bins */
} rta_dct_t;


/* from spec2cep.m  */

/** 
 * Generate a matrix of weights 'weights_matrix' to perform a discrete
 * cosine transform. This is type II dct, or type I if type is
 * specified as 'PLPDCT'.
 * 
 * @param weights_matrix size is 'input_size'*'dct_order'
 * @param input_size number of input mel bands
 * @param dct_order number of output dct coefficients
 * @param dct_type 
 * PLPDCT dpwe type 1: same as old spec2cep that expanded & used fft 
 * slaney_dct: orthogonal and unitary 
 * HTKDCT: orthogonal but not unitary (HTK like) 
 * feacalc_dct: PLPDCT with implicit repeating of first, last bins 
 *   Deep in the heart of the rasta/feacalc code, there is the logic
 *   that the first and last auditory bands extend beyond the edge of
 *   the actual spectra, and they are thus copied from their neighbors.
 *   Normally, we just ignore those bands and take the 19 in the middle,
 *   but when feacalc calculates mfccs, it actually takes the cepstrum
 *   over the spectrum *including* the repeated bins at each end.  Here,
 *   we simulate 'repeating' the bins and an nrow+2-length spectrum by
 *   adding in extra DCT weight to the first and last bins.  
 * 
 * @return 1 on success 0 on fail
 */
int rta_dct_weights(rta_real_t * weights_matrix, 
                  const unsigned int input_size,
                  const unsigned int dct_order,
                  const rta_dct_t dct_type);

/** 
 * Generate a matrix of weights 'weights_matrix' to perform a discrete
 * cosine transform. This is type II dct, or type I if type is
 * specified as 'PLPDCT'.
 * 
 * @param weights_matrix size is 'input_size'*'dct_order'
 * @param w_stride is 'weights_matrix' stride
 * @param input_size number of input mel bands
 * @param dct_order number of output dct coefficients
 * @param dct_type 
 * PLPDCT dpwe type 1: same as old spec2cep that expanded & used fft 
 * slaney_dct: orthogonal and unitary 
 * HTKDCT: orthogonal but not unitary (HTK like) 
 * feacalc_dct: PLPDCT with implicit repeating of first, last bins 
 *   Deep in the heart of the rasta/feacalc code, there is the logic
 *   that the first and last auditory bands extend beyond the edge of
 *   the actual spectra, and they are thus copied from their neighbors.
 *   Normally, we just ignore those bands and take the 19 in the middle,
 *   but when feacalc calculates mfccs, it actually takes the cepstrum
 *   over the spectrum *including* the repeated bins at each end.  Here,
 *   we simulate 'repeating' the bins and an nrow+2-length spectrum by
 *   adding in extra DCT weight to the first and last bins.  
 * 
 * @return 1 on success 0 on fail
 */
int rta_dct_weights_stride(rta_real_t * weights_matrix, const int w_stride,
                        const unsigned int input_size,
                        const unsigned int dct_order,
                        const rta_dct_t dct_type);

/** 
 * Perform a discrete cosine transform as
 * 'dct' = 'weights_matrix'*'input_vector'
 * 
 * Calculate 'dct' from the log of the spectral samples
 * 'input_vector' as 
 * 'dct' = 'weights_matrix'*log('input_vector')
 * to get MFCC.
 * 
 * @param dct size is 'dct_order'
 * @param input_vector size is 'input_size' it is actually the log of the
 * mel bands
 * @param weights_matrix size is 'input_size'*'dct_order'
 * @param input_size number of input mel bands
 * @param dct_order number of output dct coefficients
 * 
 */
void rta_dct(rta_real_t * dct, const rta_real_t * input_vector,
            const rta_real_t * weights_matrix,
            const unsigned int input_size,
            const unsigned int dct_order);

/** 
 * 
 * Perform a discrete cosine transform as
 * 'dct' = 'weights_matrix'*'input_vector'
 * 
 * Calculate 'dct' from the log of the spectral samples
 * 'input_vector' as 
 * 'dct' = 'weights_matrix'*log('input_vector')
 * to get MFCC.
 * 
 * @param dct size is 'dct_order'
 * @param d_stride is 'dct' stride
 * @param input_vector size is 'input_size' it is actually the log of the
 * mel bands
 * @param i_stride is 'input_vector' stride
 * @param weights_matrix size is 'input_size'*'dct_order'
 * @param w_stride is 'weights_matrix' stride
 * @param input_size number of input mel bands
 * @param dct_order number of output dct coefficients
 *
 */
void rta_dct_stride(rta_real_t * dct, const int d_stride,
                  const rta_real_t * input_vector, const int i_stride,
                  const rta_real_t * weights_matrix, const int w_stride,
                  const unsigned int input_size,
                  const unsigned int dct_order);

#ifdef __cplusplus
}
#endif

#endif /* _RTA_DCT_H_ */

