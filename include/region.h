/*	SCCS Id: @(#)region.h	3.4	2004/12/29	*/
/* Copyright (c) 1996 by Jean-Christophe Collet			  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef REGION_H
#define REGION_H

/* generic callback function */

typedef boolean FDECL((*callback_proc), (genericptr_t, genericptr_t));

/*
 * player_flags
 */
#define REG_HERO_INSIDE	0x01
#define REG_NOT_HEROS	0x02
#define hero_inside(r)	((r)->player_flags & REG_HERO_INSIDE)
#define heros_fault(r)	(!((r)->player_flags & REG_NOT_HEROS))
#define set_hero_inside(r)	((r)->player_flags |= REG_HERO_INSIDE)
#define clear_hero_inside(r)	((r)->player_flags &= ~REG_HERO_INSIDE)
#define set_heros_fault(r)	((r)->player_flags &= ~REG_NOT_HEROS)
#define clear_heros_fault(r)	((r)->player_flags |= REG_NOT_HEROS)

typedef struct {
  NhRect bounding_box;		/* Bounding box of the region */
  NhRect *rects;		/* Rectangles composing the region */
  short  nrects;		/* Number of rectangles  */
  boolean attach_2_u;		/* Region attached to player ? */
  unsigned int attach_2_m;	/* Region attached to monster ? */
  /*struct obj *attach_2_o;*/	/* Region attached to object ? UNUSED YET */
  const char* enter_msg;	/* Message when entering */
  const char* leave_msg;	/* Message when leaving */
  short  ttl;			/* Time to live. -1 is forever */
  short expire_f;		/* Function to call when region's ttl expire */
  short can_enter_f;		/* Function to call to check wether the player
				   can, or can not, enter the region */
  short enter_f;		/* Function to call when the player enters*/
  short can_leave_f;		/* Function to call to check wether the player
				   can, or can not, leave the region */
  short leave_f;		/* Function to call when the player leaves */
  short inside_f;		/* Function to call every turn if player's
				   inside */
  unsigned int player_flags;	/* (see above) */
  unsigned int* monsters;	/* Monsters currently inside this region */
  short n_monst;		/* Number of monsters inside this region */
  short max_monst;		/* Maximum number of monsters that can be
				   listed without having to grow the array */
#define MONST_INC	5

  /* Should probably do the same thing about objects */

  boolean visible;		/* Is the region visible ? */
  int glyph;			/* Which glyph to use if visible */
  genericptr_t arg;		/* Optional user argument (Ex: strength of
				   force field, damage of a fire zone, ...*/
} NhRegion;

#endif /* REGION_H */
