/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* marker */
#define BRUSH_C



/* dependencies */
#include <math.h>
#include "engine/qcommon/qcommon.h"
#include "common/cm/cm_polylib.h"



/* -------------------------------------------------------------------------------

   functions

   ------------------------------------------------------------------------------- */

/*
   SnapWeldVector() - ydnar
   welds two vec3_t's into a third, taking into account nearest-to-integer
   instead of averaging
 */

#define SNAP_EPSILON    0.01

static void SnapWeldVector( vec3_t a, vec3_t b, vec3_t out ){
	int i;
	vec_t ai, bi, outi;


	/* dummy check */
	if ( a == NULL || b == NULL || out == NULL ) {
		return;
	}

	/* do each element */
	for ( i = 0; i < 3; i++ )
	{
		/* round to integer */
		ai = roundf( a[ i ] );
		bi = roundf( b[ i ] );

		/* prefer exact integer */
		if ( ai == a[ i ] ) {
			out[ i ] = a[ i ];
		}
		else if ( bi == b[ i ] ) {
			out[ i ] = b[ i ];
		}

		/* use nearest */
		else if ( fabs( ai - a[ i ] ) < fabs( bi - b[ i ] ) ) {
			out[ i ] = a[ i ];
		}
		else{
			out[ i ] = b[ i ];
		}

		/* snap */
		outi = roundf( out[ i ] );
		if ( fabs( outi - out[ i ] ) <= SNAP_EPSILON ) {
			out[ i ] = outi;
		}
	}
}



/*
   FixWinding() - ydnar
   removes degenerate edges from a winding
   returns qtrue if the winding is valid
 */

bool FixWinding( winding_t *w );
bool FixWinding( winding_t *w ){
	static constexpr float degenerate_epsilon = 0.1;

	bool valid = true;
	int i, j, k;
	vec3_t vec;
	float dist;


	/* dummy check */
	if ( !w ) {
		return false;
	}

	/* check all verts */
	for ( i = 0; i < w->numpoints; i++ )
	{
		/* don't remove points if winding is a triangle */
		if ( w->numpoints == 3 ) {
			return valid;
		}

		/* get second point index */
		j = ( i + 1 ) % w->numpoints;

		/* degenerate edge? */
		VectorSubtract( w->p[ i ], w->p[ j ], vec );
		dist = VectorLength( vec );
		if ( dist < degenerate_epsilon ) {
			valid = false;
			//Sys_FPrintf( SYS_VRB, "WARNING: Degenerate winding edge found, fixing...\n" );

			/* create an average point (ydnar 2002-01-26: using nearest-integer weld preference) */
			SnapWeldVector( w->p[ i ], w->p[ j ], vec );
			VectorCopy( vec, w->p[ i ] );

			/* move the remaining verts */
			for ( k = i + 2; k < w->numpoints; k++ )
			{
				VectorCopy( w->p[ k ], w->p[ k - 1 ] );
			}
			w->numpoints--;
		}
	}

	/* one last check and return */
	if ( w->numpoints < 3 ) {
		valid = false;
	}
	return valid;

}

/*
 * Splits a winding into 4 windings
 * This assumes a 2D problem where z is special
 *
 * This could be extended someday to take a normal that would be special,
 * should we want wallwalking or something
 *
 * if this returns true, the operation succeeded, w1 to w4 are set and source has been freed.
 */
bool SplitWinding( winding_t **source, winding_t **w1, winding_t **w2, winding_t **w3, winding_t **w4 );
bool SplitWinding( winding_t **source, winding_t **w1, winding_t **w2, winding_t **w3, winding_t **w4 )
{
	static constexpr float eps = 3.0f; // how small is small enough?

	vec3_t mins = {  HUGE_QFLT,  HUGE_QFLT,  HUGE_QFLT };
	vec3_t maxs = { -HUGE_QFLT, -HUGE_QFLT, -HUGE_QFLT };
	vec3_t delta;

	// before
	if ( *source ) {
		static int i = 0;
		if ( i < 100 )
		{
			for ( int j = 0; j < (*source)->numpoints; j++ ) {
				if ( ( fabsf((*source)->p[j][0]) + fabsf((*source)->p[j][1]) + fabsf((*source)->p[j][2]) ) > 140000.0f )
				{
					i++;
					Log::Warn("Before: Large vector (%g %g %g)", (*source)->p[j][0], (*source)->p[j][1], (*source)->p[j][2]);
				}
			}
		}
	}

	/* check all verts to get the abs min and max */
	for ( int i = 0; i < (*source)->numpoints; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			if ( (*source)->p[ i ][ j ] < mins[ j ] )
			{
				mins[j] = (*source)->p[ i ][ j ];
			}
			if ( (*source)->p[ i ][ j ] > maxs[ j ] )
			{
				maxs[j] = (*source)->p[ i ][ j ];
			}
		}
	}
	if ( fabsf( mins[0] ) > 10000.0f
	  || fabsf( mins[1] ) > 10000.0f
	  || fabsf( mins[2] ) > 10000.0f
	  || fabsf( maxs[0] ) > 10000.0f
	  || fabsf( maxs[1] ) > 10000.0f
	  || fabsf( maxs[2] ) > 10000.0f )
	{
		// this is unreasonably large, skip it
		return false;
	}

	VectorSubtract( maxs, mins, delta );
	if ( fabsf( maxs[0] - mins[0] ) < eps && fabsf( maxs[1] - mins[1] ) < eps )
	{
		return false; // this is already tiny, don't do anything
	}

	float x_cut = delta[0] / 2.0f + mins[0];
	float y_cut = delta[1] / 2.0f + mins[1];
	vec3_t x_cut_plane  = { 1,  0, 0 };
	vec3_t y_cut_plane  = { 0,  1, 0 };
	vec3_t x_cut_plane2 = { -1, 0, 0 }; // pointing to the other side
	vec3_t y_cut_plane2 = { 0, -1, 0 };

	*w1 = *source;
	*w2 = CopyWinding( *source );
	*w3 = CopyWinding( *source );
	*w4 = CopyWinding( *source );
	*source = nullptr; // to avoid mistakes

	ChopWindingInPlace( w1, x_cut_plane,  x_cut, 0 );
	ChopWindingInPlace( w2, x_cut_plane2, x_cut, 0 );
	ChopWindingInPlace( w3, y_cut_plane,  y_cut, 0 );
	ChopWindingInPlace( w4, y_cut_plane2, y_cut, 0 );

	ASSERT( !*w1 || (*w1)->numpoints >= 3 );
	ASSERT( !*w2 || (*w2)->numpoints >= 3 );
	ASSERT( !*w3 || (*w3)->numpoints >= 3 );
	ASSERT( !*w4 || (*w4)->numpoints >= 3 );

	for ( winding_t *split_w : { *w1, *w2, *w3, *w4 } )
	{
		if ( !split_w )
			continue;

		static int i = 0;
		if ( i < 100 )
		{
			for ( int j = 0; j < split_w->numpoints; j++ ) {
				if ( ( fabsf(split_w->p[j][0]) + fabsf(split_w->p[j][1]) + fabsf(split_w->p[j][2]) ) > 140000.0f )
				{
					i++;
					Log::Warn("After: Large vector (%g %g %g)", split_w->p[j][0], split_w->p[j][1], split_w->p[j][2]);
				}
			}
		}
	}

	return true;
}
