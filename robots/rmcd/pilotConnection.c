
#include "config.h"

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "log.h"
#include "obstacles.h"
#include "pilotConnection.h"

static char *state_strings[PS_MAX] = {
    "arrived",
    "pending-position",
    "refining-position",
    "refining-orientation",
    "start-wiggling",
    "wiggling",
};

extern int debug;

struct pilot_connection_data pc_data;

/**
 * Do a fuzzy comparison of two values.
 *
 * @param x1 The first value.
 * @param x2 The second value.
 * @param tol The amount of tolerance to take into account when doing the
 * comparison.
 */
#define cmp_fuzzy(x1, x2, tol) \
    ((((x1) - (tol)) < (x2)) && (x2 < ((x1) + (tol))))

#define min(x, y) ((x) < (y)) ? (x) : (y)
#define max(x, y) ((x) > (y)) ? (x) : (y)

/**
 * Compare two position objects to see if they are the "same", where "same" is
 * within some tolerance.
 *
 * @return True if they are the "same".
 */
static int cmp_pos(struct robot_position *pos1, struct robot_position *pos2)
{
    int retval = 0;

    assert(pos1 != NULL);
    assert(pos2 != NULL);

    if (cmp_fuzzy(pos1->x, pos2->x, METER_TOLERANCE) &&
	cmp_fuzzy(pos1->y, pos2->y, METER_TOLERANCE)) {
	retval = 1;
    }

    return retval;
}

static int cmp_way(struct robot_position *pos1, struct robot_position *pos2)
{
    int retval = 0;

    assert(pos1 != NULL);
    assert(pos2 != NULL);

    if (cmp_fuzzy(pos1->x, pos2->x, WAYPOINT_TOLERANCE) &&
	cmp_fuzzy(pos1->y, pos2->y, WAYPOINT_TOLERANCE)) {
	retval = 1;
    }

    return retval;
}

/**
 * Convert an absolute position to a relative position fit for grboto.dgoto().
 *
 * @param rel The object to fill out with the relative position.
 * @param abs_start The current absolute position of the robot.
 * @param abs_finish The destination position for the robot.
 */
static void conv_a2r(struct robot_position *rel,
		     struct robot_position *abs_start,
		     struct robot_position *abs_finish)
{
    float ct, st;
    
    assert(rel != NULL);
    assert(abs_start != NULL);
    assert(abs_finish != NULL);
    
    ct = cos(abs_start->theta);
    st = sin(abs_start->theta);
    
    rel->x = ct*(abs_finish->x - abs_start->x) +
             st*(-abs_finish->y - -abs_start->y);
    rel->y = ct*(-abs_finish->y - -abs_start->y) +
             st*(abs_start->x - abs_finish->x);
    
    rel->theta = abs_finish->theta - abs_start->theta;
    rel->timestamp = abs_finish->timestamp;

    if (debug > 1)
	info("a2r %f %f %f\n", rel->x, rel->y, rel->theta);
}

/**
 * Convert an relative movement to an absolute position that we can send back
 * to emcd.
 *
 * @param abs_finish The object to fill out with the final absolute position.
 * @param abs_start The absolute position of the robot before the move.
 * @param rel The relative movement of the robot.
 */
static void conv_r2a(struct robot_position *abs_finish,
		     struct robot_position *abs_start,
		     struct robot_position *rel)
{
  
    float ct, st;
    
    assert(rel != NULL);
    assert(abs_start != NULL);
    assert(abs_finish != NULL);

    rel->x = floor(rel->x * 1000.0) / 1000.0;
    rel->y = floor(rel->y * 1000.0) / 1000.0;
    
    ct = cos(abs_start->theta);
    st = sin(abs_start->theta);
    
    abs_finish->x = ct*rel->x - -st*rel->y + abs_start->x;
    abs_finish->y = ct*rel->y + -st*rel->x + abs_start->y;
    
    abs_finish->theta = abs_start->theta + rel->theta;
    abs_finish->timestamp = rel->timestamp;

    if (debug > 1) {
	info("r2a %f %f %f\n",
	     abs_finish->x,
	     abs_finish->y,
	     abs_finish->theta);
    }
}

struct pilot_connection *pc_add_robot(struct robot_config *rc)
{
    struct pilot_connection *retval;
    struct mtp_packet imp;

    assert(rc != NULL);

    retval = &pc_data.pcd_connections[pc_data.pcd_connection_count];
    pc_data.pcd_connection_count += 1;
    
    retval->pc_robot = rc;

    if (debug > 1) {
	info("debug: connecting to %s\n", rc->hostname);
    }

#if 1
    mtp_init_packet(&imp,
		    MA_Opcode, MTP_CONTROL_INIT,
		    MA_Role, MTP_ROLE_RMC,
		    MA_Message, "rmcd v0.1",
		    MA_TAG_DONE);
    if ((retval->pc_handle = mtp_create_handle2(rc->hostname,
						PILOT_SERVERPORT,
						NULL)) == NULL) {
	fatal("robot mtp_create_handle");
    }
    else if (mtp_send_packet(retval->pc_handle, &imp) != MTP_PP_SUCCESS) {
	fatal("could not send init packet");
    }
    else {
	retval->pc_flags |= PCF_CONNECTED;
    }
#endif
    
    return retval;
}

void pc_dump_info(void)
{
    int lpc;

    info("info: pilot list\n");
    for (lpc = 0; lpc < pc_data.pcd_connection_count; lpc++) {
	struct pilot_connection *pc;
	int lpc2;
	
	pc = &pc_data.pcd_connections[lpc];
	info("  %s: flags=0x%x; state=%s; tries=%d\n"
	     "    actual: %.2f %.2f %.2f\tlast:  %.2f %.2f %.2f\n"
	     "    waypt:  %.2f %.2f %.2f %s\n"
	     "    goal:   %.2f %.2f %.2f\n",
	     pc->pc_robot->hostname,
	     pc->pc_flags,
	     state_strings[pc->pc_state],
	     pc->pc_tries_remaining,
	     pc->pc_actual_pos.x, pc->pc_actual_pos.y, pc->pc_actual_pos.theta,
	     pc->pc_last_pos.x, pc->pc_last_pos.y, pc->pc_last_pos.theta,
	     pc->pc_waypoint.x, pc->pc_waypoint.y, pc->pc_waypoint.theta,
	     (pc->pc_flags & PCF_WAYPOINT) ? "(active)" : "(inactive)",
	     pc->pc_goal_pos.x, pc->pc_goal_pos.y, pc->pc_goal_pos.theta);
	for (lpc2 = 0; lpc2 < pc->pc_obstacle_count; lpc2++) {
	    struct obstacle_config *oc;

	    oc = &pc->pc_obstacles[lpc2];
	    info("   obstacle[%d] = %d -- %.2f %.2f %.2f %.2f\n",
		 lpc2,
		 oc->id,
		 oc->xmin,
		 oc->ymin,
		 oc->xmax,
		 oc->ymax);
	}
    }
}

struct pilot_connection *pc_find_pilot(int robot_id)
{
    struct pilot_connection *retval = NULL;
    int lpc;
    
    assert(robot_id >= 0);

    for (lpc = 0; (lpc < pc_data.pcd_connection_count) && !retval; lpc++) {
	struct pilot_connection *pc = &pc_data.pcd_connections[lpc];

	if (pc->pc_robot->id == robot_id) {
	    retval = pc;
	}
    }
    
    return retval;
}

void pc_plot_waypoint(struct pilot_connection *pc)
{
    struct obstacle_config *oc;
    struct rc_line rl;
    
    assert(pc != NULL);

    rl.x0 = pc->pc_actual_pos.x;
    rl.y0 = pc->pc_actual_pos.y;
    rl.x1 = pc->pc_goal_pos.x;
    rl.y1 = pc->pc_goal_pos.y;
    if ((pc->pc_obstacle_count == 0) &&
	(oc = ob_find_obstacle(pc_data.pcd_config, &rl)) != NULL) {
	info("debug: preloaded obstacle\n");
	
	pc->pc_obstacles[pc->pc_obstacle_count] = *oc;
	pc->pc_obstacle_count += 1;
    }

#if 0
    if ((pc->pc_flags & PCF_WAYPOINT) &&
	(!cmp_way(&pc->pc_actual_pos, &pc->pc_waypoint))) {
	if (debug) {
	    info("debug: %s hasn't reached waypoint yet\n",
		 pc->pc_robot->hostname);
	}
    }
    else
#endif
	if (pc->pc_obstacle_count == 0) {
	if (debug) {
	    info("debug: %s is not obstructed\n",
		 pc->pc_robot->hostname);
	}
	
	pc->pc_flags &= ~PCF_WAYPOINT;
	pc->pc_obstacle_count = 0;
	pc->pc_waypoint_tries = 0;
    }
    else {
	int lpc;

	if (debug) {
	    info("debug: plotting waypoint %d for %s\n",
		 pc->pc_waypoint_tries,
		 pc->pc_robot->hostname);
	}
	
	pc->pc_flags &= ~PCF_WAYPOINT;
	for (lpc = 0; lpc < pc->pc_obstacle_count; lpc++) {
	    struct obstacle_config *oc = &pc->pc_obstacles[lpc];
	    
	    rl.x0 = pc->pc_actual_pos.x;
	    rl.y0 = pc->pc_actual_pos.y;
	    rl.x1 = pc->pc_goal_pos.x;
	    rl.y1 = pc->pc_goal_pos.y;

	    pc->pc_tries_remaining = MAX_REFINE_RETRIES;

#if 0
	    if ((pc->pc_waypoint_tries > 0) &&
		(pc->pc_waypoint_tries % 3) == 0) {
		info("*** expanding dynamic obstacle!");
		oc->xmin -= 0.20;
		oc->ymin -= 0.20;
		oc->xmax += 0.20;
		oc->ymax += 0.20;
	    }
#endif
	    
	    if (rc_compute_code(pc->pc_goal_pos.x,
				pc->pc_goal_pos.y,
				oc) == 0) {
		if (debug) {
		    info("debug: %s has a goal in an obstacle\n",
			 pc->pc_robot->hostname);
		}

		gettimeofday(&(pc->pc_waypoint_timestamp),NULL);
		pc->pc_waypoint_tries = 21;

		pc->pc_tries_remaining = 0;
		pc->pc_obstacle_count = 0;
	    }
	    else if ((rc_clip_line(&rl, oc) == 0) ||
		     (hypotf(rl.x0 - rl.x1, rl.y0 - rl.y1) < 0.20) ) {
		     //((oc->id == -1000))) {
		     //((oc->id == -1000) && (pc->pc_waypoint_tries > 20))) {
		
		if (debug) {
		    info("debug: %s cleared obstacle\n",
			 pc->pc_robot->hostname);
		}
		
		memmove(&pc->pc_obstacles[lpc],
			&pc->pc_obstacles[lpc + 1],
			sizeof(struct obstacle_config) * (32 - lpc));
		pc->pc_obstacle_count -= 1;
		lpc -= 1;
		pc->pc_waypoint_tries = 0;
	    }
	    else if (pc->pc_waypoint_tries > 20) {
		if (debug) {
		    info("debug: %s could not clear obstacle; taking "
			 "timestamp.\n",
			 pc->pc_robot->hostname
			 );
		}
		gettimeofday(&(pc->pc_waypoint_timestamp),NULL);
	    }
	    else {
		float bearing;
		rc_code_t rc;
		int in_vision = 0;
		int compass;

		printf("clip %f %f %f %f\n",
		       rl.x0, rl.y0, rl.x1, rl.y1);
		switch ((rc = rc_compute_closest(rl.x0, rl.y0, oc))) {
		case RCF_LEFT:
		    rl.x0 = oc->xmin;
		    break;
		case RCF_TOP:
		    rl.y0 = oc->ymin;
		    break;
		case RCF_BOTTOM:
		    rl.y0 = oc->ymax;
		    break;
		case RCF_RIGHT:
		    rl.x0 = oc->xmax;
		    break;
		case RCF_TOP|RCF_LEFT:
		    rl.x0 = oc->xmin;
		    rl.y0 = oc->ymin;
		    break;
		case RCF_TOP|RCF_RIGHT:
		    rl.x0 = oc->xmax;
		    rl.y0 = oc->ymin;
		    break;
		case RCF_BOTTOM|RCF_LEFT:
		    rl.x0 = oc->xmin;
		    rl.y0 = oc->ymax;
		    break;
		case RCF_BOTTOM|RCF_RIGHT:
		    rl.x0 = oc->xmax;
		    rl.y0 = oc->ymax;
		    break;
		}

		if (rc_clip_line(&rl, oc) == 0 ||
		    (hypotf(rl.x0 - rl.x1, rl.y0 - rl.y1) < 0.05)) {
		    info("debug: %s cleared obstacle 2\n", \
			 pc->pc_robot->hostname);
		    
		    memmove(&pc->pc_obstacles[lpc],
			    &pc->pc_obstacles[lpc + 1],
			    sizeof(struct obstacle_config) * (32 - lpc));
		    pc->pc_obstacle_count -= 1;
		    lpc -= 1;
		    pc->pc_waypoint_tries = 0;
		    continue;
		}

		info("debug: clip %.2f %.2f -- %.2f %.2f == %.2f\n",
		     rl.x0, rl.y0, rl.x1, rl.y1,
		     hypotf(rl.x0 - rl.x1, rl.y0 - rl.y1));
		
		pc->pc_flags |= PCF_WAYPOINT;
		pc->pc_waypoint_tries += 1;

		bearing = atan2f(rl.y0 - rl.y1, rl.x1 - rl.x0);
		compass = mtp_compass(bearing);

		if (debug) {
		    info("debug: waypoint bearing %f\n", bearing);
		    info("debug: side=%s; compass=%s\n",
			 RC_CODE_STRING(rc),
			 MTP_COMPASS_STRING(compass));
		}

		/*
		 * we need to make sure the generated waypoint is inside
		 * the vision system bounds; if it's not, there are a couple
		 * different approaches we can take (take the n+1st approach
		 * if the nth approach failed...)
		 *
		 * 1. Flip the bearing across M_PI, and try again.
		 * 2. Stall the bot; hopefully the obstacle will clear up.
		 * 3. Back up from the obstacle, and move away from the vision
		 *    system bounds; hopefully, this will give the robot
		 *    a better chance at picking a decent waypoint.
		 * 4. ?
		 *
		 * For now, we'll try 1, then 3.  I'm not familiar enough with
		 * the state_change function to chance adding more states to
		 * control if we've tried stalling.
		 */
		while (!in_vision) {
		    int new_rc = 0, alt_rc = 0;
		    
		    switch (rc) {
		    case RCF_TOP|RCF_LEFT:
			if (compass & MCF_EAST) {
			    new_rc = RCF_TOP|RCF_RIGHT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else if (compass & MCF_SOUTH) {
			    new_rc = RCF_BOTTOM|RCF_LEFT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_BOTTOM|RCF_LEFT:
			if (compass & MCF_EAST) {
			    new_rc = RCF_BOTTOM|RCF_RIGHT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else if (compass & MCF_NORTH) {
			    new_rc = RCF_TOP|RCF_LEFT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_BOTTOM|RCF_RIGHT:
			if (compass == (MCF_NORTH|MCF_WEST)) {
			    new_rc = RCF_BOTTOM|RCF_LEFT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else if (compass & MCF_NORTH) {
			    new_rc = RCF_TOP|RCF_RIGHT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else if (compass & MCF_WEST) {
			    new_rc = RCF_BOTTOM|RCF_LEFT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_TOP|RCF_RIGHT:
			if (compass & MCF_SOUTH) {
			    new_rc = RCF_BOTTOM|RCF_RIGHT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else if (compass & MCF_WEST) {
			    new_rc = RCF_TOP|RCF_LEFT;
			    alt_rc = ~new_rc & RCF_ALL;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_LEFT:
			if (compass & MCF_EAST) {
			    new_rc = rc_closest_corner(rl.x0, rl.y0, oc);
			    alt_rc = new_rc ^ (RCF_TOP|RCF_BOTTOM);
			}
			else if (compass & MCF_NORTH) {
			    new_rc = RCF_TOP|RCF_LEFT;
			    alt_rc = RCF_BOTTOM|RCF_LEFT;
			}
			else if (compass & MCF_SOUTH) {
			    new_rc = RCF_BOTTOM|RCF_LEFT;
			    alt_rc = RCF_TOP|RCF_LEFT;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_TOP:
			if (compass & MCF_SOUTH) {
			    new_rc = rc_closest_corner(rl.x0, rl.y0, oc);
			    alt_rc = new_rc ^ (RCF_LEFT|RCF_RIGHT);
			}
			else if (compass & MCF_EAST) {
			    new_rc = RCF_TOP|RCF_RIGHT;
			    alt_rc = RCF_TOP|RCF_LEFT;
			}
			else if (compass & MCF_WEST) {
			    new_rc = RCF_TOP|RCF_LEFT;
			    alt_rc = RCF_TOP|RCF_RIGHT;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_RIGHT:
			if (compass & MCF_WEST) {
			    new_rc = rc_closest_corner(rl.x0, rl.y0, oc);
			    alt_rc = new_rc ^ (RCF_TOP|RCF_BOTTOM);
			}
			else if (compass & MCF_NORTH) {
			    new_rc = RCF_TOP|RCF_RIGHT;
			    alt_rc = RCF_BOTTOM|RCF_LEFT;
			}
			else if (compass & MCF_SOUTH) {
			    new_rc = RCF_BOTTOM|RCF_RIGHT;
			    alt_rc = RCF_TOP|RCF_LEFT;
			}
			else {
			    assert(0);
			}
			break;
		    case RCF_BOTTOM:
			if (compass & MCF_NORTH) {
			    new_rc = rc_closest_corner(rl.x0, rl.y0, oc);
			    alt_rc = new_rc ^ (RCF_LEFT|RCF_RIGHT);
			}
			else if (compass & MCF_EAST) {
			    new_rc = RCF_BOTTOM|RCF_RIGHT;
			    alt_rc = RCF_BOTTOM|RCF_LEFT;
			}
			else if (compass & MCF_WEST) {
			    new_rc = RCF_BOTTOM|RCF_LEFT;
			    alt_rc = RCF_BOTTOM|RCF_RIGHT;
			}
			else {
			    assert(0);
			}
			break;

		    default:
			assert(0);
			break;
		    }

		    if (debug) {
			info("debug: new corner -- %s\n", RC_CODE_STRING(new_rc));
		    }
		    
		    rc_corner(new_rc, &pc->pc_waypoint, oc);

		    printf(" waypoint %f %f\n",
			   pc->pc_waypoint.x,
			   pc->pc_waypoint.y);

		    if (pc_point_in_bounds(pc->pc_waypoint.x,
					   pc->pc_waypoint.y)) {
			in_vision = 1;
			info("waypoint in vision\n");
		    }
		    else {
			new_rc = alt_rc;
			info("debug: alt=%s\n",
			     RC_CODE_STRING(new_rc));
			rc_corner(new_rc, &pc->pc_waypoint, oc);
			
			info(" flipped waypoint %f %f\n",
			     pc->pc_waypoint.x,
			     pc->pc_waypoint.y);
			
			if (pc_point_in_bounds(pc->pc_waypoint.x,
					       pc->pc_waypoint.y)) {
			    in_vision = 1;
			    info("flipped waypoint in vision\n");
			}
			else {
			    /* try moving backwards, and away from vision */
			    info("could not find a waypoint!\n");

			    /* XXX: how to find current direction of movement,
			     * so we know which direction is "backwards" ?
			     */

			    pc->pc_obstacle_count = 0;
			    pc->pc_flags &= ~PCF_WAYPOINT;
			    pc->pc_waypoint_tries = 21;
			    if (debug) {
				    info("debug: %s could not clear obstacle; taking "
					 "timestamp.\n",
					 pc->pc_robot->hostname);
			    }
			    gettimeofday(&(pc->pc_waypoint_timestamp), NULL);
			    break;
			}
		    }
		}
		    
		if (debug) {
		    info("debug: %s waypoint %f %f\n",
			 pc->pc_robot->hostname,
			 pc->pc_waypoint.x,
			 pc->pc_waypoint.y);
		}

		break;
	    }
	}
    }

    /* XXX: not sure about the added condition... */
    if (pc->pc_waypoint_tries <= 20) {
	float distance, theta;

	mtp_polar(&pc->pc_actual_pos,
		  (pc->pc_flags & PCF_WAYPOINT) ?
		  &pc->pc_waypoint : &pc->pc_goal_pos,
		  &distance,
		  &theta);
	if (distance > 1.5) {
	    pc->pc_flags |= PCF_WAYPOINT;
	    mtp_cartesian(&pc->pc_actual_pos, 1.5, theta, &pc->pc_waypoint);
	}
    }
}

void pc_set_goal(struct pilot_connection *pc, struct robot_position *rp)
{
    assert(pc != NULL);
    assert(rp != NULL);
    
    if ((pc->pc_state == PS_ARRIVED) &&
	(pc->pc_flags & PCF_VISION_POSITION) &&
	cmp_pos(&pc->pc_actual_pos, rp) &&
	cmp_fuzzy(pc->pc_actual_pos.theta, rp->theta, RADIAN_TOLERANCE)) {
	struct mtp_packet rmp;
	
	info("robot %d is already there\n", pc->pc_robot->id);
	
	/* Nothing to do other than send back a complete. */
	mtp_init_packet(&rmp,
			MA_Opcode, MTP_UPDATE_POSITION,
			MA_Role, MTP_ROLE_RMC,
			MA_Position, &pc->pc_actual_pos,
			MA_RobotID, pc->pc_robot->id,
			MA_Status, MTP_POSITION_STATUS_COMPLETE,
			MA_TAG_DONE);
	if (mtp_send_packet(pc_data.pcd_emc_handle, &rmp) != MTP_PP_SUCCESS) {
	    error("cannot send reply packet\n");
	}
	
	mtp_free_packet(&rmp);
    }
    else {
	info("new goal for %s in %s\n", pc->pc_robot->hostname,
	     state_strings[pc->pc_state]);
	
	pc->pc_flags &= ~PCF_WAYPOINT;
	pc->pc_waypoint_tries = 0;
	pc->pc_goal_pos = *rp;
	if (pc->pc_state != PS_START_WIGGLING &&
	    pc->pc_state != PS_WIGGLING) {
	    pc_change_state(pc, PS_PENDING_POSITION);
	}
    }
}

void pc_set_actual(struct pilot_connection *pc, struct robot_position *rp)
{
    assert(pc != NULL);
    assert(rp != NULL);

    if (debug > 1) {
	info("debug: %s vision position %f %f %f\n",
	     pc->pc_robot->hostname, rp->x, rp->y, rp->theta);
    }
    
    pc->pc_actual_pos = *rp;
    pc->pc_flags |= PCF_VISION_POSITION;
    pc_change_state(pc, PS_REFINING_POSITION);
}

void pc_wiggle(struct pilot_connection *pc, mtp_wiggle_t mw)
{
    assert(pc != NULL);

    if (debug > 1) {
	info("debug: %s wiggle %d\n", pc->pc_robot->hostname, mw);
    }

    switch (mw) {
    case MTP_WIGGLE_START:
	pc_change_state(pc, PS_START_WIGGLING);
	break;
    case MTP_WIGGLE_180_R:
	pc_change_state(pc, PS_WIGGLING);
	break;

    default:
	assert(0);
	break;
    }
}

void pc_change_state(struct pilot_connection *pc, pilot_state_t ps)
{
    int send_mp = 0, send_pmp = 0;
    struct mtp_packet mp, pmp;
    
    assert(pc != NULL);
    assert(ps >= 0);
    assert(ps < PS_MAX);

    info(" foo len %d\n", pc_data.pcd_config->bounds.bounds_len);
    
    if (debug > 1) {
	info("debug: %s current state '%s'\n",
	     pc->pc_robot->hostname, state_strings[pc->pc_state]);
    }

    switch (ps) {
    case PS_REFINING_POSITION:
	pc->pc_flags &= ~PCF_IN_PROGRESS;
	
	if (pc->pc_state != ps) {
	    pc->pc_tries_remaining = MAX_REFINE_RETRIES;
	}
	
	if (!(pc->pc_flags & PCF_VISION_POSITION)) {
	    mtp_init_packet(&mp,
			    MA_Opcode, MTP_REQUEST_POSITION,
			    MA_Role, MTP_ROLE_RMC,
			    MA_RobotID, pc->pc_robot->id,
			    MA_TAG_DONE);
	    send_mp = 1;
	}
	else if (pc->pc_flags & PCF_CONTACT) {
	    mtp_init_packet(&pmp,
			    MA_Opcode, MTP_REQUEST_REPORT,
			    MA_Role, MTP_ROLE_RMC,
			    MA_RobotID, pc->pc_robot->id,
			    MA_TAG_DONE);
	    send_pmp = 1;
	}
	else if (pc->pc_flags & PCF_VISION_POSITION) {
	    pc_plot_waypoint(pc);

	    /* this condition true means that pc_plot_waypoint could 
	     * not return with a valid waypoint because we've exceeded
	     * the number of possible waypoint tries
	     */
	    if (!(pc->pc_flags & PCF_WAYPOINT) && 
		pc->pc_waypoint_tries > 20
		) {
		/*
		 * we need to let this guy sleep for 10s, then try him again
		 */

		/* then when the timeout expires, change state to PS_ARRIVED,
		 * and let the cycles continue...
		 */

		info("waypoint_tries exceeded for robot %d; letting him rest"
		     " for a bit...\n",
		     pc->pc_robot->id
		     );
		
	    }
	    else {
	    
		if (!(pc->pc_flags & PCF_WAYPOINT))
		    pc->pc_tries_remaining -= 1;
		
		if (!(pc->pc_flags & PCF_WAYPOINT) &&
		    ((pc->pc_tries_remaining <= 0) ||
		     (cmp_pos(&pc->pc_actual_pos, &pc->pc_goal_pos)))) {
		    struct robot_position rp;
		    
		    ps = PS_REFINING_ORIENTATION;
		    
		    memset(&rp, 0, sizeof(rp));
		    rp.theta = pc->pc_goal_pos.theta - pc->pc_actual_pos.theta;
		    mtp_init_packet(&pmp,
				    MA_Opcode, MTP_COMMAND_GOTO,
				    MA_Role, MTP_ROLE_RMC,
				    MA_Position, &rp,
				    MA_RobotID, pc->pc_robot->id,
				    MA_CommandID, 1,
				    MA_TAG_DONE);
		    send_pmp = 1;
		}
		else {
		    struct robot_position rp_rel;
		    
		    /* Not there yet, contiue refining. */
		    pc->pc_last_pos = pc->pc_actual_pos;
		    conv_a2r(&rp_rel,
			     &pc->pc_actual_pos,
			     (pc->pc_flags & PCF_WAYPOINT) ?
			     &pc->pc_waypoint :
			     &pc->pc_goal_pos);
		    mtp_init_packet(&pmp,
				    MA_Opcode, MTP_COMMAND_GOTO,
				    MA_Role, MTP_ROLE_RMC,
				    MA_RobotID, pc->pc_robot->id,
				    MA_CommandID, 1,
				    MA_Position, &rp_rel,
				    MA_TAG_DONE);
		    send_pmp = 1;
		}
	    }
	}
	break;
	
    case PS_ARRIVED:
	info ("case arrived...\n");
	mtp_init_packet(&mp,
			MA_Opcode, MTP_UPDATE_POSITION,
			MA_Role, MTP_ROLE_RMC,
			MA_Position, &pc->pc_actual_pos,
			MA_RobotID, pc->pc_robot->id,
			MA_Status, MTP_POSITION_STATUS_COMPLETE,
			MA_TAG_DONE);
	send_mp = 1;
	break;

    case PS_PENDING_POSITION:
    case PS_START_WIGGLING:
	if ((ps == PS_PENDING_POSITION) &&
	    (pc->pc_state != PS_ARRIVED) && (pc->pc_state != PS_WIGGLING)) {
	    pc->pc_flags |= PCF_IN_PROGRESS;
	}
	mtp_init_packet(&pmp,
			MA_Opcode, MTP_COMMAND_STOP,
			MA_Role, MTP_ROLE_RMC,
			MA_RobotID, pc->pc_robot->id,
			MA_CommandID, ps == PS_PENDING_POSITION ? 1 : 2,
			MA_TAG_DONE);
	send_pmp = 1;
	break;
	
    case PS_WIGGLING:
	if (pc->pc_flags & PCF_CONTACT) {
	    mtp_init_packet(&mp,
			    MA_Opcode, MTP_WIGGLE_STATUS,
			    MA_Role, MTP_ROLE_RMC,
			    MA_RobotID, pc->pc_robot->id,
			    MA_Status, MTP_POSITION_STATUS_ERROR,
			    MA_TAG_DONE);
	    send_mp = 1;

	    pc->pc_flags &= ~PCF_CONTACT;
	    pc->pc_flags |= PCF_WIGGLE_REVERSE;
	    ps = PS_ARRIVED;
	}
	else {
	    mtp_init_packet(&pmp,
			    MA_Opcode, MTP_COMMAND_GOTO,
			    MA_Role, MTP_ROLE_RMC,
			    MA_RobotID, pc->pc_robot->id,
			    MA_CommandID, 2,
			    MA_Theta, (pc->pc_flags & PCF_WIGGLE_REVERSE ?
				       -M_PI : M_PI),
			    MA_TAG_DONE);
	    send_pmp = 1;

	    pc->pc_flags &= ~PCF_WIGGLE_REVERSE;
	}
	break;

    case PS_REFINING_ORIENTATION:
	if (pc->pc_flags & PCF_CONTACT) {
	    ps = PS_ARRIVED;
	}
	else {
	    assert(0);
	}
	break;
	
    default:
	fprintf(stderr, "bad state %s\n", state_strings[ps]);
	assert(0);
	break;
    }
    
    if (debug) {
	fprintf(stderr,
		"debug: %s state change, %s -> %s\n",
		pc->pc_robot->hostname,
		state_strings[pc->pc_state],
		state_strings[ps]);
    }
    
    if (send_mp) {
	if (debug > 1) {
	    fprintf(stderr, "sending emc packet: ");
	    mtp_print_packet(stderr, &mp);
	}
	
	if (mtp_send_packet(pc_data.pcd_emc_handle, &mp) != MTP_PP_SUCCESS) {
	    fatal("emc send");
	}
    }

    if (send_pmp) {
	if (debug > 1) {
	    fprintf(stderr, "sending pilot packet: ");
	    mtp_print_packet(stderr, &pmp);
	}
	
	if (mtp_send_packet(pc->pc_handle, &pmp) != MTP_PP_SUCCESS) {
	    fatal("pilot send");
	}
    }

    pc->pc_state = ps;
}

static void pc_handle_update(struct pilot_connection *pc,
			     struct mtp_update_position *mup)
{
    assert(pc != NULL);
    assert(mup != NULL);

    if (((mup->position.x != 0.0f) ||
	 (mup->position.y != 0.0f) ||
	 (mup->position.theta != 0.0f))) {
	pc->pc_flags &= ~PCF_VISION_POSITION;
    }

    if (((pc->pc_state == PS_START_WIGGLING) ||
	 (pc->pc_state == PS_WIGGLING)) &&
	(mup->command_id == 2) &&
	((mup->status == MTP_POSITION_STATUS_IDLE) ||
	 (mup->status == MTP_POSITION_STATUS_COMPLETE))) {
	struct mtp_packet vsn;

	if (debug) {
	    info("sending %s wiggle status %d\n",
		 pc->pc_robot->hostname,
		 mup->status);
	}
	
	mtp_init_packet(&vsn,
			MA_Opcode, MTP_WIGGLE_STATUS,
			MA_Role, MTP_ROLE_RMC,
			MA_RobotID, pc->pc_robot->id,
			MA_Status, mup->status,
			MA_TAG_DONE);
	if (mtp_send_packet(pc_data.pcd_emc_handle, &vsn) != MTP_PP_SUCCESS) {
	    error("could not send wiggle-status to emc!");
	}
	
	mtp_free_packet(&vsn);
    }
    
    switch (mup->status) {
    case MTP_POSITION_STATUS_IDLE:
	/* Response to a COMMAND_STOP. */
	pc->pc_flags &= ~PCF_VISION_POSITION;
	if (mup->command_id == 1 &&
	    !(pc->pc_flags & PCF_IN_PROGRESS) &&
	    pc->pc_state == PS_PENDING_POSITION) {
	    pc_change_state(pc, PS_REFINING_POSITION);
	}
	break;
    case MTP_POSITION_STATUS_ERROR:
	/* XXX */
	{
	    struct mtp_packet ump;
	    
	    pc->pc_flags &= ~PCF_VISION_POSITION;
	    
	    info("error for %d\n", pc->pc_robot->id);
	    
	    mtp_init_packet(&ump,
			    MA_Opcode, MTP_UPDATE_POSITION,
			    MA_Role, MTP_ROLE_RMC,
			    MA_Position, &pc->pc_actual_pos,
			    MA_RobotID, pc->pc_robot->id,
			    MA_Status, MTP_POSITION_STATUS_ERROR,
			    MA_TAG_DONE);
	    if (mtp_send_packet(pc_data.pcd_emc_handle, &ump) !=
		MTP_PP_SUCCESS) {
		fatal("request id failed");
	    }
		
	    mtp_free_packet(&ump);

	    // XXX:  ALSO NEED TO SEND A MSG TO VMC if this was a wiggle!!
	    mtp_init_packet(&ump,
			    MA_Opcode,MTP_WIGGLE_STATUS,
			    MA_Role,MTP_ROLE_RMC,
			    MA_RobotID,pc->pc_robot->id,
			    MA_Status,MTP_POSITION_STATUS_ERROR,
			    MA_TAG_DONE);
	    if (mtp_send_packet(pc_data.pcd_emc_handle, &ump) !=
		MTP_PP_SUCCESS) {
		fatal("wiggle-status error msg failed");
	    }

	    mtp_free_packet(&ump);
	    pc->pc_state = PS_ARRIVED;
	}
	break;

    case MTP_POSITION_STATUS_ABORTED:
	pc->pc_flags &= ~PCF_VISION_POSITION;
	if (pc->pc_state == PS_REFINING_ORIENTATION)
	    pc_change_state(pc, PS_REFINING_POSITION);
	else if (pc->pc_state == PS_PENDING_POSITION)
	    pc_change_state(pc, PS_REFINING_POSITION);
	else
	    pc_change_state(pc, pc->pc_state);
	break;

    case MTP_POSITION_STATUS_CONTACT:
	pc->pc_flags &= ~PCF_VISION_POSITION;
	pc->pc_flags |= PCF_CONTACT;
	pc_change_state(pc, pc->pc_state);
	break;
    case MTP_POSITION_STATUS_COMPLETE:
	info("pilot finished %d\n", pc->pc_state);

	pc->pc_flags &= ~PCF_VISION_POSITION;

	switch (pc->pc_state) {
	    //	    /* XXX: is this right? */
	    //	case PS_ARRIVED:
	case PS_PENDING_POSITION:
	case PS_REFINING_POSITION:
	    if (mup->command_id == 1)
		pc_change_state(pc, PS_REFINING_POSITION);
	    break;
	case PS_REFINING_ORIENTATION:
	    if (mup->command_id == 1)
		pc_change_state(pc, PS_ARRIVED);
	    break;

	case PS_WIGGLING:
	    if (mup->command_id == 2)
		pc_change_state(pc, PS_PENDING_POSITION);
	    break;
	    
	case PS_ARRIVED:
	case PS_START_WIGGLING:
	    break;
	    
	default:
	    assert(0);
	    break;
	}
	break;
	
    default:
	error("unhandled status %d\n", mup->status);
	assert(0);
	break;
    }
}

static void pc_handle_report(struct pilot_connection *pc,
			     struct mtp_contact_report *mcr)
{
    int compass = 0, lpc;
    struct mtp_packet mp;
    
    assert(pc != NULL);
    assert(mcr != NULL);

    pc->pc_flags &= ~PCF_CONTACT;

    for (lpc = 0; lpc < mcr->count; lpc++) {
	struct obstacle_config oc_fake, *oc = NULL;
	struct contact_point cp;
	struct rc_line rl;
	float bearing;
	
	bearing = atan2f(mcr->points[lpc].y, mcr->points[lpc].x);
	compass |= mtp_compass(bearing);
	
	REL2ABS(&cp,
		pc->pc_actual_pos.theta,
		&mcr->points[lpc],
		&pc->pc_actual_pos);
	
	rl.x0 = pc->pc_actual_pos.x;
	rl.y0 = pc->pc_actual_pos.y;
	rl.x1 = cp.x;
	rl.y1 = cp.y;
	
	if (debug > 1) {
	    info("debug: obstacle check %f %f\n", cp.x, cp.y);
	}
	
	if (pc->pc_obstacle_count == 0 &&
	    (oc = ob_find_obstacle(pc_data.pcd_config, &rl)) != NULL) {
	    if (debug) {
		info("debug: found obstacle %d\n", oc->id);
	    }
	}
	else {
	    if (debug) {
		info("debug: did not find obstacle at bearing %f\n",
		     bearing);
	    }

	    oc = &oc_fake;
	    oc->id = -1000; // XXX

	    if (bearing < M_PI_4 && bearing > -M_PI_4) {
		mcr->points[lpc].x = 0.15;
		mcr->points[lpc].y = 0.0;
	    }
	    else if (bearing >= -M_PI_4 && bearing < (M_PI_2 + M_PI_4)) {
		if (mcr->points[lpc].y)
		    mcr->points[lpc].y = 0.15;
	    }
	    else if (bearing >= (M_PI_2 + M_PI_4) ||
		     bearing < -(M_PI_2 + M_PI_4)) {
		if (mcr->points[lpc].y)
		    mcr->points[lpc].y = 0.15;
	    }
	    else if (bearing < (M_PI_2 + M_PI_4) && (bearing > -M_PI_4)) {
		mcr->points[lpc].x = -0.15;
		mcr->points[lpc].y = 0.0;
	    }
	    
	    REL2ABS(&cp,
		    pc->pc_actual_pos.theta,
		    &mcr->points[lpc],
		    &pc->pc_actual_pos);

	    info("debug: new point %f %f\n",
		 cp.x, cp.y);

	    oc->xmin = cp.x - OBSTACLE_BUFFER - 0.10;
	    oc->xmax = cp.x + OBSTACLE_BUFFER + 0.10;
	    oc->ymin = cp.y - OBSTACLE_BUFFER - 0.10;
	    oc->ymax = cp.y + OBSTACLE_BUFFER + 0.10;

	}

	if (pc->pc_obstacle_count > 0) {
	    struct obstacle_config *oc_old;
	    
	    oc_old = &pc->pc_obstacles[pc->pc_obstacle_count - 1];
	    oc_old->xmin = min(oc->xmin, oc_old->xmin);
	    oc_old->ymin = min(oc->ymin, oc_old->ymin);
	    oc_old->xmax = max(oc->xmax, oc_old->xmax);
	    oc_old->ymax = max(oc->ymax, oc_old->ymax);
	    oc = NULL;
	    
	    info("debug: expanding obstacle %f %f %f %f\n",
		 oc_old->xmin,
		 oc_old->ymin,
		 oc_old->xmax,
		 oc_old->ymax);
	}
	else {
	    info("debug: obstacle %d %f %f %f %f\n",
		 oc->id, oc->xmin, oc->ymin, oc->xmax, oc->ymax);
	}
	
	if (oc != NULL) {
	    pc->pc_obstacles[pc->pc_obstacle_count] = *oc;
	    pc->pc_obstacle_count += 1;
	}
    }

    compass &= ~(MCF_NORTH|MCF_SOUTH);

    info("debug: compass %s\n", MTP_COMPASS_STRING(compass));
    switch (compass) {
    case MCF_EAST|MCF_WEST:
	info("debug: %s cannot move!\n", pc->pc_robot->hostname);
	pc->pc_waypoint_tries = 21;
	gettimeofday(&(pc->pc_waypoint_timestamp), NULL);
	break;
    case MCF_EAST:
	mtp_init_packet(&mp,
			MA_Opcode, MTP_COMMAND_GOTO,
			MA_Role, MTP_ROLE_RMC,
			MA_X, -0.1,
			MA_RobotID, pc->pc_robot->id,
			MA_CommandID, 1,
			MA_TAG_DONE);
	mtp_send_packet(pc->pc_handle, &mp);
	break;
    case MCF_WEST:
	mtp_init_packet(&mp,
			MA_Opcode, MTP_COMMAND_GOTO,
			MA_Role, MTP_ROLE_RMC,
			MA_X, 0.1,
			MA_RobotID, pc->pc_robot->id,
			MA_CommandID, 1,
			MA_TAG_DONE);
	mtp_send_packet(pc->pc_handle, &mp);
	break;
    case 0:
	pc_change_state(pc, PS_REFINING_POSITION);
	break;
    default:
	assert(0);
	break;
    }
}

void pc_handle_packet(struct pilot_connection *pc, struct mtp_packet *mp)
{
    assert(pc != NULL);
    assert(mp != NULL);
    
    if (debug > 1) {
	fprintf(stderr, "%s pilot packet: ", pc->pc_robot->hostname);
	mtp_print_packet(stderr, mp);
    }

    switch (mp->data.opcode) {
    case MTP_UPDATE_POSITION:
	pc_handle_update(pc, &mp->data.mtp_payload_u.update_position);
	break;
    case MTP_CONTACT_REPORT:
	pc_handle_report(pc, &mp->data.mtp_payload_u.contact_report);
	break;
    case MTP_TELEMETRY:
	mp->data.mtp_payload_u.telemetry.mtp_telemetry_u.garcia.robot_id =
	    pc->pc_robot->id;
	mtp_send_packet(pc_data.pcd_emc_handle, mp);
	break;
	
    default:
	fprintf(stderr, "error: unhandled pilot packet %d\n", mp->data.opcode);
	break;
    }
}

void pc_handle_signal(fd_set *rready, fd_set *wready)
{
    int lpc;
    
    assert(rready != NULL);
    // assert(wready != NULL);

    for (lpc = 0; lpc < pc_data.pcd_connection_count; lpc++) {
	struct pilot_connection *pc = &pc_data.pcd_connections[lpc];
	mtp_handle_t mh = pc->pc_handle;

	if ((mh != NULL) && FD_ISSET(mh->mh_fd, rready)) {
	    do {
		struct mtp_packet mp;

		if (mtp_receive_packet(mh, &mp) != MTP_PP_SUCCESS) {
		    pc->pc_flags &= ~PCF_CONNECTED;
		    pc->pc_flags |= PCF_CONNECTING;
		}
		else {
		    pc_handle_packet(pc, &mp);
		}
	    } while ((pc->pc_flags & PCF_CONNECTED) && (mh->mh_remaining > 0));
	}
    }
}

void pc_handle_timeout(struct timeval *current_time)
{
    int lpc;
    struct pilot_connection *pc;

    assert(current_time != NULL);

    /*
     * We go through all the pilot connections, checking to see whose 
     * pc_waypoint_tries have exceeded the threshold; then we check those
     * pilot connections' timestamps; if any have exceeded, we want to allow
     * them to continue trying to reach the waypoint.
     */
    
    for (lpc = 0; lpc < pc_data.pcd_connection_count; lpc++) {
	pc = &(pc_data.pcd_connections[lpc]);
	
	if (pc->pc_waypoint_tries > 20) {
	    info("checking timestamp for stopped robot %d\n",
		 pc->pc_robot->id
		 );
	    if ((current_time->tv_sec - pc->pc_waypoint_timestamp.tv_sec) > 
		10
		) {
		pc->pc_waypoint_tries = 0;

		pc_change_state(pc,PS_PENDING_POSITION);
		info("changed state for stopped robot %d to PS_ARRIVED\n",
		     pc->pc_robot->id
		     );
		
	    }
	}
    }
    
}

int pc_point_in_bounds(float x, float y)
{
    int i;
    struct box *boxes;
    int boxes_len;
    int retval = 0;

    boxes = pc_data.pcd_config->bounds.bounds_val;
    boxes_len = pc_data.pcd_config->bounds.bounds_len;

    printf(" len %d\n", pc_data.pcd_config->bounds.bounds_len);
    for (i = 0; i < boxes_len; ++i) {
	printf(" cmp %f %f %f %f\n",
	       boxes[i].x,
	       boxes[i].y,
	       boxes[i].x + boxes[i].width,
	       boxes[i].y + boxes[i].height);
	
	if (x >= boxes[i].x && y >= boxes[i].y &&
	    x <= (boxes[i].x + boxes[i].width) && 
	    y <= (boxes[i].y + boxes[i].height)
	    ) {
	    retval = 1;
	    break;
	}
    }

    return retval;
}

int pc_point_in_obstacle(float x, float y) {
    int i;
    struct obstacle_config *oc;
    int oc_len;
    int retval = 0;

    oc = pc_data.pcd_config->obstacles.obstacles_val;
    oc_len = pc_data.pcd_config->obstacles.obstacles_len;

    for (i = 0; i < oc_len; ++i) {
	if (x >= oc[i].xmin && y >= oc[i].ymin &&
	    x <= oc[i].xmax && y <= oc[i].ymax
	    ) {
	    retval = 1;
	    break;
	}
    }

    return retval;

}
