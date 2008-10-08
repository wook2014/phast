/* $Id: fit_feature.h,v 1.1 2008-08-15 16:57:12 acs Exp $
   Written by Adam Siepel, 2008 */

#ifndef FIT_FEAT_H
#define FIT_FEAT_H

#include <fit_column.h>

/* metadata for fitting scale factors to individual alignment columns */
typedef struct {
  GFF_Feature *feat;
  ColFitData *cdata;
} FeatFitData;

double ff_compute_log_likelihood(TreeModel *mod, MSA *msa, GFF_Feature *feat,
                                 double **scratch);

double ff_scale_derivs(FeatFitData *d, double *first_deriv,
                       double *second_deriv, double ***scratch);

double ff_scale_derivs_subtree(FeatFitData *d, Vector *gradient, 
                                 Matrix *hessian, double ***scratch);

double ff_likelihood_wrapper(Vector *params, void *data);

void ff_grad_wrapper(Vector *grad, Vector *params, void *data, 
                     Vector *lb, Vector *ub);

void ff_lrts(TreeModel *mod, MSA *msa, GFF_Set *feats, mode_type mode, 
             double *feat_pvals, double *feat_scales, double *feat_llrs, 
             FILE *logf);

void ff_lrts_sub(TreeModel *mod, MSA *msa, GFF_Set *gff, mode_type mode, 
                 double *feat_pvals, double *feat_null_scales, 
                 double *feat_scales, double *feat_sub_scales, 
                 double *feat_llrs, FILE *logf);

void ff_score_tests(TreeModel *mod, MSA *msa, GFF_Set *gff, mode_type mode, 
                    double *feat_pvals, double *feat_derivs, 
                    double *feat_teststats);

void ff_score_tests_sub(TreeModel *mod, MSA *msa, GFF_Set *gff, mode_type mode,
                        double *feat_pvals, double *feat_null_scales, 
                        double *feat_derivs, double *feat_sub_derivs, 
                        double *feat_teststats, FILE *logf);

void ff_gerp(TreeModel *mod, MSA *msa, GFF_Set *gff, mode_type mode, 
             double *feat_nneut, double *feat_nobs, double *feat_nrejected, 
             double *feat_nspec, FILE *logf);

FeatFitData *ff_init_fit_data(TreeModel *mod,  MSA *msa, scale_type stype, 
                              mode_type mode, int second_derivs);

void ff_free_fit_data(FeatFitData *d);

void ff_find_missing_branches(TreeModel *mod, MSA *msa, GFF_Feature *feat, 
                              int *has_data, int *nspec);

#endif