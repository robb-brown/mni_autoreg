/* ----------------------------- MNI Header -----------------------------------
@NAME       : test_eigen.c
@INPUT      : argc, argv - command line arguments
@OUTPUT     : (none)
@RETURNS    : status
@DESCRIPTION: Program to test calculation of eigen vectors and quadratic fitting.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Mon Sep 25 08:45:43 MET 1995
@MODIFIED   : $Log: test_eigen.c,v $
@MODIFIED   : Revision 1.1  1999-10-25 19:52:11  louis
@MODIFIED   : final checkin before switch to CVS
@MODIFIED   :

@COPYRIGHT  :
              Copyright 1995 Louis Collins, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.
---------------------------------------------------------------------------- */

#ifndef lint
static char rcsid[]="$Header: /private-cvsroot/registration/mni_autoreg/minctracc/Extra_progs/test_eigen.c,v 1.1 1999-10-25 19:52:11 louis Exp $";
#endif

#include <internal_volume_io.h>

/* Constants */
#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif
#ifndef public
#  define public
#  define private static
#endif

#define VERY_SMALL_EPS 0.0001	/* this is data dependent! */

public BOOLEAN return_local_eigen(Real r[3][3][3],
				  Real dir_1[3],
				  Real dir_2[3],
				  Real dir_3[3],
				  Real val[3]);

public BOOLEAN return_local_eigen_from_hessian(Real r[3][3][3],
				  Real dir_1[3],
				  Real dir_2[3],
				  Real dir_3[3],
				  Real val[3]);

public BOOLEAN return_3D_disp_from_quad_fit(Real r[3][3][3], 
					    Real *dispu, 
					    Real *dispv, 
					    Real *dispw);	

public BOOLEAN return_3D_disp_from_min_quad_fit(Real r[3][3][3], 
						Real *dispu, 
						Real *dispv, 
						Real *dispw);	


void setup_val(Real val[3][3][3]) {
  int i,j,k;

/*
  val[0][0][0] = 0.8;  val[1][0][0] = 0.8;  val[2][0][0] = 0.8;
  val[0][1][0] = 0.8;  val[1][1][0] = 0.8;  val[2][1][0] = 0.8;
  val[0][2][0] = 0.8;  val[1][2][0] = 0.8;  val[2][2][0] = 0.8;

  val[0][0][1] = 0.90;  val[1][0][1] = 0.90;  val[2][0][1] = 0.90;
  val[0][1][1] = 0.90;  val[1][1][1] = 1.0 ;  val[2][1][1] = 0.90;
  val[0][2][1] = 0.90;  val[1][2][1] = 0.90;  val[2][2][1] = 0.90;

  val[0][0][2] = 0.8;  val[1][0][2] = 0.8;  val[2][0][2] = 0.8;
  val[0][1][2] = 0.8;  val[1][1][2] = 0.8;  val[2][1][2] = 0.8;
  val[0][2][2] = 0.8;  val[1][2][2] = 0.8;  val[2][2][2] = 0.8;
*/

  val[0][0][0] = 0.4;  val[1][0][0] = 0.38;  val[2][0][0] = 0.7;
  val[0][1][0] = 0.38;  val[1][1][0] = 0.55;  val[2][1][0] = 0.6;
  val[0][2][0] = 0.7;  val[1][2][0] = 0.6;  val[2][2][0] = 0.7;

  val[0][0][1] = 0.38;  val[1][0][1] = 0.45; val[2][0][1] = 0.6;
  val[0][1][1] = 0.45;  val[1][1][1] = 0.5;  val[2][1][1] = 0.55;
  val[0][2][1] = 0.6;  val[1][2][1] = 0.55;  val[2][2][1] = 0.6;

  val[0][0][2] = 0.7;  val[1][0][2] = 0.6;  val[2][0][2] = 0.7;
  val[0][1][2] = 0.6;  val[1][1][2] = 0.55;  val[2][1][2] = 0.6;
  val[0][2][2] = 0.7;  val[1][2][2] = 0.6;  val[2][2][2] = 0.7;


  for_inclusive(i,0,2)
    for_inclusive(j,0,2)
      for_inclusive(k,0,2) 
	val[i][j][k] = 1.0;

  for_inclusive(i,0,2) {
    val[0][0][i] *= 1.21;
    val[0][1][i] *= 1.2;
    val[0][2][i] *= 1.21;
    val[2][0][i] *= 1.1;
    val[2][1][i] *= 1.;
    val[2][2][i] *= 1.3;
  }

  val[1][1][1] = 1.0;

}

print_val(Real r[3][3][3]) 
{
  int u,v,w;

  print ("input data:\n");
  for_less(v,0,3) {
    for_less(w,0,3) {
      for_less(u,0,3)
	print ("%5.3f ",r[u][v][w]);
      print ("    ");
    } 
    print ("\n");
  }
  print ("\n");

}

public BOOLEAN return_principal_directions(Real r[3][3][3],
					   Real dir_1[3],
					   Real dir_2[3],
					   Real *r_K,
					   Real *r_S,
					   Real *r_k1,
					   Real *r_k2,
					   Real *r_norm,
					   Real *r_Lvv,
					   Real eps);


char *prog_name;

int main(int argc, char *argv[])
{

  Real 
    tmp, max_val, min_val, intensity_threshold,
    max_val_x, max_val_y, max_val_z,
    min_val_x, min_val_y, min_val_z,
    du, dv, dw,
    K, S, k1, k2, Lvv,
    dir_max[3],
    dir_mid[3],
    dir_min[3],
    dir_vals[3],
    val[3][3][3];
  
  int
    flag,i,j,k;


  prog_name = argv[0];

  if (argc!=1) {
    print("usage: %s \n", prog_name);
    exit(EXIT_FAILURE);
  }


		/* eigen value analysis on Hessian */

  print ("\n***************** Eigen value analysis on Hessian matrix\n");
  for_less(i,0,3)  {
    dir_min[i] = dir_mid[i] = dir_max[i] = dir_vals[i] = -1000000.0;
  }
  setup_val(val);
  print_val(val);
  flag = return_local_eigen_from_hessian(val, dir_min, dir_mid, dir_max, dir_vals);

  if (flag) 
    print ("flag is True (covar matrix is pos semidef)\n"); 
  else 
    print ("flag is False (covar is not pos semidef, or no eigen vals found)\n");

  print ("val:");
  for_less(i,0,3)
    print ("%12.8f ",dir_vals[i]);
  print ("\n");

  print ("vecs:\n");
  for_less(i,0,3) 
    print ("     %12.8f %12.8f %12.8f\n",dir_min[i], dir_mid[i],dir_max[i]);

  if (flag) {
    flag = return_3D_disp_from_min_quad_fit(val, &du, &dv, &dw);

    if (flag) 
      print ("flag is True (covar matrix is pos def)\n"); 
    else 
      print ("flag is False (covar is not pos def, or no eigen vals found)\n");

    print ("disp: %f %f %f\n", du, dv, dw);
  }



		/* eigen value analysis on intensities */

  print ("\n***************** Eigen value analysis on intensities\n");

  for_less(i,0,3)  {
    dir_min[i] = dir_mid[i] = dir_max[i] = dir_vals[i] = -1000000.0;
  }

  setup_val(val);
  flag = return_local_eigen(val, dir_min, dir_mid, dir_max, dir_vals);

  if (flag) print ("flag is True\n"); else print ("flag is False\n");

  for_less(i,0,3)
    print ("%12.8f ",dir_vals[i]);
  print ("\n");

  for_less(i,0,3) 
    print ("%12.8f %12.8f %12.8f\n",dir_min[i], dir_mid[i],dir_max[i]);


  
    
  print ("\n***************** Principal direction analysis\n");

  for_less(i,0,3)  {
    dir_min[i] = dir_mid[i] = dir_max[i] = dir_vals[i] = -1000000;
  }
  setup_val(val);

  flag = return_principal_directions(val,dir_mid,dir_min, &K, &S, 
				     &k1, &k2, dir_max, &Lvv, 0.0000001);
  
  dir_vals[0] = k2;
  dir_vals[1] = k1;
  dir_vals[2] = sqrt(dir_max[0]*dir_max[0] + 
		     dir_max[1]*dir_max[1] + 
		     dir_max[2]*dir_max[2]);
  
  if (dir_vals[2] > 0.0) {
    for_less(i,0,3)
      dir_max[i] /= dir_vals[2];
  }
  
  if (flag) print ("flag is True\n"); else print ("flag is False\n");
  for_less(i,0,3)
    print ("%12.8f ",dir_vals[i]);
  print ("\n");

  for_less(i,0,3) 
    print ("%12.8f %12.8f %12.8f\n",dir_min[i], dir_mid[i],dir_max[i]);


  exit(EXIT_SUCCESS);
}

