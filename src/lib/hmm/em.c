/* $Id: em.c,v 1.7 2004-08-25 18:20:37 acs Exp $
   Written by Adam Siepel, 2003
   Copyright 2003, Adam Siepel, University of California */

/* Experimental code for training a phylo-HMM by EM, including its
   phylogenetic models. */

#include <em.h>
#include <assert.h>
#include <sufficient_stats.h>
#include <fit_em.h>
#include <sys/time.h>

/* generic log function: show log likelihood and all HMM transitions
   probs */
void default_log_function(FILE *logf, double total_logl, HMM *hmm, 
                          void *data, int show_header) {
  int i, j;

  if (show_header) {
    fprintf(logf, "\nlogl\t");
    for (i = 0; i < hmm->nstates; i++) {
      for (j = 0; j < hmm->nstates; j++) {
        fprintf(logf, "(%d,%d)\t", i, j);
      }
    }
    fprintf(logf, "\n");
  }

  fprintf(logf, "%f\t", total_logl);
  for (i = 0; i < hmm->nstates; i++)
    for (j = 0; j < hmm->nstates; j++)
      fprintf(logf, "%f\t", mm_get(hmm->transition_matrix, i, j));
  fprintf(logf, "\n");
  fflush(logf);
}

/* hmm and models must be initialized appropriately */
/* must be one model for every state in the HMM */
/* the ith training sample in data must be of length 'sample_lens[i]' */
/* returns log likelihood of optimized model */
/* pass NULL for estimate_state_models and get_observation_index to
   estimate transition probs only (compute_emissions will only be
   called once) */
double hmm_train_by_em(HMM *hmm, void *models, void *data, int nsamples, 
                       int *sample_lens, gsl_matrix *pseudocounts, 
                       void (*compute_emissions)(double**, void**, int, void*, 
                                                 int, int), 
                       void (*estimate_state_models)(void**, int, void*, 
                                                     double**, int),
                       void (*estimate_transitions)(HMM*, void*, double**),
                       int (*get_observation_index)(void*, int, int),
                       void (*log_function)(FILE*, double, HMM*, void*, int),
                       FILE *logf) { 

  int i, k, l, s, obsidx, nobs, maxlen = 0, done, it;
  double **emissions, **forward_scores, **backward_scores, **E = NULL, **A;
  double *totalE = NULL, *totalA;
  double total_logl, prev_total_logl, val;
  List *val_list;
  int do_state_models = (estimate_state_models != NULL &&
                         get_observation_index != NULL);
  struct timeval start_time, end_time;

  if (logf != NULL)
    gettimeofday(&start_time, NULL);

  for (s = 0; s < nsamples; s++)
    if (sample_lens[s] > maxlen) maxlen = sample_lens[s];

  emissions = (double**)smalloc(hmm->nstates * sizeof(double*));
  forward_scores = (double**)smalloc(hmm->nstates * sizeof(double*));
  backward_scores = (double**)smalloc(hmm->nstates * sizeof(double*));
  for (i = 0; i < hmm->nstates; i++){
    emissions[i] = (double*)smalloc(maxlen * sizeof(double));
    forward_scores[i] = (double*)smalloc(maxlen * sizeof(double));
    backward_scores[i] = (double*)smalloc(maxlen * sizeof(double));
  }
  A = (double**)smalloc(hmm->nstates * sizeof(double*));
  totalA = (double*)smalloc(hmm->nstates * sizeof(double));
  for (k = 0; k < hmm->nstates; k++) 
    A[k] = (double*)smalloc(hmm->nstates * sizeof(double));

  if (do_state_models) {
    nobs = get_observation_index(data, -1, -1); /* this is a bit
                                                   clumsy, but will do
                                                   for now */
    E = (double**)smalloc(hmm->nstates * sizeof(double*));
    totalE = (double*)smalloc(hmm->nstates * sizeof(double));
    for (k = 0; k < hmm->nstates; k++) 
      E[k] = (double*)smalloc(nobs * sizeof(double));
  }

  val_list = lst_new_dbl(hmm->nstates);

  prev_total_logl = NEGINFTY;
  done = FALSE;

  for (it = 1; !done; it++) {
    total_logl = 0;

    /* initialize 'A' and 'E' counts (see below) */
    for (k = 0; k < hmm->nstates; k++) {
      for (l = 0; l < hmm->nstates; l++) A[k][l] = 0;
      totalA[k] = 0;
    }
    if (do_state_models) {
      for (k = 0; k < hmm->nstates; k++) {
        for (obsidx = 0; obsidx < nobs; obsidx++) E[k][obsidx] = 0;
        totalE[k] = 0;
      }
    }

    for (s = 0; s < nsamples; s++) {
      double logp_fw, logp_bw;

      if (do_state_models || s == 0)  /* only do once if not estimating
                                         state models (E == NULL) */
        compute_emissions(emissions, models, hmm->nstates, data, 
                          s, sample_lens[s]);

      logp_fw = hmm_forward(hmm, emissions, sample_lens[s], 
                            forward_scores);
      logp_bw = hmm_backward(hmm, emissions, sample_lens[s], 
                             backward_scores);

      if (abs(logp_fw - logp_bw) > 0.01)
        if (logf != NULL) 
          fprintf(logf, "WARNING: forward and backward algorithms returned different total log\nprobabilities (%f and %f, respectively).\n", logp_fw, logp_bw);

      total_logl += logp_fw;

      for (i = 0; i < sample_lens[s]; i++) {
        double this_logp;

        /* to avoid rounding errors, estimate total log prob
           separately for each column */
        if (do_state_models) {
          lst_clear(val_list);
          for (l = 0; l < hmm->nstates; l++) 
            lst_push_dbl(val_list, (forward_scores[l][i] + 
                                    backward_scores[l][i]));
          this_logp = log_sum(val_list);
          obsidx = get_observation_index(data, s, i);
          for (k = 0; k < hmm->nstates; k++) {
            /* compute expected number of times each state emits each
               distinct observation ('E' in Durbin et al.'s notation; see
               pp. 63-64) */
            val = exp2(forward_scores[k][i] + backward_scores[k][i] - 
                       this_logp);
            E[k][obsidx] += val;
            totalE[k] += val;
          }
        }
           
        /* compute expected number of transitions from each state to
           each other ('A' in Durbin et al.'s notation, pp. 63-64) */
        for (k = 0; k < hmm->nstates; k++) {
          for (l = 0; l < hmm->nstates; l++) {
            val = exp2(forward_scores[k][i] + 
                       hmm_get_transition_score(hmm, k, l) + 
                       emissions[l][i+1] + backward_scores[l][i+1] - 
                       logp_fw);
                                /* FIXME: begin and end states? start
                                   and end idx */
            A[k][l] += val;
            totalA[k] += val;
          }
        }
      }
    }

    if (logf != NULL) {         /* do this before updating params;
                                   otherwise you're outputting the current
                                   likelihood with the new params,
                                   which is confusing */
      if (log_function != NULL)
        log_function(logf, total_logl, hmm, data, it == 1);
      else 
        default_log_function(logf, total_logl, hmm, NULL, it == 1);
    }

    /* check convergence */
    if (total_logl - prev_total_logl <= EM_CONVERGENCE_THRESHOLD)
      done = TRUE;              /* no param update */

    else {
      prev_total_logl = total_logl;

      /* update transitions; use special function if given, otherwise
         assume fully general parameterization  */
      if (estimate_transitions != NULL)
        estimate_transitions(hmm, data, A);
      else 
        for (k = 0; k < hmm->nstates; k++)
          for (l = 0; l < hmm->nstates; l++) 
            mm_set(hmm->transition_matrix, k, l, A[k][l] / totalA[k]);

      /* FIXME: begin and end */

      hmm_reset(hmm);

      /* FIXME: need to use pseudocounts here */  

      /* re-estimate state models */
      if (do_state_models)
        estimate_state_models(models, hmm->nstates, data, E, nobs);
    }
  }

  if (logf != NULL) {
    gettimeofday(&end_time, NULL);
    fprintf(logf, "\nNumber of iterations: %d\nTotal time: %.4f sec.\n", it, 
            end_time.tv_sec - start_time.tv_sec + 
            (end_time.tv_usec - start_time.tv_usec)/1.0e6);
  }

  for (i = 0; i < hmm->nstates; i++) {
    free(emissions[i]);
    free(forward_scores[i]);
    free(backward_scores[i]);
    free(A[i]);
    if (do_state_models) free(E[i]);
  }
  free(emissions);
  free(forward_scores);
  free(backward_scores);
  free(A);
  free(totalA);
  if (do_state_models) {
    free(E);
    free(totalE);
  }
  lst_free(val_list);

  return total_logl;
}

