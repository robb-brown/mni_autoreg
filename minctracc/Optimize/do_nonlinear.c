/* ----------------------------- MNI Header -----------------------------------
@NAME       : do_nonlinear.c
@DESCRIPTION: routines to do non-linear registration by local linear
              correlation.
@COPYRIGHT  :
              Copyright 1993 Louis Collins, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.

@CREATED    : Thu Nov 18 11:22:26 EST 1993 LC

@MODIFIED   : $Log: do_nonlinear.c,v $
@MODIFIED   : Revision 1.15  1995-05-02 11:30:27  louis
@MODIFIED   : started clean up of code, separation of non used procedures into
@MODIFIED   : old_methods.c.  This version was working, but I am now going to
@MODIFIED   : rewrite everything to use GRID_TRANSFORM.
@MODIFIED   :
 * Revision 1.14  1995/02/22  08:56:06  louis
 * Montreal Neurological Institute version.
 * compiled and working on SGI.  this is before any changes for SPARC/
 * Solaris.
 *
 * Revision 1.13  94/06/23  09:18:04  louis
 * working version before testing of increased cpu time when calculating
 * deformations on a particular slice.
 * 
 * Revision 1.12  94/06/21  10:58:32  louis
 * working optimized version of program.  when compiled with -O, this
 * code is approximately 60% faster than the previous version.
 * 
 * 
 * Revision 1.11  94/06/19  15:43:50  louis
 * clean working version of 3D local deformation with simplex optimization
 * (by default) on magnitude data (default).  No more FFT stuff.
 * This working version before change of deformation field in do_nonlinear.c
 * 
 * 
 * Revision 1.10  94/06/19  15:00:20  louis
 * working version with 3D simplex optimization to find local deformation
 * vector.  time for cleanup.  will remove all fft stuff from code in 
 * next version.
 * 
 * 
 * Revision 1.9  94/06/18  12:29:12  louis
 * temporary version.  trying to find previously working simplex
 * method.
 * 
 * Revision 1.8  94/06/17  10:29:20  louis
 * this version has both a simplex method and convolution method 
 * working to find the best local offset for a given lattice node.
 * 
 * The simplex method works well, is stable and yields good results.
 * The convolution method also works, but the discretization of the 
 * possible deformed vector yields results that are not always stable.
 * 
 * The conv technique is only 2 times faster than the simplex method, 
 * and therefore does not warrent further testing....
 * 
 * I will now modify the simplex method:
 *    - use nearest neighbour interpolation only for the moving lattice
 *    - use tri-cubic for the non-moving lattice
 *    - swap the moving and stable grids.  now the source grid moves,
 *      the target (deformed) grid should move to increase the effective
 *      resolution of the deformation vector (over the linear interpolant
 *      limit).
 * 
 * 
 * Revision 1.7  94/06/15  09:46:47  louis
 * non-working version using FFT for local def.
 *
 * Revision 1.6  94/06/06  18:45:24  louis
 * working version: clamp and blur of deformation lattice now ensures
 * a smooth recovered deformation.  Unfortunately, the r = cost-similarity
 * function used in the optimization is too heavy on the cost_fn.  This has
 * to get fixed...
 * 
 * 
 * Revision 1.5  94/06/06  09:32:41  louis
 * working version: 2d deformations based on local neighbourhood correlation.
 * numerous small bugs over 1.4.  Major bug fix: the routine will now 
 * properly calculate the additional warp (instead of the absolute amount)
 * that needs to be added.  This was due to a mis-type in a call to
 * go_get_values_with_offset.  It was being called with points from the
 * target volume, when they should have been from the source vol.
 * 
 * Some fixes still need to be done: smoothing, clamp 1st deriv, balence
 * of r = cost +  similarity.
 * 
 * Revision 1.4  94/06/02  20:12:07  louis
 * made modifications to allow deformations to be calulated in 2D on slices. 
 * changes had to be made in set_up_lattice, init_lattice when defining
 * the special case of a single slice....
 * Build_default_deformation_field also had to reflect these changes.
 * do_non-linear-optimization also had to check if one of dimensions had
 * a single element.
 * All these changes were made, and slightly tested.  Another type of
 * deformation strategy will be necessary (to replace the deformation 
 * perpendicular to the surface, since it does not work well).
 * 
 * Revision 1.3  94/05/28  15:57:33  louis
 * working version, before removing smoothing and adding springs!
 * 
 * Revision 1.2  94/04/06  11:47:47  louis
 * working linted version of linear + non-linear registration based on Lvv
 * operator working in 3D
 * 
 * Revision 1.1  94/02/21  16:33:41  louis
 * Initial revision
 * 
---------------------------------------------------------------------------- */

#ifndef lint
static char rcsid[]="$Header: /private-cvsroot/registration/mni_autoreg/minctracc/Optimize/do_nonlinear.c,v 1.15 1995-05-02 11:30:27 louis Exp $";
#endif



#include <volume_io.h>
#include "arg_data.h"
#include "deform_field.h"
#include "line_data.h"
#include "local_macros.h"
#include <print_error.h>
#include <limits.h>
#include <recipes.h>


#include <sys/types.h>
#include <time.h>

time_t time(time_t *tloc);


#define BRUTE_FORCE     1
#define SECANT_METHOD   0
#define TEST_LENGTH     1.0

#define EQUAL(a,b) ( ABS( (a) - (b)) < 0.000001)

static Volume   Gd1;
static Volume   Gd1_dx; 
static Volume   Gd1_dy; 
static Volume   Gd1_dz; 
static Volume   Gd1_dxyz;
static Volume   Gd2;
static Volume   Gd2_dx; 
static Volume   Gd2_dy; 
static Volume   Gd2_dz; 
static Volume   Gd2_dxyz;
static Volume   Gm1;
static Volume   Gm2; 
static Real     Gsimplex_size;
static Real     Gcost_radius;
static General_transform 
                *Glinear_transform;
static  Volume  Gsuper_dx,Gsuper_dy,Gsuper_dz;
static float    Gsqrt_s1;

static Arg_Data *Gglobals;

static float			/* these are used to communicate to the correlation */
  *Ga1xyz,			/* functions over top the SIMPLEX optimization */
  *Ga2xyz,			/* routine */
  *TX, *TY, *TZ,
  *SX, *SY, *SZ;
static int
  Glen;


extern double iteration_weight;
extern double similarity_cost_ratio;
extern int    iteration_limit;
extern int    number_dimensions;
extern double ftol;

/* prototypes */

public void make_super_sampled_data(Volume orig_dx, Volume *dx, Volume *dy, Volume *dz);

public void interpolate_super_sampled_data(Volume orig_dx, Volume orig_dy, Volume orig_dz,
				      Volume dx, Volume dy, Volume dz,
				      int dim);

int amoeba2(float **p, 
	    float y[], 
	    int ndim, 
	    float ftol, 
	    float (*funk)(), 
	    int *nfunk);

public void    build_source_lattice(Real x, Real y, Real z,
				    float PX[], float PY[], float PZ[],
				    Real fwhm_x, Real fwhm_y, Real fwhm_z, 
				    int nx, int ny, int nz,
				    int ndim, int *length);

public void    build_target_lattice1(float px[], float py[], float pz[],
				    float tx[], float ty[], float tz[],
				    int len);

public void    build_target_lattice2(float px[], float py[], float pz[],
				    float tx[], float ty[], float tz[],
				    int len);


public void go_get_samples_in_source(Volume data,
			   float x[], float y[], float z[],
			   float samples[],
			   int len,
			   int inter_type);

public float go_get_samples_with_offset(Volume data,
				       float *x, float *y, float *z,
				       Real dx, Real dy, Real dz,
				       int len, float sqrt_s1, float *a1) ;

private Real cost_fn(float x, float y, float z, Real max_length);

private float xcorr_fitting_function(float *x);

public Status save_deform_data(Volume dx,
			       Volume dy,
			       Volume dz,
			       char *name,
			       char *history);

private Real optimize_3D_deformation_for_single_node(Real spacing, Real threshold1, 
						     Real src_x, Real src_y, Real src_z,
						     Real mx, Real my, Real mz,
						     Real *def_x, Real *def_y, Real *def_z,
						     int iteration, int total_iters,
						     int *nfunks,
						     int ndim);

private BOOLEAN get_best_start_from_neighbours(Real threshold1, 
					       Real wx, Real wy, Real wz,
					       Real mx, Real my, Real mz,
					       Real *tx, Real *ty, Real *tz,
					       Real *d1x_dir, Real *d1y_dir, Real *d1z_dir,
					       Real *def_x, Real *def_y, Real *def_z);
  

public Real get_value_of_point_in_volume(Real xworld, Real yworld, Real zworld, 
					  Volume data_x);



public void clamp_warp_deriv(Volume dx, Volume dy, Volume dz);


public void get_average_warp_of_neighbours(Volume dx, Volume dy, Volume dz, 
					   int i,int j,int k,
					   Real *mx, Real *my, Real *mz);

public void smooth_the_warp(Volume smooth_dx, Volume smooth_dy, Volume smooth_dz, 
			    Volume warp_dx, Volume warp_dy, Volume warp_dz,
			    Volume warp_mag, Real thres);

public void add_additional_warp_to_current(Volume dx, Volume dy, Volume dz,
					    Volume adx, Volume ady, Volume adz,
					    Real weight);

public Real get_maximum_magnitude(Volume dxyz);

public void save_data(char *basename, int i, int j,
		      General_transform *transform);




/**************************************************************************/

public Status do_non_linear_optimization(Volume d1,
					 Volume d1_dx, 
					 Volume d1_dy, 
					 Volume d1_dz, 
					 Volume d1_dxyz,
					 Volume d2,
					 Volume d2_dx, 
					 Volume d2_dy, 
					 Volume d2_dz, 
					 Volume d2_dxyz,
					 Volume m1,
					 Volume m2, 
					 Arg_Data *globals)
{
  General_transform
    *additional_warp,
    *current_warp;

  Volume
    additional_dx,additional_dy,additional_dz,
    additional_mag;
  Deform_field 
    *current;
  
  long
    timer1,timer2,
    nfunk_total;
  int 
    loop_start[3], loop_end[3],
    iters,
    i,j,k,
    nodes_done, nodes_tried, nodes_seen, matching_type, over,
    nfunks,

    nfunk1, nodes1,

    sizes[3];
  Real 
    min, max, sum, sum2, mag, mean_disp_mag, std, var, 
    voxel,val_x, val_y, val_z,
    steps[3],steps_data[3],
    def_x, def_y, def_z, 
    wx,wy,wz,
    mx,my,mz,
    tx,ty,tz, 
    displace,
    zero,
    threshold1,
    threshold2,
    result;
  progress_struct
    progress;
  General_transform 
    *all_until_last, *tmp_trans;

				/* set up globals for communication with other routines */
  Gd1     = d1;
  Gd1_dx  = d1_dx; 
  Gd1_dy  = d1_dy; 
  Gd1_dz  = d1_dz; 
  Gd1_dxyz= d1_dxyz;
  Gd2     = d2;
  Gd2_dx  = d2_dx; 
  Gd2_dy  = d2_dy; 
  Gd2_dz  = d2_dz; 
  Gd2_dxyz= d2_dxyz;
  Gm1     = m1;
  Gm2     = m2; 
  Gglobals= globals;


  Ga1xyz = vector(1,512);	/* allocate space for the global data for */
  Ga2xyz = vector(1,512);	/* the local neighborhood lattice values */
  
  SX = vector(1,512);		/* and coordinates in source volume  */
  SY = vector(1,512);
  SZ = vector(1,512);
  TX = vector(1,512);		/* and coordinates in target volume  */
  TY = vector(1,512);
  TZ = vector(1,512);




  /*******************************************************************************/
  /*  copy all but the last general transformation (which should be non-lin) to  */
  /*  form a general transformation 'all_until_last'                             */
  
  ALLOC(all_until_last,1)
				/* copy the first transform from the global data struct  */
  copy_general_transform(get_nth_general_transform(globals->trans_info.transformation, 0),
			 all_until_last);

				/* copy and concat the rest of tem, stopping before the end */
  for_less(i,1,get_n_concated_transforms(globals->trans_info.transformation)-1){
    ALLOC(tmp_trans,1);
    copy_general_transform(get_nth_general_transform(globals->trans_info.transformation, i),
			   tmp_trans);
    concat_general_transforms(all_until_last, 
			     tmp_trans,
			     all_until_last);
  }
  if (globals->flags.debug) {	/* print some debugging info    */
    print("orig transform is %d long\n",
	  get_n_concated_transforms(globals->trans_info.transformation));
    print("all_until_last is %d long\n",
	  get_n_concated_transforms(all_until_last));
  }

  Glinear_transform = all_until_last; /* set up linear part of transformation   */

  /*******************************************************************************/
  /*  get a pointer to the last non-linear transform in the globals struct.      */
  /*  so that it can be used to update the global transformation at each         */
  /*  iterative step.                                                            */

  current = (Deform_field *)NULL;
  for_less(i,0,get_n_concated_transforms(globals->trans_info.transformation)) {
    if (get_transform_type( get_nth_general_transform(globals->trans_info.transformation,i) ) 
	       == USER_TRANSFORM)
      current = (Deform_field *)get_nth_general_transform(globals->trans_info.transformation,i)->user_data;
  }
  if (current == (Deform_field *)NULL) { /* exit if no deformation */
    print_error_and_line_num("Cannot find the deformation field transform to optimize",
		__FILE__, __LINE__);
  }


  /*******************************************************************************/
  /*   build and allocate the temporary deformation volume needed                */

  additional_dx = copy_volume_definition(current->dx, NC_UNSPECIFIED, FALSE, 0.0,0.0);
  additional_dy = copy_volume_definition(current->dy, NC_UNSPECIFIED, FALSE, 0.0,0.0);
  additional_dz = copy_volume_definition(current->dz, NC_UNSPECIFIED, FALSE, 0.0,0.0);
  additional_mag = copy_volume_definition(current->dz, NC_UNSPECIFIED, FALSE, 0.0,0.0);

  alloc_volume_data(additional_dx);
  alloc_volume_data(additional_dy);
  alloc_volume_data(additional_dz);
  alloc_volume_data(additional_mag);

  get_volume_sizes(additional_dx, sizes);
  get_volume_separations(additional_dx, steps);

  get_volume_separations(d2_dxyz, steps_data);

  if (steps_data[0]!=0.0) {
    Gsimplex_size= ABS(steps[0]/steps_data[0]);
    if (ABS(Gsimplex_size) < ABS(steps_data[0])) {
      print ("*** WARNING ***\n");
      print ("Simplex size will be smaller than data voxel size (%f < %f)\n",
	     Gsimplex_size,steps_data[0]);
    }
  }
  else
    print_error_and_line_num("Zero step size for gradient data2: %f %f %f\n", 
		__FILE__, __LINE__,steps_data[0],steps_data[1],steps_data[2]);


  Gcost_radius = 8*Gsimplex_size*Gsimplex_size*Gsimplex_size;


  /*******************************************************************************/
  /*   initialize this iterations warp                                           */

  zero = CONVERT_VALUE_TO_VOXEL(additional_dx, 0.0);	
  for_less(i,0,sizes[0])
    for_less(j,0,sizes[1])
      for_less(k,0,sizes[2]){
	SET_VOXEL_3D(additional_dx, i,j,k, zero);
	SET_VOXEL_3D(additional_dy, i,j,k, zero);
	SET_VOXEL_3D(additional_dz, i,j,k, zero);
	SET_VOXEL_3D(additional_mag, i,j,k, zero);
      }
  mean_disp_mag = 0.0;


  /*******************************************************************************/
  /*    set the threshold to be 10% of the maximum gradient magnitude            */
  /*    for each source and target volumes                                       */

  if (globals->threshold[0]==0.0)
    threshold1 = 0.10 * get_maximum_magnitude(d1_dxyz);
  else
    threshold1 = globals->threshold[0] * get_maximum_magnitude(d1_dxyz);

  if (globals->threshold[1]==0.0)
    threshold2 = 0.10 * get_maximum_magnitude(d2_dxyz);
  else
    threshold2 = globals->threshold[1] * get_maximum_magnitude(d2_dxyz);

  if (threshold1<0.0 || threshold2<0.0) {
    print_error_and_line_num("Gradient magnitude threshold error: %f %f\n", __FILE__, __LINE__, 
		threshold1, threshold2);
  }
  if (globals->flags.debug) {	
    print("Source vol threshold = %f\n", threshold1);
    print("Target vol threshold = %f\n", threshold2);
    print("Iteration limit      = %d\n", iteration_limit);
    print("Iteration weight     = %f\n", iteration_weight);
  }

  /*******************************************************************************/
  /*    start the iterations to estimate the warp                     

	for each iteration {
	   for each voxel in the deformation field {
	      get the original coordinate node
	      find the best deformation for the node, taking into consideration
	        the previous deformation
	      store this best deformation
	   }

	   for each voxel in the deformation field {
 	      add WEIGHTING*best_deformation to the current deformation
	   }

	}
  */

  for_less(i,0,3) {
    if (sizes[i]>3) {
      loop_start[i] = 1;
      loop_end[i] = sizes[i]-1;
    }
    else {
      loop_start[i]=0;
      loop_end[i] = sizes[i];
    }
  }

  if (globals->flags.debug) {
    print("loop: (%d %d) (%d %d) (%d %d)\n",
	  loop_start[0],loop_end[0],loop_start[1],loop_end[1],loop_start[2],loop_end[2]);
  }

  if (globals->trans_info.use_super) {
    make_super_sampled_data(current->dx, &Gsuper_dx,  &Gsuper_dy,  &Gsuper_dz);
  }
  else {
    Gsuper_dx = current->dx;
    Gsuper_dy = current->dy;
    Gsuper_dz = current->dz;
  }


  for_less(iters,0,iteration_limit) {

    if (globals->trans_info.use_super)
      interpolate_super_sampled_data(current->dx, current->dy, current->dz,
				     Gsuper_dx,  Gsuper_dy,  Gsuper_dz,
				     number_dimensions);
  
    
    print("Iteration %2d of %2d\n",iters+1, iteration_limit);

    if (Gglobals->trans_info.use_simplex==TRUE) 
      print ("will use simplex\n");
    else
      print ("will use FFT\n");

    
    /* if (iters<3) matching_type = BRUTE_FORCE; else matching_type = SECANT_METHOD; */

    matching_type = BRUTE_FORCE; /* force matching type to be BRUTE_FORCE for now!!! */

    nodes_done = 0; nodes_tried = 0; nodes_seen=0; displace = 0.0; over = 0;
    nfunk_total = 0;

    sum = sum2 = 0.0;
    min = 1000.0;
    max = -1000.0;

    initialize_progress_report( &progress, FALSE, 
			       (loop_end[0]-loop_start[0])*(loop_end[1]-loop_start[1]) + 1,
			       "Estimating deformations" );

    for_less(i,loop_start[0],loop_end[0]) {

      timer1 = time(NULL);
      nfunk1 = 0; nodes1 = 0;

      for_less(j,loop_start[1],loop_end[1]) {
	for_less(k,loop_start[2],loop_end[2]){
	  
	  nodes_seen++;
	  def_x = def_y = def_z = 0.0;
	  
				/* get the lattice coordinate */
	  convert_3D_voxel_to_world(current->dx, 
				    (Real)i, (Real)j, (Real)k,
				    &wx, &wy, &wz);

				/* get the warp to be added to the target point */
	  GET_VOXEL_3D(voxel, current->dx, i,j,k); 
	  val_x = CONVERT_VOXEL_TO_VALUE(current->dx , voxel ); 
	  GET_VOXEL_3D(voxel, current->dy, i,j,k); 
	  val_y = CONVERT_VOXEL_TO_VALUE(current->dy , voxel ); 
	  GET_VOXEL_3D(voxel, current->dz, i,j,k); 
	  val_z = CONVERT_VOXEL_TO_VALUE(current->dz , voxel ); 

				/* add the warp to the target lattice point */
	  wx += val_x; wy += val_y; wz += val_z;

	  if (point_not_masked(m2, wx, wy, wz) &&
	      get_value_of_point_in_volume(wx,wy,wz, Gd2_dxyz)>threshold2) {


			    /* now get the mean warped position of the target's neighbours */

	    get_average_warp_of_neighbours(current->dx, current->dy, current->dz, 
					   i,j,k,
					   &mx, &my, &mz);
	    
	    general_inverse_transform_point(globals->trans_info.transformation,
					    wx,wy,wz,
					    &tx,&ty,&tz);


	    result = optimize_3D_deformation_for_single_node(steps[0], 
							     threshold1,
							     tx,ty,tz,
							     mx, my, mz,
							     &def_x, &def_y, &def_z, 
							     iters, iteration_limit,
							     &nfunks,
							     number_dimensions);
	    

	    if (result == -40.0) {
	      nodes_tried++;
	      result = 0.0;
	    } else {
	      if (ABS(result) > 0.95*steps[0])
		over++;

	      nodes_done++;

	      displace += ABS(result);
	      sum += ABS(result);
	      sum2 += ABS(result) * ABS(result);

	      if (ABS(result)>max) max = ABS(result);
	      if (ABS(result)<min) min = ABS(result);

	      nfunk_total += nfunks;

	      nfunk1 += nfunks; nodes1++;

	    }
	    
	  }
	  
	  mag = sqrt(def_x*def_x + def_y*def_y + def_z*def_z);
	  mag = CONVERT_VALUE_TO_VOXEL(additional_dx, mag); 
	  SET_VOXEL_3D(additional_mag, i,j,k, mag);
	  
	  def_x = CONVERT_VALUE_TO_VOXEL(additional_dx, def_x); 
	  SET_VOXEL_3D(additional_dx, i,j,k, def_x);
	  def_y = CONVERT_VALUE_TO_VOXEL(additional_dy, def_y); 
	  SET_VOXEL_3D(additional_dy, i,j,k, def_y);
	  def_z = CONVERT_VALUE_TO_VOXEL(additional_dz, def_z); 
	  SET_VOXEL_3D(additional_dz, i,j,k, def_z);
	  
	}

	update_progress_report( &progress, 
			       (loop_end[1]-loop_start[1])*(i-loop_start[0])+(j-loop_start[1])+1 );
      }
      timer2 = time(NULL);

      if (globals->flags.debug) 
	print ("slice: (%d : %d) = %d sec -- nodes=%d av funks %f\n",
	       i+1-loop_start[0], loop_end[0]-loop_start[0]+1, timer2-timer1, 
	       nodes1,
	       nodes1==0? 0.0:(float)nfunk1/(float)nodes1);
      
    }
    terminate_progress_report( &progress );

    if (globals->flags.debug) {
      if (nodes_done>0) {
	mean_disp_mag = displace/nodes_done;
	var = ((sum2 * nodes_done) - sum*sum) / ((float)nodes_done*(float)(nodes_done-1));
	std = sqrt(var);
	nfunks = nfunk_total / nodes_done;
      }
      else {
	mean_disp_mag=0.0; std = 0.0;
      }
      print ("Nodes seen = %d, tried = %d, done = %d, avg disp = %f +/- %f\n",
	     nodes_seen, nodes_tried, nodes_done, mean_disp_mag, std);
      print ("av nfunks = %d , over = %d, max disp = %f, min disp = %f\n", nfunks, over, max, min);

      nodes_tried = 0;
      for_less(i,0,sizes[0])
	for_less(j,0,sizes[1])
	  for_less(k,0,sizes[2]){
	    GET_VOXEL_3D(voxel, additional_mag, i,j,k); 
	    mag = CONVERT_VOXEL_TO_VALUE(additional_mag , voxel ); 
	    if (mag >= (mean_disp_mag+std))
	      nodes_tried++;
	  }
      print ("there are %d of %d over (mean+1std) disp.\n", nodes_tried, nodes_done);
  

    }


				/* update the current warp, so that the
				   next iteration will use all the data
				   calculated thus far.
				   (the result goes into additional_d* )   */

    add_additional_warp_to_current(additional_dx, additional_dy, additional_dz,
				   current->dx, current->dy, current->dz,
				   iteration_weight);

				/* smooth the warp (result into current->d*)  */

    smooth_the_warp(current->dx, current->dy, current->dz,
		    additional_dx, additional_dy, additional_dz,
		    additional_mag, 0.0);

    
/*                                 clamp the data so that the 1st derivative of
				   the deformation field does not exceed 1.0*step
				   in magnitude 

     clamp_warp_deriv(current->dx, current->dy, current->dz); 
*/

    
    if (iters<iteration_limit-1 && Gglobals->trans_info.use_simplex==TRUE) {
      for_less(i,loop_start[0],loop_end[0]) {
	for_less(j,loop_start[1],loop_end[1]) {
	  for_less(k,loop_start[2],loop_end[2]){
	    
	    def_x = def_y = def_z = 0.0;
	    
	    GET_VOXEL_3D(voxel, additional_mag, i,j,k); 
	    mag = CONVERT_VOXEL_TO_VALUE(additional_mag , voxel ); 
	    if (mag >= (mean_disp_mag+std)) {
	      
	      /* get the lattice coordinate */
	      convert_3D_voxel_to_world(current->dx, 
					(Real)i, (Real)j, (Real)k,
					&wx, &wy, &wz);
	      
	      /* get the warp to be added to the target point */
	      GET_VOXEL_3D(voxel, current->dx, i,j,k); 
	      val_x = CONVERT_VOXEL_TO_VALUE(current->dx , voxel ); 
	      GET_VOXEL_3D(voxel, current->dy, i,j,k); 
	      val_y = CONVERT_VOXEL_TO_VALUE(current->dy , voxel ); 
	      GET_VOXEL_3D(voxel, current->dz, i,j,k); 
	      val_z = CONVERT_VOXEL_TO_VALUE(current->dz , voxel ); 
	      
	      /* add the warp to the target lattice point */
	      wx += val_x; wy += val_y; wz += val_z;
	      
	      if ( point_not_masked(m2, wx, wy, wz)) {
		
		/* now get the mean warped position of the target's neighbours */
		
		get_average_warp_of_neighbours(current->dx, current->dy, current->dz, 
					       i,j,k,
					       &mx, &my, &mz);
		
		general_inverse_transform_point(globals->trans_info.transformation,
						wx,wy,wz,
						&tx,&ty,&tz);
		
		result = optimize_3D_deformation_for_single_node(steps[0], 
								 threshold1,
								 tx,ty,tz,
								 mx, my, mz,
								 &def_x, &def_y, &def_z, 
								 iters, iteration_limit,
								 &nfunks,
								 number_dimensions);
		
	      }
	    }
	      
	    def_x = CONVERT_VALUE_TO_VOXEL(additional_dx, def_x); 
	    SET_VOXEL_3D(additional_dx, i,j,k, def_x);
	    def_y = CONVERT_VALUE_TO_VOXEL(additional_dy, def_y); 
	    SET_VOXEL_3D(additional_dy, i,j,k, def_y);
	    def_z = CONVERT_VALUE_TO_VOXEL(additional_dz, def_z); 
	    SET_VOXEL_3D(additional_dz, i,j,k, def_z);
	    
	  }
	}
      }
      
      
      add_additional_warp_to_current(additional_dx, additional_dy, additional_dz,
				     current->dx, current->dy, current->dz,
				     iteration_weight);
      
      /* smooth the warp (result into current->d*)  */
      
      smooth_the_warp(current->dx, current->dy, current->dz,
		      additional_dx, additional_dy, additional_dz,
		      additional_mag, (Real)(mean_disp_mag+std));


    }


				/* reset the next iteration's warp. */


    zero = CONVERT_VALUE_TO_VOXEL(additional_dx, 0.0);
    for_less(i,0,sizes[0])
      for_less(j,0,sizes[1])
	for_less(k,0,sizes[2]){
	  SET_VOXEL_3D(additional_dx, i,j,k, zero);
	  SET_VOXEL_3D(additional_dy, i,j,k, zero);
	  SET_VOXEL_3D(additional_dz, i,j,k, zero);

	}
  

    if (globals->flags.debug && 
	globals->flags.verbose == 3)
      save_data(globals->filenames.output_trans, 
		iters+1, iteration_limit, 
		current_warp);
      
    }
    
    /* free up allocated temporary deformation volumes */
  if (globals->trans_info.use_super) {
    (void)free_volume_data(Gsuper_dx); (void)delete_volume(Gsuper_dx);
    (void)free_volume_data(Gsuper_dy); (void)delete_volume(Gsuper_dy);
    (void)free_volume_data(Gsuper_dz); (void)delete_volume(Gsuper_dz);
  }

  (void)free_volume_data(additional_dx); (void)delete_volume(additional_dx);
  (void)free_volume_data(additional_dy); (void)delete_volume(additional_dy);
  (void)free_volume_data(additional_dz); (void)delete_volume(additional_dz);
  (void)free_volume_data(additional_mag); (void)delete_volume(additional_mag);

  free_vector(Ga1xyz ,1,512);
  free_vector(Ga2xyz ,1,512);
  free_vector(TX ,1,512);
  free_vector(TY ,1,512);
  free_vector(TZ ,1,512);
  free_vector(SX ,1,512);
  free_vector(SY ,1,512);
  free_vector(SZ ,1,512);
    

  return (OK);



}





private BOOLEAN get_best_start_from_neighbours(Real threshold1, 
					       Real wx, Real wy, Real wz,
					       Real mx, Real my, Real mz,
					       Real *tx, Real *ty, Real *tz,
					       Real *d1x_dir, Real *d1y_dir, Real *d1z_dir,
					       Real *def_x, Real *def_y, Real *def_z)
     
{
  Real
    mag_normal1,
    nx, ny, nz;


				/* map point from source, forward into target space */

  general_transform_point(Gglobals->trans_info.transformation, wx,wy,wz, tx,ty,tz);

				/* average out target point with the mean position 
				   of its neightbours */

  nx = (*tx+mx)/2.0; 
  ny = (*ty+my)/2.0; 
  nz = (*tz+mz)/2.0; 
				/* what is the deformation needed to achieve this 
				   displacement */
  *def_x = nx - *tx;
  *def_y = ny - *ty;
  *def_z = nz - *tz;
  
  *tx = nx; *ty = ny; *tz = nz;
  
  mag_normal1 = get_value_of_point_in_volume(wx,wy,wz, Gd1_dxyz);

  if (mag_normal1 < threshold1)
    return(FALSE);	
  else
    return(TRUE);
  
}




/***********************************************************/
/* note that the value of the spacing coming in is FWHM/2 for the data
   used to do the correlation. */

private Real optimize_3D_deformation_for_single_node(Real spacing, 
						     Real threshold1, 
						     Real src_x, Real src_y, Real src_z,
						     Real mx, Real my, Real mz,
						     Real *def_x, Real *def_y, Real *def_z,
						     int iteration, int total_iters,
						     int *num_functions,
						     int ndim)
{
  Real
    voxel[3],
    pos[3],
    steps[3],
    simplex_size,
    result,
    tx,ty,tz, 
    xp,yp,zp, 
    d1x_dir, d1y_dir, d1z_dir;
  float 
     *y, **p;
    
    
  int 
    nfunk,len,
    numsteps,
    i,j;

  result = 0.0;			/* assume no additional deformation */
  *num_functions = 0;		/* assume no optimization done      */

  if (!get_best_start_from_neighbours(threshold1, 
				      src_x,  src_y,  src_z,
				      mx,  my,  mz,
				      &tx,  &ty,  &tz,
				      &d1x_dir, &d1y_dir, &d1z_dir,
				      def_x,  def_y,  def_z)) {

				/* then there is no gradient magnitude strong enough
				   to grab onto...*/

    result = -40.0;		/* set result to a flag value used above */

  }
  else {

    /* we now have the info needed to continue...

       src_x, src_y, src_z - point in source volume
       tx, ty, tz          - best point in target volume, so far.
       def_x,def_y,def_z   - currently contains the additional def needed to 
                             take src_x,src_y,src_z mid-way to the neighbour's mean-point
			     (which is now stored in tx,ty,tz).
                    
       */   
    
    
    numsteps = 7;

    len = numsteps+1; 
    for_less(i,1,ndim) {
      len *= (numsteps+1);
    }
				/* get the node point in the source volume taking
				/* into consideration the warp from neighbours  */

    general_inverse_transform_point(Gglobals->trans_info.transformation, 
				    tx,  ty,  tz,
				    &xp, &yp, &zp);


				/* build a lattice of points, in source volume  */
    build_source_lattice(xp, yp, zp, 
			 SX,    SY,    SZ,
			 spacing*3, spacing*3, spacing*3, /* sub-lattice 
							     diameter= 1.5*fwhm */
			 numsteps+1,  numsteps+1,  numsteps+1,
			 ndim, &Glen);

    
				/* map this lattice forward into the target space,
				   using the current transformation, in order to 
				   build a deformed lattice */
    build_target_lattice2(SX,SY,SZ,
			  TX,TY,TZ,
			  Glen);

    for_inclusive(i,1,Glen) {
      convert_3D_world_to_voxel(Gd2_dxyz, 
				(Real)TX[i],(Real)TY[i],(Real)TZ[i], 
				&pos[0], &pos[1], &pos[2]);

      if (ndim>2)		/* jiggle the points off center. */
				/* so that nearest-neighbour interpolation can be used */
	TX[i] = pos[0] -1.0 + 2.0*drand48();
      else
	TX[i] = pos[0];
      TY[i] = pos[1] -1.0 + 2.0*drand48();
      TZ[i] = pos[2] -1.0 + 2.0*drand48(); 
    }

				/* build the source lattice (without local neighbour warp),
				   that will be used in the optimization below */
    for_inclusive(i,1,Glen) {
      SX[i] += src_x - xp;
      SY[i] += src_y - yp;
      SZ[i] += src_z - zp;
    }
    
    go_get_samples_in_source(Gd1_dxyz, SX,SY,SZ, Ga1xyz, Glen, 1);

    Gsqrt_s1 = 0.0;
    for_inclusive(i,1,Glen)
      Gsqrt_s1 += Ga1xyz[i]*Ga1xyz[i];

    Gsqrt_s1 = sqrt((double)Gsqrt_s1);

				/* set up SIMPLEX OPTIMIZATION */

    nfunk = 0;
    p = matrix(1,ndim+1,1,ndim);	/* simplex */
    y = vector(1,ndim+1);        /* value of correlation at simplex vertices */
    
    
    p[1][1] = 0.0;
    p[1][2] = 0.0;
    if (ndim > 2)
      p[1][3] = 0.0;
    
    
    for (i=2; i<=(ndim+1); ++i)	/* copy initial guess to all points of simplex */
      for (j=1; j<=ndim; ++j)
	p[i][j] = p[1][j];

    get_volume_separations(Gd2_dxyz,steps);

    simplex_size = Gsimplex_size * (0.5 + 
				    0.5*((Real)(total_iters-iteration)/(Real)total_iters));

    
    p[2][1] += simplex_size;	/* set up all vertices of simplex */
    p[3][2] += simplex_size;
    if (ndim > 2) {
      p[4][3] += simplex_size;
    }
    
    for (i=1; i<=(ndim+1); ++i)	{ /* set up value of correlation at all points of simplex */
      y[i] = xcorr_fitting_function(p[i]);
    }


    if (amoeba2(p,y,ndim,ftol,xcorr_fitting_function,&nfunk)) {    /* do optimization */

      if ( y[1] < y[2] + 0.00001 )
	i=1;
      else
	i=2;
      
      if ( y[i] > y[3] + 0.00001)
	i=3;
      
      if ((ndim > 2) && ( y[i] > y[4] + 0.00001))
	i=4;

      *num_functions = nfunk;
      

      convert_3D_world_to_voxel(Gd2_dxyz, tx,ty,tz, &voxel[0], &voxel[1], &voxel[2]);

      if (ndim>2)
	convert_3D_voxel_to_world(Gd2_dxyz, 
				  (Real)(voxel[0]+p[i][3]), 
				  (Real)(voxel[1]+p[i][2]), 
				  (Real)(voxel[2]+p[i][1]),
				  &pos[0], &pos[1], &pos[2]);
      else
	convert_3D_voxel_to_world(Gd2_dxyz, 
				  (Real)(voxel[0]), 
				  (Real)(voxel[1]+p[i][2]), 
				  (Real)(voxel[2]+p[i][1]),
				  &pos[0], &pos[1], &pos[2]);


      *def_x += pos[0]-tx;
      *def_y += pos[1]-ty;
      if (ndim > 2)
	*def_z += pos[2]-tz;
      else
	*def_z = 0.0;
      
      result = sqrt((*def_x * *def_x) + (*def_y * *def_y) + (*def_z * *def_z)) ;      

/*******begin debug stuff*************

      if (result>2.0) {

	p[1][1] = p[i][1];
	p[1][2] = p[i][2];
	if (ndim > 2)
	  p[1][3] = p[i][3];
	
	print ("dx=[");
	for_inclusive(i,-4,4) {
	  p[2][1] = p[1][1];
	  p[2][2] = p[1][2];
	  if (ndim > 2)
	    p[2][3] = p[1][3];
	  
	  p[2][1] += (float)i * Gsimplex_size/4.0;
	  
	  y[1] = xcorr_fitting_function(p[2]);
	  print ("%9.7f ",y[1]);
	}
	print ("];\n");
	
	print ("dy=[");
	for_inclusive(i,-4,4) {
	  p[2][1] = p[1][1];
	  p[2][2] = p[1][2];
	  if (ndim > 2)
	    p[2][3] = p[1][3];
	  
	  p[2][2] += (float)i * Gsimplex_size/4.0;
	  
	  y[1] = xcorr_fitting_function(p[2]);
	  print ("%9.7f ",y[1]);
	}
	print ("];\n");
	
	if (ndim>2) {
	  print ("dz=[");
	  for_inclusive(i,-4,4) {
	    p[2][1] = p[1][1];
	    p[2][2] = p[1][2];
	    if (ndim > 2)
	      p[2][3] = p[1][3];
	    
	    p[2][3] += (float)i * Gsimplex_size/4.0;
	    
	    y[1] = xcorr_fitting_function(p[2]);
	    print ("%9.7f ",y[1]);
	  }
	print ("];\n");
	}
	p[1][1] = 0.0;
	p[1][2] = 0.0;
	if (ndim > 2)
	  p[1][3] = 0.0;
	
	print ("dx=[");
	for_inclusive(i,-4,4) {
	  p[2][1] = p[1][1];
	  p[2][2] = p[1][2];
	  if (ndim > 2)
	    p[2][3] = p[1][3];
	  
	  p[2][1] += (float)i * Gsimplex_size/4.0;
	  
	  y[1] = xcorr_fitting_function(p[2]);
	  print ("%9.7f ",y[1]);
	}
	print ("];\n");
	
	print ("dy=[");
	for_inclusive(i,-4,4) {
	  p[2][1] = p[1][1];
	  p[2][2] = p[1][2];
	  if (ndim > 2)
	    p[2][3] = p[1][3];
	  
	  p[2][2] += (float)i * Gsimplex_size/4.0;
	  
	  y[1] = xcorr_fitting_function(p[2]);
	  print ("%9.7f ",y[1]);
	}
	print ("];\n");
	
	if (ndim>2) {
	  print ("dz=[");
	  for_inclusive(i,-4,4) {
	    p[2][1] = p[1][1];
	    p[2][2] = p[1][2];
	    if (ndim > 2)
	      p[2][3] = p[1][3];
	    
	    p[2][3] += (float)i * Gsimplex_size/4.0;
	    
	    y[1] = xcorr_fitting_function(p[2]);
	    print ("%9.7f ",y[1]);
	  }
	print ("];\n");
	}

	print ("\n");


      }

*******end debug stuff  *************/



    }
    else {
      *num_functions = 0;
      *def_x += 0.0;
      *def_y += 0.0;
      *def_z += 0.0;     
      result = 0.0;
    }

    free_matrix(p, 1,ndim+1,1,ndim);    
    free_vector(y, 1,ndim+1);  
    

  }
  
  return(result);
}



/* Build the target lattice by transforming the source points through the
   current non-linear transformation stored in:

        Gglobals->trans_info.transformation                        

*/
public void    build_target_lattice1(float px[], float py[], float pz[],
				     float tx[], float ty[], float tz[],
				     int len)
{
  int i;
  Real x,y,z;


  for_inclusive(i,1,len) {
    general_transform_point(Gglobals->trans_info.transformation, 
			    (Real)px[i],(Real) py[i], (Real)pz[i], 
			    &x, &y, &z);
    tx[i] = (float)x;
    ty[i] = (float)y;
    tz[i] = (float)z;
  }

}

/* Build the target lattice by transforming the source points through the
   current non-linear transformation stored in:

        Glinear_transform and Gsuper_d{x,y,z} deformation volumes  

*/
public void    build_target_lattice2(float px[], float py[], float pz[],
				     float tx[], float ty[], float tz[],
				     int len)
{
  int i,sizes[3];
  Real x,y,z;
  Real vx,vy,vz;
  Real dx,dy,dz;
  long 
     ind0, ind1, ind2;

  get_volume_sizes(Gsuper_dx,sizes);

  for_inclusive(i,1,len) {
    general_transform_point(Glinear_transform,
			    (Real)px[i],(Real) py[i], (Real)pz[i], 
			    &x, &y, &z);
    convert_3D_world_to_voxel(Gsuper_dx, x,y,z, &vx, &vy, &vz);

    if ((vx >= -0.5) && (vx < sizes[0]-0.5) &&
	(vy >= -0.5) && (vy < sizes[1]-0.5) &&
	(vz >= -0.5) && (vz < sizes[2]-0.5)    ) {

      ind0 = (long) (vx+0.5);
      ind1 = (long) (vy+0.5);
      ind2 = (long) (vz+0.5);

      GET_VALUE_3D( dx ,  Gsuper_dx, ind0  , ind1  , ind2  );
      GET_VALUE_3D( dy ,  Gsuper_dy, ind0  , ind1  , ind2  );
      GET_VALUE_3D( dz ,  Gsuper_dz, ind0  , ind1  , ind2  );

      x += dx;
      y += dy;
      z += dz;
    }


    tx[i] = (float)x;
    ty[i] = (float)y;
    tz[i] = (float)z;

  }

}








private Real cost_fn(float x, float y, float z, Real max_length)
{
  Real v2,v,d;

  v2 = x*x + y*y + z*z;
  v = sqrt(v2);

  v *= v2;

  if (v<max_length)
    d = 0.2 * v / (max_length - v);
  else
    d = FLT_MAX;

  return(d);
}

private float xcorr_fitting_function(float *d)

{
   float
      similarity,cost, r;

   similarity = go_get_samples_with_offset(Gd2_dxyz, TX,TY,TZ,
					   d[1], d[2], d[3],
					   Glen, Gsqrt_s1, Ga1xyz);

   cost = (float)cost_fn(d[1], d[2], d[3], Gcost_radius);

   r = 1.0 - similarity*similarity_cost_ratio + cost*(1.0-similarity_cost_ratio);
   
   return(r);
}





