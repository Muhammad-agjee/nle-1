/* NetHack 3.6	do_wear.c	$NHDT-Date: 1432512763 2015/05/25 00:12:43 $  $NHDT-Branch: master $:$NHDT-Revision: 1.81 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static NEARDATA const char see_yourself[] = "see yourself";
static NEARDATA const char unknown_type[] = "Unknown type of %s (%d)";
static NEARDATA const char c_armor[] = "armor", c_suit[] = "suit",
                           c_shirt[] = "shirt", c_cloak[] = "cloak",
                           c_gloves[] = "gloves", c_boots[] = "boots",
                           c_helmet[] = "helmet", c_shield[] = "shield",
                           c_weapon[] = "weapon", c_sword[] = "sword",
                           c_axe[] = "axe", c_that_[] = "that";

static NEARDATA const long takeoff_order[] = {
    WORN_BLINDF, W_WEP,      WORN_SHIELD, WORN_GLOVES, LEFT_RING,
    RIGHT_RING,  WORN_CLOAK, WORN_HELMET, WORN_AMUL,   WORN_ARMOR,
    WORN_SHIRT,  WORN_BOOTS, W_SWAPWEP,   W_QUIVER,    0L
};

STATIC_DCL void FDECL(on_msg, (struct obj *));
STATIC_DCL void FDECL(toggle_stealth, (struct obj *, long, BOOLEAN_P));
STATIC_DCL void FDECL(toggle_displacement, (struct obj *, long, BOOLEAN_P));
STATIC_PTR int NDECL(Armor_on);
STATIC_PTR int NDECL(Boots_on);
STATIC_PTR int NDECL(Cloak_on);
STATIC_PTR int NDECL(Helmet_on);
STATIC_PTR int NDECL(Gloves_on);
STATIC_DCL void FDECL(wielding_corpse, (struct obj *, BOOLEAN_P));
STATIC_PTR int NDECL(Shield_on);
STATIC_PTR int NDECL(Shirt_on);
STATIC_DCL void NDECL(Amulet_on);
STATIC_DCL void FDECL(learnring, (struct obj *, BOOLEAN_P));
STATIC_DCL void FDECL(Ring_off_or_gone, (struct obj *, BOOLEAN_P));
STATIC_PTR int FDECL(select_off, (struct obj *));
STATIC_DCL struct obj *NDECL(do_takeoff);
STATIC_PTR int NDECL(take_off);
STATIC_DCL int FDECL(menu_remarm, (int));
STATIC_DCL void FDECL(already_wearing, (const char *));
STATIC_DCL void FDECL(already_wearing2, (const char *, const char *));

void
off_msg(otmp)
struct obj *otmp;
{
    if (flags.verbose)
        You("were wearing %s.", doname(otmp));
}

/* for items that involve no delay */
STATIC_OVL void
on_msg(otmp)
struct obj *otmp;
{
    if (flags.verbose) {
        char how[BUFSZ];
        /* call xname() before obj_is_pname(); formatting obj's name
           might set obj->dknown and that affects the pname test */
        const char *otmp_name = xname(otmp);

        how[0] = '\0';
        if (otmp->otyp == TOWEL)
            Sprintf(how, " around your %s", body_part(HEAD));
        You("are now wearing %s%s.",
            obj_is_pname(otmp) ? the(otmp_name) : an(otmp_name), how);
    }
}

/* starting equipment gets auto-worn at beginning of new game,
   and we don't want stealth or displacement feedback then */
static boolean initial_don = FALSE; /* manipulated in set_wear() */

/* putting on or taking off an item which confers stealth;
   give feedback and discover it iff stealth state is changing */
STATIC_OVL
void
toggle_stealth(obj, oldprop, on)
struct obj *obj;
long oldprop; /* prop[].extrinsic, with obj->owornmask stripped by caller */
boolean on;
{
    if (on ? initial_don : context.takeoff.cancelled_don)
        return;

    if (!oldprop &&  /* extrinsic stealth from something else */
        !HStealth && /* intrinsic stealth */
        !BStealth) { /* stealth blocked by something */
        if (obj->otyp == RIN_STEALTH)
            learnring(obj, TRUE);
        else
            makeknown(obj->otyp);

        if (on) {
            if (!is_boots(obj))
                You("move very quietly.");
            else if (Levitation || Flying)
                You("float imperceptibly.");
            else
                You("walk very quietly.");
        } else {
            You("sure are noisy.");
        }
    }
}

/* putting on or taking off an item which confers displacement;
   give feedback and discover it iff displacement state is changing *and*
   hero is able to see self (or sense monsters) */
STATIC_OVL
void
toggle_displacement(obj, oldprop, on)
struct obj *obj;
long oldprop; /* prop[].extrinsic, with obj->owornmask stripped by caller */
boolean on;
{
    if (on ? initial_don : context.takeoff.cancelled_don)
        return;

    if (!oldprop && /* extrinsic displacement from something else */
        !(u.uprops[DISPLACED].intrinsic) && /* (theoretical) */
        !(u.uprops[DISPLACED].blocked) &&   /* (also theoretical) */
        /* we don't use canseeself() here because it augments vision
           with touch, which isn't appropriate for deciding whether
           we'll notice that monsters have trouble spotting the hero */
        ((!Blind &&      /* see anything */
          !u.uswallow && /* see surroundings */
          !Invisible) || /* see self */
         /* actively sensing nearby monsters via telepathy or extended
            monster detection overrides vision considerations because
            hero also senses self in this situation */
         (Unblind_telepat || (Blind_telepat && Blind) || Detect_monsters))) {
        makeknown(obj->otyp);

        You_feel("that monsters%s have difficulty pinpointing your location.",
                 on ? "" : " no longer");
    }
}

/*
 * The Type_on() functions should be called *after* setworn().
 * The Type_off() functions call setworn() themselves.
 * [Blindf_on() is an exception and calls setworn() itself.]
 */

STATIC_PTR
int
Boots_on(VOID_ARGS)
{
    long oldprop =
        u.uprops[objects[uarmf->otyp].oc_oprop].extrinsic & ~WORN_BOOTS;

    switch (uarmf->otyp) {
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case JUMPING_BOOTS:
    case KICKING_BOOTS:
        break;
    case WATER_WALKING_BOOTS:
        if (u.uinwater)
            spoteffects(TRUE);
        /* (we don't need a lava check here since boots can't be
           put on while feet are stuck) */
        break;
    case SPEED_BOOTS:
        /* Speed boots are still better than intrinsic speed, */
        /* though not better than potion speed */
        if (!oldprop && !(HFast & TIMEOUT)) {
            makeknown(uarmf->otyp);
            You_feel("yourself speed up%s.",
                     (oldprop || HFast) ? " a bit more" : "");
        }
        break;
    case ELVEN_BOOTS:
        toggle_stealth(uarmf, oldprop, TRUE);
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, rnd(20));
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation && !BLevitation) {
            makeknown(uarmf->otyp);
            float_up();
            spoteffects(FALSE);
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    default:
        impossible(unknown_type, c_boots, uarmf->otyp);
    }
    return 0;
}

int
Boots_off(VOID_ARGS)
{
    struct obj *otmp = uarmf;
    int otyp = otmp->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic & ~WORN_BOOTS;

    context.takeoff.mask &= ~W_ARMF;
    /* For levitation, float_down() returns if Levitation, so we
     * must do a setworn() _before_ the levitation case.
     */
    setworn((struct obj *) 0, W_ARMF);
    switch (otyp) {
    case SPEED_BOOTS:
        if (!Very_fast && !context.takeoff.cancelled_don) {
            makeknown(otyp);
            You_feel("yourself slow down%s.", Fast ? " a bit" : "");
        }
        break;
    case WATER_WALKING_BOOTS:
        /* check for lava since fireproofed boots make it viable */
        if ((is_pool(u.ux, u.uy) || is_lava(u.ux, u.uy)) && !Levitation
            && !Flying && !is_clinger(youmonst.data)
            && !context.takeoff.cancelled_don &&
            /* avoid recursive call to lava_effects() */
            !iflags.in_lava_effects) {
            /* make boots known in case you survive the drowning */
            makeknown(otyp);
            spoteffects(TRUE);
        }
        break;
    case ELVEN_BOOTS:
        toggle_stealth(otmp, oldprop, FALSE);
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation && !BLevitation
            && !context.takeoff.cancelled_don) {
            (void) float_down(0L, 0L);
            makeknown(otyp);
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case JUMPING_BOOTS:
    case KICKING_BOOTS:
        break;
    default:
        impossible(unknown_type, c_boots, otyp);
    }
    context.takeoff.cancelled_don = FALSE;
    return 0;
}

STATIC_PTR int
Cloak_on(VOID_ARGS)
{
    long oldprop =
        u.uprops[objects[uarmc->otyp].oc_oprop].extrinsic & ~WORN_CLOAK;

    switch (uarmc->otyp) {
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case ROBE:
    case LEATHER_CLOAK:
        break;
    case CLOAK_OF_PROTECTION:
        makeknown(uarmc->otyp);
        break;
    case ELVEN_CLOAK:
        toggle_stealth(uarmc, oldprop, TRUE);
        break;
    case CLOAK_OF_DISPLACEMENT:
        toggle_displacement(uarmc, oldprop, TRUE);
        break;
    case MUMMY_WRAPPING:
        /* Note: it's already being worn, so we have to cheat here. */
        if ((HInvis || EInvis) && !Blind) {
            newsym(u.ux, u.uy);
            You("can %s!", See_invisible ? "no longer see through yourself"
                                         : see_yourself);
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        /* since cloak of invisibility was worn, we know mummy wrapping
           wasn't, so no need to check `oldprop' against blocked */
        if (!oldprop && !HInvis && !Blind) {
            makeknown(uarmc->otyp);
            newsym(u.ux, u.uy);
            pline("Suddenly you can%s yourself.",
                  See_invisible ? " see through" : "not see");
        }
        break;
    case OILSKIN_CLOAK:
        pline("%s very tightly.", Tobjnam(uarmc, "fit"));
        break;
    /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance |= WORN_CLOAK;
        break;
    default:
        impossible(unknown_type, c_cloak, uarmc->otyp);
    }
    return 0;
}

int
Cloak_off(VOID_ARGS)
{
    struct obj *otmp = uarmc;
    int otyp = otmp->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic & ~WORN_CLOAK;

    context.takeoff.mask &= ~W_ARMC;
    /* For mummy wrapping, taking it off first resets `Invisible'. */
    setworn((struct obj *) 0, W_ARMC);
    switch (otyp) {
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_PROTECTION:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case OILSKIN_CLOAK:
    case ROBE:
    case LEATHER_CLOAK:
        break;
    case ELVEN_CLOAK:
        toggle_stealth(otmp, oldprop, FALSE);
        break;
    case CLOAK_OF_DISPLACEMENT:
        toggle_displacement(otmp, oldprop, FALSE);
        break;
    case MUMMY_WRAPPING:
        if (Invis && !Blind) {
            newsym(u.ux, u.uy);
            You("can %s.", See_invisible ? "see through yourself"
                                         : "no longer see yourself");
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        if (!oldprop && !HInvis && !Blind) {
            makeknown(CLOAK_OF_INVISIBILITY);
            newsym(u.ux, u.uy);
            pline("Suddenly you can %s.",
                  See_invisible ? "no longer see through yourself"
                                : see_yourself);
        }
        break;
    /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance &= ~WORN_CLOAK;
        break;
    default:
        impossible(unknown_type, c_cloak, otyp);
    }
    return 0;
}

STATIC_PTR
int
Helmet_on(VOID_ARGS)
{
    switch (uarmh->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
    case HELM_OF_TELEPATHY:
        break;
    case HELM_OF_BRILLIANCE:
        adj_abon(uarmh, uarmh->spe);
        break;
    case CORNUTHAUM:
        /* people think marked wizards know what they're talking
         * about, but it takes trained arrogance to pull it off,
         * and the actual enchantment of the hat is irrelevant.
         */
        ABON(A_CHA) += (Role_if(PM_WIZARD) ? 1 : -1);
        context.botl = 1;
        makeknown(uarmh->otyp);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        /* changing alignment can toggle off active artifact
           properties, including levitation; uarmh could get
           dropped or destroyed here */
        uchangealign((u.ualign.type != A_NEUTRAL)
                         ? -u.ualign.type
                         : (uarmh->o_id % 2) ? A_CHAOTIC : A_LAWFUL,
                     1);
    /* makeknown(uarmh->otyp);   -- moved below, after xname() */
    /*FALLTHRU*/
    case DUNCE_CAP:
        if (uarmh && !uarmh->cursed) {
            if (Blind)
                pline("%s for a moment.", Tobjnam(uarmh, "vibrate"));
            else
                pline("%s %s for a moment.", Tobjnam(uarmh, "glow"),
                      hcolor(NH_BLACK));
            curse(uarmh);
        }
        context.botl = 1; /* reveal new alignment or INT & WIS */
        if (Hallucination) {
            pline("My brain hurts!"); /* Monty Python's Flying Circus */
        } else if (uarmh && uarmh->otyp == DUNCE_CAP) {
            You_feel("%s.", /* track INT change; ignore WIS */
                     ACURR(A_INT)
                             <= (ABASE(A_INT) + ABON(A_INT) + ATEMP(A_INT))
                         ? "like sitting in a corner"
                         : "giddy");
        } else {
            /* [message moved to uchangealign()] */
            makeknown(HELM_OF_OPPOSITE_ALIGNMENT);
        }
        break;
    default:
        impossible(unknown_type, c_helmet, uarmh->otyp);
    }
    return 0;
}

int
Helmet_off(VOID_ARGS)
{
    context.takeoff.mask &= ~W_ARMH;

    switch (uarmh->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
        break;
    case DUNCE_CAP:
        context.botl = 1;
        break;
    case CORNUTHAUM:
        if (!context.takeoff.cancelled_don) {
            ABON(A_CHA) += (Role_if(PM_WIZARD) ? -1 : 1);
            context.botl = 1;
        }
        break;
    case HELM_OF_TELEPATHY:
        /* need to update ability before calling see_monsters() */
        setworn((struct obj *) 0, W_ARMH);
        see_monsters();
        return 0;
    case HELM_OF_BRILLIANCE:
        if (!context.takeoff.cancelled_don)
            adj_abon(uarmh, -uarmh->spe);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        /* changing alignment can toggle off active artifact
           properties, including levitation; uarmh could get
           dropped or destroyed here */
        uchangealign(u.ualignbase[A_CURRENT], 2);
        break;
    default:
        impossible(unknown_type, c_helmet, uarmh->otyp);
    }
    setworn((struct obj *) 0, W_ARMH);
    context.takeoff.cancelled_don = FALSE;
    return 0;
}

STATIC_PTR
int
Gloves_on(VOID_ARGS)
{
    long oldprop =
        u.uprops[objects[uarmg->otyp].oc_oprop].extrinsic & ~WORN_GLOVES;

    switch (uarmg->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, rnd(20));
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(uarmg->otyp);
        context.botl = 1; /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        adj_abon(uarmg, uarmg->spe);
        break;
    default:
        impossible(unknown_type, c_gloves, uarmg->otyp);
    }
    return 0;
}

STATIC_OVL void
wielding_corpse(obj, voluntary)
struct obj *obj;
boolean voluntary; /* taking gloves off on purpose? */
{
    char kbuf[BUFSZ];

    if (!obj || obj->otyp != CORPSE)
        return;
    if (obj != uwep && (obj != uswapwep || !u.twoweap))
        return;

    if (touch_petrifies(&mons[obj->corpsenm]) && !Stone_resistance) {
        You("now wield %s in your bare %s.",
            corpse_xname(obj, (const char *) 0, CXN_ARTICLE),
            makeplural(body_part(HAND)));
        Sprintf(kbuf, "%s gloves while wielding %s",
                voluntary ? "removing" : "losing", killer_xname(obj));
        instapetrify(kbuf);
        /* life-saved; can't continue wielding cockatrice corpse though */
        remove_worn_item(obj, FALSE);
    }
}

int
Gloves_off(VOID_ARGS)
{
    long oldprop =
        u.uprops[objects[uarmg->otyp].oc_oprop].extrinsic & ~WORN_GLOVES;
    boolean on_purpose = !context.mon_moving && !uarmg->in_use;

    context.takeoff.mask &= ~W_ARMG;

    switch (uarmg->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(uarmg->otyp);
        context.botl = 1; /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        if (!context.takeoff.cancelled_don)
            adj_abon(uarmg, -uarmg->spe);
        break;
    default:
        impossible(unknown_type, c_gloves, uarmg->otyp);
    }
    setworn((struct obj *) 0, W_ARMG);
    context.takeoff.cancelled_don = FALSE;
    (void) encumber_msg(); /* immediate feedback for GoP */

    /* prevent wielding cockatrice when not wearing gloves */
    if (uwep && uwep->otyp == CORPSE)
        wielding_corpse(uwep, on_purpose);

    /* KMH -- ...or your secondary weapon when you're wielding it
       [This case can't actually happen; twoweapon mode won't
       engage if a corpse has been set up as the alternate weapon.] */
    if (u.twoweap && uswapwep && uswapwep->otyp == CORPSE)
        wielding_corpse(uswapwep, on_purpose);

    return 0;
}

STATIC_PTR int
Shield_on(VOID_ARGS)
{
    /*
        switch (uarms->otyp) {
            case SMALL_SHIELD:
            case ELVEN_SHIELD:
            case URUK_HAI_SHIELD:
            case ORCISH_SHIELD:
            case DWARVISH_ROUNDSHIELD:
            case LARGE_SHIELD:
            case SHIELD_OF_REFLECTION:
                    break;
            default: impossible(unknown_type, c_shield, uarms->otyp);
        }
    */
    return 0;
}

int
Shield_off(VOID_ARGS)
{
    context.takeoff.mask &= ~W_ARMS;
    /*
        switch (uarms->otyp) {
            case SMALL_SHIELD:
            case ELVEN_SHIELD:
            case URUK_HAI_SHIELD:
            case ORCISH_SHIELD:
            case DWARVISH_ROUNDSHIELD:
            case LARGE_SHIELD:
            case SHIELD_OF_REFLECTION:
                    break;
            default: impossible(unknown_type, c_shield, uarms->otyp);
        }
    */
    setworn((struct obj *) 0, W_ARMS);
    return 0;
}

STATIC_PTR int
Shirt_on(VOID_ARGS)
{
    /*
        switch (uarmu->otyp) {
            case HAWAIIAN_SHIRT:
            case T_SHIRT:
                    break;
            default: impossible(unknown_type, c_shirt, uarmu->otyp);
        }
    */
    return 0;
}

int
Shirt_off(VOID_ARGS)
{
    context.takeoff.mask &= ~W_ARMU;
    /*
        switch (uarmu->otyp) {
            case HAWAIIAN_SHIRT:
            case T_SHIRT:
                    break;
            default: impossible(unknown_type, c_shirt, uarmu->otyp);
        }
    */
    setworn((struct obj *) 0, W_ARMU);
    return 0;
}

/* This must be done in worn.c, because one of the possible intrinsics
 * conferred
 * is fire resistance, and we have to immediately set HFire_resistance in
 * worn.c
 * since worn.c will check it before returning.
 */
STATIC_PTR
int
Armor_on(VOID_ARGS)
{
    return 0;
}

int
Armor_off(VOID_ARGS)
{
    context.takeoff.mask &= ~W_ARM;
    setworn((struct obj *) 0, W_ARM);
    context.takeoff.cancelled_don = FALSE;
    return 0;
}

/* The gone functions differ from the off functions in that if you die from
 * taking it off and have life saving, you still die.
 */
int
Armor_gone()
{
    context.takeoff.mask &= ~W_ARM;
    setnotworn(uarm);
    context.takeoff.cancelled_don = FALSE;
    return 0;
}

STATIC_OVL void
Amulet_on()
{
    /* make sure amulet isn't wielded; can't use remove_worn_item()
       here because it has already been set worn in amulet slot */
    if (uamul == uwep)
        setuwep((struct obj *) 0);
    else if (uamul == uswapwep)
        setuswapwep((struct obj *) 0);
    else if (uamul == uquiver)
        setuqwep((struct obj *) 0);

    switch (uamul->otyp) {
    case AMULET_OF_ESP:
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_MAGICAL_BREATHING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_UNCHANGING:
        if (Slimed)
            make_slimed(0L, (char *) 0);
        break;
    case AMULET_OF_CHANGE: {
        int orig_sex = poly_gender();

        if (Unchanging)
            break;
        change_sex();
        /* Don't use same message as polymorph */
        if (orig_sex != poly_gender()) {
            makeknown(AMULET_OF_CHANGE);
            You("are suddenly very %s!",
                flags.female ? "feminine" : "masculine");
            context.botl = 1;
        } else
            /* already polymorphed into single-gender monster; only
               changed the character's base sex */
            You("don't feel like yourself.");
        pline_The("amulet disintegrates!");
        if (orig_sex == poly_gender() && uamul->dknown
            && !objects[AMULET_OF_CHANGE].oc_name_known
            && !objects[AMULET_OF_CHANGE].oc_uname)
            docall(uamul);
        useup(uamul);
        break;
    }
    case AMULET_OF_STRANGULATION:
        if (can_be_strangled(&youmonst)) {
            makeknown(AMULET_OF_STRANGULATION);
            pline("It constricts your throat!");
            Strangled = 6L;
        }
        break;
    case AMULET_OF_RESTFUL_SLEEP: {
        long newnap = (long) rnd(100), oldnap = (HSleepy & TIMEOUT);

        /* avoid clobbering FROMOUTSIDE bit, which might have
           gotten set by previously eating one of these amulets */
        if (newnap < oldnap || oldnap == 0L)
            HSleepy = (HSleepy & ~TIMEOUT) | newnap;
    } break;
    case AMULET_OF_YENDOR:
        break;
    }
}

void
Amulet_off()
{
    context.takeoff.mask &= ~W_AMUL;

    switch (uamul->otyp) {
    case AMULET_OF_ESP:
        /* need to update ability before calling see_monsters() */
        setworn((struct obj *) 0, W_AMUL);
        see_monsters();
        return;
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_CHANGE:
    case AMULET_OF_UNCHANGING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_MAGICAL_BREATHING:
        if (Underwater) {
            /* HMagical_breathing must be set off
                before calling drown() */
            setworn((struct obj *) 0, W_AMUL);
            if (!breathless(youmonst.data) && !amphibious(youmonst.data)
                && !Swimming) {
                You("suddenly inhale an unhealthy amount of water!");
                (void) drown();
            }
            return;
        }
        break;
    case AMULET_OF_STRANGULATION:
        if (Strangled) {
            if (Breathless)
                Your("%s is no longer constricted!", body_part(NECK));
            else
                You("can breathe more easily!");
            Strangled = 0L;
        }
        break;
    case AMULET_OF_RESTFUL_SLEEP:
        setworn((struct obj *) 0, W_AMUL);
        /* HSleepy = 0L; -- avoid clobbering FROMOUTSIDE bit */
        if (!ESleepy && !(HSleepy & ~TIMEOUT))
            HSleepy &= ~TIMEOUT; /* clear timeout bits */
        return;
    case AMULET_OF_YENDOR:
        break;
    }
    setworn((struct obj *) 0, W_AMUL);
    return;
}

/* handle ring discovery; comparable to learnwand() */
STATIC_OVL void
learnring(ring, observed)
struct obj *ring;
boolean observed;
{
    int ringtype = ring->otyp;

    /* if effect was observeable then we usually discover the type */
    if (observed) {
        /* if we already know the ring type which accomplishes this
           effect (assumes there is at most one type for each effect),
           mark this ring as having been seen (no need for makeknown);
           otherwise if we have seen this ring, discover its type */
        if (objects[ringtype].oc_name_known)
            ring->dknown = 1;
        else if (ring->dknown)
            makeknown(ringtype);
#if 0 /* see learnwand() */
	else
	    ring->eknown = 1;
#endif
    }

    /* make enchantment of charged ring known (might be +0) and update
       perm invent window if we've seen this ring and know its type */
    if (ring->dknown && objects[ringtype].oc_name_known) {
        if (objects[ringtype].oc_charged)
            ring->known = 1;
        update_inventory();
    }
}

void
Ring_on(obj)
register struct obj *obj;
{
    long oldprop = u.uprops[objects[obj->otyp].oc_oprop].extrinsic;
    int old_attrib, which;
    boolean observable;

    /* make sure ring isn't wielded; can't use remove_worn_item()
       here because it has already been set worn in a ring slot */
    if (obj == uwep)
        setuwep((struct obj *) 0);
    else if (obj == uswapwep)
        setuswapwep((struct obj *) 0);
    else if (obj == uquiver)
        setuqwep((struct obj *) 0);

    /* only mask out W_RING when we don't have both
       left and right rings of the same type */
    if ((oldprop & W_RING) != W_RING)
        oldprop &= ~W_RING;

    switch (obj->otyp) {
    case RIN_TELEPORTATION:
    case RIN_REGENERATION:
    case RIN_SEARCHING:
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case RIN_POISON_RESISTANCE:
    case RIN_FIRE_RESISTANCE:
    case RIN_COLD_RESISTANCE:
    case RIN_SHOCK_RESISTANCE:
    case RIN_CONFLICT:
    case RIN_TELEPORT_CONTROL:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION:
    case RIN_SUSTAIN_ABILITY:
    case MEAT_RING:
        break;
    case RIN_STEALTH:
        toggle_stealth(obj, oldprop, TRUE);
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* can now see invisible monsters */
        set_mimic_blocking(); /* do special mimic handling */
        see_monsters();

        if (Invis && !oldprop && !HSee_invisible && !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you are transparent, but there!");
            learnring(obj, TRUE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!oldprop && !HInvis && !BInvis && !Blind) {
            learnring(obj, TRUE);
            newsym(u.ux, u.uy);
            self_invis_message();
        }
        break;
    case RIN_LEVITATION:
        if (!oldprop && !HLevitation && !BLevitation) {
            float_up();
            learnring(obj, TRUE);
            spoteffects(FALSE); /* for sinks */
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case RIN_GAIN_STRENGTH:
        which = A_STR;
        goto adjust_attrib;
    case RIN_GAIN_CONSTITUTION:
        which = A_CON;
        goto adjust_attrib;
    case RIN_ADORNMENT:
        which = A_CHA;
    adjust_attrib:
        old_attrib = ACURR(which);
        ABON(which) += obj->spe;
        observable = (old_attrib != ACURR(which));
        /* if didn't change, usually means ring is +0 but might
           be because nonzero couldn't go below min or above max;
           learn +0 enchantment if attribute value is not stuck
           at a limit [and ring has been seen and its type is
           already discovered, both handled by learnring()] */
        if (observable || !extremeattr(which))
            learnring(obj, observable);
        context.botl = 1;
        break;
    case RIN_INCREASE_ACCURACY: /* KMH */
        u.uhitinc += obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.udaminc += obj->spe;
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHAN:
        rescham();
        break;
    case RIN_PROTECTION:
        /* usually learn enchantment and discover type;
           won't happen if ring is unseen or if it's +0
           and the type hasn't been discovered yet */
        observable = (obj->spe != 0);
        learnring(obj, observable);
        if (obj->spe)
            find_ac(); /* updates botl */
        break;
    }
}

STATIC_OVL void
Ring_off_or_gone(obj, gone)
register struct obj *obj;
boolean gone;
{
    long mask = (obj->owornmask & W_RING);
    int old_attrib, which;
    boolean observable;

    context.takeoff.mask &= ~mask;
    if (!(u.uprops[objects[obj->otyp].oc_oprop].extrinsic & mask))
        impossible("Strange... I didn't know you had that ring.");
    if (gone)
        setnotworn(obj);
    else
        setworn((struct obj *) 0, obj->owornmask);

    switch (obj->otyp) {
    case RIN_TELEPORTATION:
    case RIN_REGENERATION:
    case RIN_SEARCHING:
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case RIN_POISON_RESISTANCE:
    case RIN_FIRE_RESISTANCE:
    case RIN_COLD_RESISTANCE:
    case RIN_SHOCK_RESISTANCE:
    case RIN_CONFLICT:
    case RIN_TELEPORT_CONTROL:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION:
    case RIN_SUSTAIN_ABILITY:
    case MEAT_RING:
        break;
    case RIN_STEALTH:
        toggle_stealth(obj, (EStealth & ~mask), FALSE);
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* Make invisible monsters go away */
        if (!See_invisible) {
            set_mimic_blocking(); /* do special mimic handling */
            see_monsters();
        }

        if (Invisible && !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you cannot see yourself.");
            learnring(obj, TRUE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!Invis && !BInvis && !Blind) {
            newsym(u.ux, u.uy);
            Your("body seems to unfade%s.",
                 See_invisible ? " completely" : "..");
            learnring(obj, TRUE);
        }
        break;
    case RIN_LEVITATION:
        if (!BLevitation) {
            (void) float_down(0L, 0L);
            if (!Levitation)
                learnring(obj, TRUE);
        } else {
            float_vs_flight(); /* maybe toggle (BFlying & I_SPECIAL) */
        }
        break;
    case RIN_GAIN_STRENGTH:
        which = A_STR;
        goto adjust_attrib;
    case RIN_GAIN_CONSTITUTION:
        which = A_CON;
        goto adjust_attrib;
    case RIN_ADORNMENT:
        which = A_CHA;
    adjust_attrib:
        old_attrib = ACURR(which);
        ABON(which) -= obj->spe;
        observable = (old_attrib != ACURR(which));
        /* same criteria as Ring_on() */
        if (observable || !extremeattr(which))
            learnring(obj, observable);
        context.botl = 1;
        break;
    case RIN_INCREASE_ACCURACY: /* KMH */
        u.uhitinc -= obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.udaminc -= obj->spe;
        break;
    case RIN_PROTECTION:
        /* might have been put on while blind and we can now see
           or perhaps been forgotten due to amnesia */
        observable = (obj->spe != 0);
        learnring(obj, observable);
        if (obj->spe)
            find_ac(); /* updates botl */
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHAN:
        /* If you're no longer protected, let the chameleons
         * change shape again -dgk
         */
        restartcham();
        break;
    }
}

void
Ring_off(obj)
struct obj *obj;
{
    Ring_off_or_gone(obj, FALSE);
}

void
Ring_gone(obj)
struct obj *obj;
{
    Ring_off_or_gone(obj, TRUE);
}

void
Blindf_on(otmp)
register struct obj *otmp;
{
    boolean already_blind = Blind, changed = FALSE;

    /* blindfold might be wielded; release it for wearing */
    if (otmp->owornmask & (W_WEP | W_SWAPWEP | W_QUIVER))
        remove_worn_item(otmp, FALSE);
    setworn(otmp, W_TOOL);
    on_msg(otmp);

    if (Blind && !already_blind) {
        changed = TRUE;
        if (flags.verbose)
            You_cant("see any more.");
        /* set ball&chain variables before the hero goes blind */
        if (Punished)
            set_bc(0);
    } else if (already_blind && !Blind) {
        changed = TRUE;
        /* "You are now wearing the Eyes of the Overworld." */
        You("can see!");
    }
    if (changed) {
        /* blindness has just been toggled */
        if (Blind_telepat || Infravision)
            see_monsters();
        vision_full_recalc = 1; /* recalc vision limits */
        if (!Blind)
            learn_unseen_invent();
        context.botl = 1;
    }
}

void
Blindf_off(otmp)
register struct obj *otmp;
{
    boolean was_blind = Blind, changed = FALSE;

    context.takeoff.mask &= ~W_TOOL;
    setworn((struct obj *) 0, otmp->owornmask);
    off_msg(otmp);

    if (Blind) {
        if (was_blind) {
            /* "still cannot see" makes no sense when removing lenses
               since they can't have been the cause of your blindness */
            if (otmp->otyp != LENSES)
                You("still cannot see.");
        } else {
            changed = TRUE; /* !was_blind */
            /* "You were wearing the Eyes of the Overworld." */
            You_cant("see anything now!");
            /* set ball&chain variables before the hero goes blind */
            if (Punished)
                set_bc(0);
        }
    } else if (was_blind) {
        if (!gulp_blnd_check()) {
            changed = TRUE; /* !Blind */
            You("can see again.");
        }
    }
    if (changed) {
        /* blindness has just been toggled */
        if (Blind_telepat || Infravision)
            see_monsters();
        vision_full_recalc = 1; /* recalc vision limits */
        if (!Blind)
            learn_unseen_invent();
        context.botl = 1;
    }
}

/* called in moveloop()'s prologue to set side-effects of worn start-up items;
   also used by poly_obj() when a worn item gets transformed */
void
set_wear(obj)
struct obj *obj; /* if null, do all worn items; otherwise just obj itself */
{
    initial_don = !obj;

    if (!obj ? ublindf != 0 : (obj == ublindf))
        (void) Blindf_on(ublindf);
    if (!obj ? uright != 0 : (obj == uright))
        (void) Ring_on(uright);
    if (!obj ? uleft != 0 : (obj == uleft))
        (void) Ring_on(uleft);
    if (!obj ? uamul != 0 : (obj == uamul))
        (void) Amulet_on();

    if (!obj ? uarmu != 0 : (obj == uarmu))
        (void) Shirt_on();
    if (!obj ? uarm != 0 : (obj == uarm))
        (void) Armor_on();
    if (!obj ? uarmc != 0 : (obj == uarmc))
        (void) Cloak_on();
    if (!obj ? uarmf != 0 : (obj == uarmf))
        (void) Boots_on();
    if (!obj ? uarmg != 0 : (obj == uarmg))
        (void) Gloves_on();
    if (!obj ? uarmh != 0 : (obj == uarmh))
        (void) Helmet_on();
    if (!obj ? uarms != 0 : (obj == uarms))
        (void) Shield_on();

    initial_don = FALSE;
}

/* check whether the target object is currently being put on (or taken off) */
boolean donning(otmp) /* also checks for doffing */
register struct obj *otmp;
{
    /* long what = (occupation == take_off) ? context.takeoff.what : 0L; */
    long what = context.takeoff.what; /* if nonzero, occupation is implied */
    boolean result = FALSE;

    /* 'W' and 'T' set afternmv, 'A' sets context.takeoff.what */
    if (otmp == uarm)
        result = (afternmv == Armor_on || afternmv == Armor_off
                  || what == WORN_ARMOR);
    else if (otmp == uarmu)
        result = (afternmv == Shirt_on || afternmv == Shirt_off
                  || what == WORN_SHIRT);
    else if (otmp == uarmc)
        result = (afternmv == Cloak_on || afternmv == Cloak_off
                  || what == WORN_CLOAK);
    else if (otmp == uarmf)
        result = (afternmv == Boots_on || afternmv == Boots_off
                  || what == WORN_BOOTS);
    else if (otmp == uarmh)
        result = (afternmv == Helmet_on || afternmv == Helmet_off
                  || what == WORN_HELMET);
    else if (otmp == uarmg)
        result = (afternmv == Gloves_on || afternmv == Gloves_off
                  || what == WORN_GLOVES);
    else if (otmp == uarms)
        result = (afternmv == Shield_on || afternmv == Shield_off
                  || what == WORN_SHIELD);

    return result;
}

/* check whether the target object is currently being taken off,
   so that stop_donning() and steal() can vary messages */
boolean
doffing(otmp)
struct obj *otmp;
{
    long what = context.takeoff.what;
    boolean result = FALSE;

    /* 'T' (also 'W') sets afternmv, 'A' sets context.takeoff.what */
    if (otmp == uarm)
        result = (afternmv == Armor_off || what == WORN_ARMOR);
    else if (otmp == uarmu)
        result = (afternmv == Shirt_off || what == WORN_SHIRT);
    else if (otmp == uarmc)
        result = (afternmv == Cloak_off || what == WORN_CLOAK);
    else if (otmp == uarmf)
        result = (afternmv == Boots_off || what == WORN_BOOTS);
    else if (otmp == uarmh)
        result = (afternmv == Helmet_off || what == WORN_HELMET);
    else if (otmp == uarmg)
        result = (afternmv == Gloves_off || what == WORN_GLOVES);
    else if (otmp == uarms)
        result = (afternmv == Shield_off || what == WORN_SHIELD);

    return result;
}

void
cancel_don()
{
    /* the piece of armor we were donning/doffing has vanished, so stop
     * wasting time on it (and don't dereference it when donning would
     * otherwise finish)
     */
    context.takeoff.cancelled_don =
        (afternmv == Boots_on || afternmv == Helmet_on
         || afternmv == Gloves_on || afternmv == Armor_on);
    afternmv = 0;
    nomovemsg = (char *) 0;
    multi = 0;
    context.takeoff.delay = 0;
    context.takeoff.what = 0L;
}

/* called by steal() during theft from hero; interrupt donning/doffing */
int
stop_donning(stolenobj)
struct obj *stolenobj; /* no message if stolenobj is already being doffing */
{
    char buf[BUFSZ];
    struct obj *otmp;
    boolean putting_on;
    int result = 0;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if ((otmp->owornmask & W_ARMOR) && donning(otmp))
            break;
    /* at most one item will pass donning() test at any given time */
    if (!otmp)
        return 0;

    /* donning() returns True when doffing too; doffing() is more specific */
    putting_on = !doffing(otmp);
    /* cancel_don() looks at afternmv; it also serves as cancel_doff() */
    cancel_don();
    /* don't want <armor>_on() or <armor>_off() being called
       by unmul() since the on or off action isn't completing */
    afternmv = 0;
    if (putting_on || otmp != stolenobj) {
        Sprintf(buf, "You stop %s %s.",
                putting_on ? "putting on" : "taking off",
                thesimpleoname(otmp));
    } else {
        buf[0] = '\0';   /* silently stop doffing stolenobj */
        result = -multi; /* remember this before calling unmul() */
    }
    unmul(buf);
    /* while putting on, item becomes worn immediately but side-effects are
       deferred until the delay expires; when interrupted, make it unworn
       (while taking off, item stays worn until the delay expires; when
       interrupted, leave it worn) */
    if (putting_on)
        remove_worn_item(otmp, FALSE);

    return result;
}

static NEARDATA const char clothes[] = { ARMOR_CLASS, 0 };
static NEARDATA const char accessories[] = { RING_CLASS, AMULET_CLASS,
                                             TOOL_CLASS, FOOD_CLASS, 0 };

/* the 'T' command */
int
dotakeoff()
{
    register struct obj *otmp = (struct obj *) 0;
    int armorpieces = 0;

#define MOREARM(x)     \
    if (x) {           \
        armorpieces++; \
        otmp = x;      \
    }
    MOREARM(uarmh);
    MOREARM(uarms);
    MOREARM(uarmg);
    MOREARM(uarmf);
    if (uarmc) {
        armorpieces++;
        otmp = uarmc;
    } else if (uarm) {
        armorpieces++;
        otmp = uarm;
    } else if (uarmu) {
        armorpieces++;
        otmp = uarmu;
    }
    if (!armorpieces) {
        /* assert( GRAY_DRAGON_SCALES > YELLOW_DRAGON_SCALE_MAIL ); */
        if (uskin)
            pline_The("%s merged with your skin!",
                      uskin->otyp >= GRAY_DRAGON_SCALES
                          ? "dragon scales are"
                          : "dragon scale mail is");
        else
            pline("Not wearing any armor.%s",
                  (iflags.cmdassist && (uleft || uright || uamul || ublindf))
                      ? "  Use 'R' command to remove accessories."
                      : "");
        return 0;
    }
    if (armorpieces > 1 || ParanoidRemove)
        otmp = getobj(clothes, "take off");
    if (!otmp)
        return 0;
    if (!(otmp->owornmask & W_ARMOR)) {
        You("are not wearing that.");
        return 0;
    }
    /* note: the `uskin' case shouldn't be able to happen here; dragons
       can't wear any armor so will end up with `armorpieces == 0' above */
    if (otmp == uskin || ((otmp == uarm) && uarmc)
        || ((otmp == uarmu) && (uarmc || uarm))) {
        char why[BUFSZ], what[BUFSZ];

        why[0] = what[0] = '\0';
        if (otmp != uskin) {
            if (uarmc)
                Strcat(what, cloak_simple_name(uarmc));
            if ((otmp == uarmu) && uarm) {
                if (uarmc)
                    Strcat(what, " and ");
                Strcat(what, suit_simple_name(uarm));
            }
            Sprintf(why, " without taking off your %s first", what);
        }
        You_cant("take that off%s.", why);
        return 0;
    }

    reset_remarm(); /* clear context.takeoff.mask and context.takeoff.what */
    (void) select_off(otmp);
    if (!context.takeoff.mask)
        return 0;
    reset_remarm(); /* armoroff() doesn't use context.takeoff.mask */

    (void) armoroff(otmp);
    return (1);
}

/* the 'R' command */
int
doremring()
{
    register struct obj *otmp = 0;
    int Accessories = 0;

#define MOREACC(x)     \
    if (x) {           \
        Accessories++; \
        otmp = x;      \
    }
    MOREACC(uleft);
    MOREACC(uright);
    MOREACC(uamul);
    MOREACC(ublindf);

    if (!Accessories) {
        pline("Not wearing any accessories.%s",
              (iflags.cmdassist && (uarm || uarmc || uarmu || uarms || uarmh
                                    || uarmg || uarmf))
                  ? "  Use 'T' command to take off armor."
                  : "");
        return (0);
    }
    if (Accessories > 1 || ParanoidRemove)
        otmp = getobj(accessories, "remove");
    if (!otmp)
        return 0;
    if (!(otmp->owornmask & (W_RING | W_AMUL | W_TOOL))) {
        You("are not wearing that.");
        return 0;
    }

    reset_remarm(); /* clear context.takeoff.mask and context.takeoff.what */
    (void) select_off(otmp);
    if (!context.takeoff.mask)
        return 0;
    reset_remarm(); /* not used by Ring_/Amulet_/Blindf_off() */

    if (otmp == uright || otmp == uleft) {
        /* Sometimes we want to give the off_msg before removing and
         * sometimes after; for instance, "you were wearing a moonstone
         * ring (on right hand)" is desired but "you were wearing a
         * square amulet (being worn)" is not because of the redundant
         * "being worn".
         */
        off_msg(otmp);
        Ring_off(otmp);
    } else if (otmp == uamul) {
        Amulet_off();
        off_msg(otmp);
    } else if (otmp == ublindf) {
        Blindf_off(otmp); /* does its own off_msg */
    } else {
        impossible("removing strange accessory?");
    }
    return (1);
}

/* Check if something worn is cursed _and_ unremovable. */
int
cursed(otmp)
register struct obj *otmp;
{
    /* Curses, like chickens, come home to roost. */
    if ((otmp == uwep) ? welded(otmp) : (int) otmp->cursed) {
        boolean use_plural = (is_boots(otmp) || is_gloves(otmp)
                              || otmp->otyp == LENSES || otmp->quan > 1L);

        You("can't.  %s cursed.", use_plural ? "They are" : "It is");
        otmp->bknown = TRUE;
        return (1);
    }
    return (0);
}

int
armoroff(otmp)
register struct obj *otmp;
{
    register int delay = -objects[otmp->otyp].oc_delay;

    if (cursed(otmp))
        return (0);
    if (delay) {
        nomul(delay);
        multi_reason = "disrobing";
        if (is_helmet(otmp)) {
            /* ick... */
            nomovemsg = !strcmp(helm_simple_name(otmp), "hat")
                            ? "You finish taking off your hat."
                            : "You finish taking off your helmet.";
            afternmv = Helmet_off;
        } else if (is_gloves(otmp)) {
            nomovemsg = "You finish taking off your gloves.";
            afternmv = Gloves_off;
        } else if (is_boots(otmp)) {
            nomovemsg = "You finish taking off your boots.";
            afternmv = Boots_off;
        } else {
            nomovemsg = "You finish taking off your suit.";
            afternmv = Armor_off;
        }
    } else {
        /* Be warned!  We want off_msg after removing the item to
         * avoid "You were wearing ____ (being worn)."  However, an
         * item which grants fire resistance might cause some trouble
         * if removed in Hell and lifesaving puts it back on; in this
         * case the message will be printed at the wrong time (after
         * the messages saying you died and were lifesaved).  Luckily,
         * no cloak, shield, or fast-removable armor grants fire
         * resistance, so we can safely do the off_msg afterwards.
         * Rings do grant fire resistance, but for rings we want the
         * off_msg before removal anyway so there's no problem.  Take
         * care in adding armors granting fire resistance; this code
         * might need modification.
         * 3.2 (actually 3.1 even): that comment is obsolete since
         * fire resistance is not required for Gehennom so setworn()
         * doesn't force the resistance granting item to be re-worn
         * after being lifesaved anymore.
         */
        if (is_cloak(otmp))
            (void) Cloak_off();
        else if (is_shield(otmp))
            (void) Shield_off();
        else
            setworn((struct obj *) 0, otmp->owornmask & W_ARMOR);
        off_msg(otmp);
    }
    context.takeoff.mask = context.takeoff.what = 0L;
    return (1);
}

STATIC_OVL void
already_wearing(cc)
const char *cc;
{
    You("are already wearing %s%c", cc, (cc == c_that_) ? '!' : '.');
}

STATIC_OVL void
already_wearing2(cc1, cc2)
const char *cc1, *cc2;
{
    You_cant("wear %s because you're wearing %s there already.", cc1, cc2);
}

/*
 * canwearobj checks to see whether the player can wear a piece of armor
 *
 * inputs: otmp (the piece of armor)
 *         noisy (if TRUE give error messages, otherwise be quiet about it)
 * output: mask (otmp's armor type)
 */
int
canwearobj(otmp, mask, noisy)
struct obj *otmp;
long *mask;
boolean noisy;
{
    int err = 0;
    const char *which;

    which = is_cloak(otmp) ? c_cloak : is_shirt(otmp)
                                           ? c_shirt
                                           : is_suit(otmp) ? c_suit : 0;
    if (which && cantweararm(youmonst.data) &&
        /* same exception for cloaks as used in m_dowear() */
        (which != c_cloak || youmonst.data->msize != MZ_SMALL)
        && (racial_exception(&youmonst, otmp) < 1)) {
        if (noisy)
            pline_The("%s will not fit on your body.", which);
        return 0;
    } else if (otmp->owornmask & W_ARMOR) {
        if (noisy)
            already_wearing(c_that_);
        return 0;
    }

    if (welded(uwep) && bimanual(uwep) && (is_suit(otmp) || is_shirt(otmp))) {
        if (noisy)
            You("cannot do that while holding your %s.",
                is_sword(uwep) ? c_sword : c_weapon);
        return 0;
    }

    if (is_helmet(otmp)) {
        if (uarmh) {
            if (noisy)
                already_wearing(an(helm_simple_name(uarmh)));
            err++;
        } else if (Upolyd && has_horns(youmonst.data) && !is_flimsy(otmp)) {
            /* (flimsy exception matches polyself handling) */
            if (noisy)
                pline_The("%s won't fit over your horn%s.",
                          helm_simple_name(otmp),
                          plur(num_horns(youmonst.data)));
            err++;
        } else
            *mask = W_ARMH;
    } else if (is_shield(otmp)) {
        if (uarms) {
            if (noisy)
                already_wearing(an(c_shield));
            err++;
        } else if (uwep && bimanual(uwep)) {
            if (noisy)
                You("cannot wear a shield while wielding a two-handed %s.",
                    is_sword(uwep) ? c_sword : (uwep->otyp == BATTLE_AXE)
                                                   ? c_axe
                                                   : c_weapon);
            err++;
        } else if (u.twoweap) {
            if (noisy)
                You("cannot wear a shield while wielding two weapons.");
            err++;
        } else
            *mask = W_ARMS;
    } else if (is_boots(otmp)) {
        if (uarmf) {
            if (noisy)
                already_wearing(c_boots);
            err++;
        } else if (Upolyd && slithy(youmonst.data)) {
            if (noisy)
                You("have no feet..."); /* not body_part(FOOT) */
            err++;
        } else if (Upolyd && youmonst.data->mlet == S_CENTAUR) {
            /* break_armor() pushes boots off for centaurs,
               so don't let dowear() put them back on... */
            if (noisy)
                pline("You have too many hooves to wear %s.",
                      c_boots); /* makeplural(body_part(FOOT)) yields
                                   "rear hooves" which sounds odd */
            err++;
        } else if (u.utrap
                   && (u.utraptype == TT_BEARTRAP || u.utraptype == TT_INFLOOR
                       || u.utraptype == TT_LAVA
                       || u.utraptype == TT_BURIEDBALL)) {
            if (u.utraptype == TT_BEARTRAP) {
                if (noisy)
                    Your("%s is trapped!", body_part(FOOT));
            } else if (u.utraptype == TT_INFLOOR || u.utraptype == TT_LAVA) {
                if (noisy)
                    Your("%s are stuck in the %s!",
                         makeplural(body_part(FOOT)), surface(u.ux, u.uy));
            } else { /*TT_BURIEDBALL*/
                if (noisy)
                    Your("%s is attached to the buried ball!",
                         body_part(LEG));
            }
            err++;
        } else
            *mask = W_ARMF;
    } else if (is_gloves(otmp)) {
        if (uarmg) {
            if (noisy)
                already_wearing(c_gloves);
            err++;
        } else if (welded(uwep)) {
            if (noisy)
                You("cannot wear gloves over your %s.",
                    is_sword(uwep) ? c_sword : c_weapon);
            err++;
        } else
            *mask = W_ARMG;
    } else if (is_shirt(otmp)) {
        if (uarm || uarmc || uarmu) {
            if (uarmu) {
                if (noisy)
                    already_wearing(an(c_shirt));
            } else {
                if (noisy)
                    You_cant("wear that over your %s.",
                             (uarm && !uarmc) ? c_armor
                                              : cloak_simple_name(uarmc));
            }
            err++;
        } else
            *mask = W_ARMU;
    } else if (is_cloak(otmp)) {
        if (uarmc) {
            if (noisy)
                already_wearing(an(cloak_simple_name(uarmc)));
            err++;
        } else
            *mask = W_ARMC;
    } else if (is_suit(otmp)) {
        if (uarmc) {
            if (noisy)
                You("cannot wear armor over a %s.", cloak_simple_name(uarmc));
            err++;
        } else if (uarm) {
            if (noisy)
                already_wearing("some armor");
            err++;
        } else
            *mask = W_ARM;
    } else {
        /* getobj can't do this after setting its allow_all flag; that
           happens if you have armor for slots that are covered up or
           extra armor for slots that are filled */
        if (noisy)
            silly_thing("wear", otmp);
        err++;
    }
    /* Unnecessary since now only weapons and special items like pick-axes get
     * welded to your hand, not armor
        if (welded(otmp)) {
            if (!err++) {
                if (noisy) weldmsg(otmp);
            }
        }
     */
    return !err;
}

/* the 'W' command */
int
dowear()
{
    struct obj *otmp;
    int delay;
    long mask = 0;

    /* cantweararm checks for suits of armor */
    /* verysmall or nohands checks for shields, gloves, etc... */
    if ((verysmall(youmonst.data) || nohands(youmonst.data))) {
        pline("Don't even bother.");
        return (0);
    }

    otmp = getobj(clothes, "wear");
    if (!otmp)
        return (0);

    if (!canwearobj(otmp, &mask, TRUE))
        return (0);

    if (!retouch_object(&otmp, FALSE))
        return 1; /* costs a turn even though it didn't get worn */

    if (otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT
        && qstart_level.dnum == u.uz.dnum) { /* in quest */
        if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
            You("narrowly avoid losing all chance at your goal.");
        else /* converted */
            You("are suddenly overcome with shame and change your mind.");
        u.ublessed = 0; /* lose your god's protection */
        makeknown(otmp->otyp);
        context.botl = 1;
        return 1;
    }

    otmp->known = 1; /* since AC is shown on the status line */
    /* if the armor is wielded, release it for wearing */
    if (otmp->owornmask & (W_WEP | W_SWAPWEP | W_QUIVER))
        remove_worn_item(otmp, FALSE);
    setworn(otmp, mask);
    delay = -objects[otmp->otyp].oc_delay;
    if (delay) {
        nomul(delay);
        multi_reason = "dressing up";
        if (is_boots(otmp))
            afternmv = Boots_on;
        if (is_helmet(otmp))
            afternmv = Helmet_on;
        if (is_gloves(otmp))
            afternmv = Gloves_on;
        if (otmp == uarm)
            afternmv = Armor_on;
        nomovemsg = "You finish your dressing maneuver.";
    } else {
        if (is_cloak(otmp))
            (void) Cloak_on();
        if (is_shield(otmp))
            (void) Shield_on();
        if (is_shirt(otmp))
            (void) Shirt_on();
        on_msg(otmp);
    }
    context.takeoff.mask = context.takeoff.what = 0L;
    return (1);
}

int
doputon()
{
    struct obj *otmp;
    long mask = 0L;

    if (uleft && uright && uamul && ublindf) {
        Your("%s%s are full, and you're already wearing an amulet and %s.",
             humanoid(youmonst.data) ? "ring-" : "",
             makeplural(body_part(FINGER)),
             ublindf->otyp == LENSES ? "some lenses" : "a blindfold");
        return (0);
    }
    otmp = getobj(accessories, "put on");
    if (!otmp)
        return (0);
    if (otmp->owornmask & (W_RING | W_AMUL | W_TOOL)) {
        already_wearing(c_that_);
        return (0);
    }
    if (welded(otmp)) {
        weldmsg(otmp);
        return (0);
    }
#if 0
/* defer til Xxx_on(); various failures below now leave item wielded
 */
	/* accessory might be wielded; release it for wearing */
	if (otmp->owornmask & (W_WEP|W_SWAPWEP|W_QUIVER))
		remove_worn_item(otmp, FALSE);
#endif

    if (otmp->oclass == RING_CLASS || otmp->otyp == MEAT_RING) {
        if (nolimbs(youmonst.data)) {
            You("cannot make the ring stick to your body.");
            return (0);
        }
        if (uleft && uright) {
            There("are no more %s%s to fill.",
                  humanoid(youmonst.data) ? "ring-" : "",
                  makeplural(body_part(FINGER)));
            return (0);
        }
        if (uleft)
            mask = RIGHT_RING;
        else if (uright)
            mask = LEFT_RING;
        else
            do {
                char qbuf[QBUFSZ];
                char answer;

                Sprintf(qbuf, "Which %s%s, Right or Left?",
                        humanoid(youmonst.data) ? "ring-" : "",
                        body_part(FINGER));
                if (!(answer = yn_function(qbuf, "rl", '\0')))
                    return (0);
                switch (answer) {
                case 'l':
                case 'L':
                    mask = LEFT_RING;
                    break;
                case 'r':
                case 'R':
                    mask = RIGHT_RING;
                    break;
                }
            } while (!mask);
        if (uarmg && uarmg->cursed) {
            uarmg->bknown = TRUE;
            You("cannot remove your gloves to put on the ring.");
            return (0);
        }
        if (welded(uwep) && bimanual(uwep)) {
            /* welded will set bknown */
            You("cannot free your weapon hands to put on the ring.");
            return (0);
        }
        if (welded(uwep) && mask == RIGHT_RING) {
            /* welded will set bknown */
            You("cannot free your weapon hand to put on the ring.");
            return (0);
        }
        if (!retouch_object(&otmp, FALSE))
            return 1; /* costs a turn even though it didn't get worn */
        setworn(otmp, mask);
        Ring_on(otmp);
    } else if (otmp->oclass == AMULET_CLASS) {
        if (uamul) {
            already_wearing("an amulet");
            return (0);
        }
        if (!retouch_object(&otmp, FALSE))
            return 1;
        setworn(otmp, W_AMUL);
        Amulet_on();
        /* finished now if an amulet of change got used up */
        if (!uamul)
            return 1;
    } else { /* it's a blindfold, towel, or lenses */
        if (ublindf) {
            if (ublindf->otyp == TOWEL)
                Your("%s is already covered by a towel.", body_part(FACE));
            else if (ublindf->otyp == BLINDFOLD) {
                if (otmp->otyp == LENSES)
                    already_wearing2("lenses", "a blindfold");
                else
                    already_wearing("a blindfold");
            } else if (ublindf->otyp == LENSES) {
                if (otmp->otyp == BLINDFOLD)
                    already_wearing2("a blindfold", "some lenses");
                else
                    already_wearing("some lenses");
            } else
                already_wearing(something); /* ??? */
            return (0);
        }
        if (otmp->otyp != BLINDFOLD && otmp->otyp != TOWEL
            && otmp->otyp != LENSES) {
            You_cant("wear that!");
            return (0);
        }
        if (!retouch_object(&otmp, FALSE))
            return 1;
        Blindf_on(otmp);
        return (1);
    }
    if (is_worn(otmp))
        prinv((char *) 0, otmp, 0L);
    return (1);
}

void
find_ac()
{
    int uac = mons[u.umonnum].ac;

    if (uarm)
        uac -= ARM_BONUS(uarm);
    if (uarmc)
        uac -= ARM_BONUS(uarmc);
    if (uarmh)
        uac -= ARM_BONUS(uarmh);
    if (uarmf)
        uac -= ARM_BONUS(uarmf);
    if (uarms)
        uac -= ARM_BONUS(uarms);
    if (uarmg)
        uac -= ARM_BONUS(uarmg);
    if (uarmu)
        uac -= ARM_BONUS(uarmu);
    if (uleft && uleft->otyp == RIN_PROTECTION)
        uac -= uleft->spe;
    if (uright && uright->otyp == RIN_PROTECTION)
        uac -= uright->spe;
    if (HProtection & INTRINSIC)
        uac -= u.ublessed;
    uac -= u.uspellprot;
    if (uac < -128)
        uac = -128; /* u.uac is an schar */
    if (uac != u.uac) {
        u.uac = uac;
        context.botl = 1;
    }
}

void
glibr()
{
    register struct obj *otmp;
    int xfl = 0;
    boolean leftfall, rightfall, wastwoweap = FALSE;
    const char *otherwep = 0, *thiswep, *which, *hand;

    leftfall = (uleft && !uleft->cursed
                && (!uwep || !welded(uwep) || !bimanual(uwep)));
    rightfall = (uright && !uright->cursed && (!welded(uwep)));
    if (!uarmg && (leftfall || rightfall) && !nolimbs(youmonst.data)) {
        /* changed so cursed rings don't fall off, GAN 10/30/86 */
        Your("%s off your %s.",
             (leftfall && rightfall) ? "rings slip" : "ring slips",
             (leftfall && rightfall) ? makeplural(body_part(FINGER))
                                     : body_part(FINGER));
        xfl++;
        if (leftfall) {
            otmp = uleft;
            Ring_off(uleft);
            dropx(otmp);
        }
        if (rightfall) {
            otmp = uright;
            Ring_off(uright);
            dropx(otmp);
        }
    }

    otmp = uswapwep;
    if (u.twoweap && otmp) {
        /* secondary weapon doesn't need nearly as much handling as
           primary; when in two-weapon mode, we know it's one-handed
           with something else in the other hand and also that it's
           a weapon or weptool rather than something unusual, plus
           we don't need to compare its type with the primary */
        otherwep = is_sword(otmp) ? c_sword : weapon_descr(otmp);
        if (otmp->quan > 1L)
            otherwep = makeplural(otherwep);
        hand = body_part(HAND);
        which = "left ";
        Your("%s %s%s from your %s%s.", otherwep, xfl ? "also " : "",
             otense(otmp, "slip"), which, hand);
        xfl++;
        wastwoweap = TRUE;
        setuswapwep((struct obj *) 0); /* clears u.twoweap */
        if (canletgo(otmp, ""))
            dropx(otmp);
    }
    otmp = uwep;
    if (otmp && !welded(otmp)) {
        long savequan = otmp->quan;

        /* nice wording if both weapons are the same type */
        thiswep = is_sword(otmp) ? c_sword : weapon_descr(otmp);
        if (otherwep && strcmp(thiswep, makesingular(otherwep)))
            otherwep = 0;
        if (otmp->quan > 1L) {
            /* most class names for unconventional wielded items
               are ok, but if wielding multiple apples or rations
               we don't want "your foods slip", so force non-corpse
               food to be singular; skipping makeplural() isn't
               enough--we need to fool otense() too */
            if (!strcmp(thiswep, "food"))
                otmp->quan = 1L;
            else
                thiswep = makeplural(thiswep);
        }
        hand = body_part(HAND);
        which = "";
        if (bimanual(otmp))
            hand = makeplural(hand);
        else if (wastwoweap)
            which = "right "; /* preceding msg was about left */
        pline("%s %s%s %s%s from your %s%s.",
              !strncmp(thiswep, "corpse", 6) ? "The" : "Your",
              otherwep ? "other " : "", thiswep, xfl ? "also " : "",
              otense(otmp, "slip"), which, hand);
        /* xfl++; */
        otmp->quan = savequan;
        setuwep((struct obj *) 0);
        if (canletgo(otmp, ""))
            dropx(otmp);
    }
}

struct obj *
some_armor(victim)
struct monst *victim;
{
    register struct obj *otmph, *otmp;

    otmph = (victim == &youmonst) ? uarmc : which_armor(victim, W_ARMC);
    if (!otmph)
        otmph = (victim == &youmonst) ? uarm : which_armor(victim, W_ARM);
    if (!otmph)
        otmph = (victim == &youmonst) ? uarmu : which_armor(victim, W_ARMU);

    otmp = (victim == &youmonst) ? uarmh : which_armor(victim, W_ARMH);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarmg : which_armor(victim, W_ARMG);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarmf : which_armor(victim, W_ARMF);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarms : which_armor(victim, W_ARMS);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    return (otmph);
}

/* used for praying to check and fix levitation trouble */
struct obj *
stuck_ring(ring, otyp)
struct obj *ring;
int otyp;
{
    if (ring != uleft && ring != uright) {
        impossible("stuck_ring: neither left nor right?");
        return (struct obj *) 0;
    }

    if (ring && ring->otyp == otyp) {
        /* reasons ring can't be removed match those checked by select_off();
           limbless case has extra checks because ordinarily it's temporary */
        if (nolimbs(youmonst.data) && uamul
            && uamul->otyp == AMULET_OF_UNCHANGING && uamul->cursed)
            return uamul;
        if (welded(uwep) && (ring == uright || bimanual(uwep)))
            return uwep;
        if (uarmg && uarmg->cursed)
            return uarmg;
        if (ring->cursed)
            return ring;
    }
    /* either no ring or not right type or nothing prevents its removal */
    return (struct obj *) 0;
}

/* also for praying; find worn item that confers "Unchanging" attribute */
struct obj *
unchanger()
{
    if (uamul && uamul->otyp == AMULET_OF_UNCHANGING)
        return uamul;
    return 0;
}

STATIC_PTR
int
select_off(otmp)
register struct obj *otmp;
{
    struct obj *why;
    char buf[BUFSZ];

    if (!otmp)
        return 0;
    *buf = '\0'; /* lint suppresion */

    /* special ring checks */
    if (otmp == uright || otmp == uleft) {
        if (nolimbs(youmonst.data)) {
            pline_The("ring is stuck.");
            return 0;
        }
        why = 0; /* the item which prevents ring removal */
        if (welded(uwep) && (otmp == uright || bimanual(uwep))) {
            Sprintf(buf, "free a weapon %s", body_part(HAND));
            why = uwep;
        } else if (uarmg && uarmg->cursed) {
            Sprintf(buf, "take off your %s", c_gloves);
            why = uarmg;
        }
        if (why) {
            You("cannot %s to remove the ring.", buf);
            why->bknown = TRUE;
            return 0;
        }
    }
    /* special glove checks */
    if (otmp == uarmg) {
        if (welded(uwep)) {
            You("are unable to take off your %s while wielding that %s.",
                c_gloves, is_sword(uwep) ? c_sword : c_weapon);
            uwep->bknown = TRUE;
            return 0;
        } else if (Glib) {
            You_cant("take off the slippery %s with your slippery %s.",
                     c_gloves, makeplural(body_part(FINGER)));
            return 0;
        }
    }
    /* special boot checks */
    if (otmp == uarmf) {
        if (u.utrap && u.utraptype == TT_BEARTRAP) {
            pline_The("bear trap prevents you from pulling your %s out.",
                      body_part(FOOT));
            return 0;
        } else if (u.utrap && u.utraptype == TT_INFLOOR) {
            You("are stuck in the %s, and cannot pull your %s out.",
                surface(u.ux, u.uy), makeplural(body_part(FOOT)));
            return 0;
        }
    }
    /* special suit and shirt checks */
    if (otmp == uarm || otmp == uarmu) {
        why = 0; /* the item which prevents disrobing */
        if (uarmc && uarmc->cursed) {
            Sprintf(buf, "remove your %s", cloak_simple_name(uarmc));
            why = uarmc;
        } else if (otmp == uarmu && uarm && uarm->cursed) {
            Sprintf(buf, "remove your %s", c_suit);
            why = uarm;
        } else if (welded(uwep) && bimanual(uwep)) {
            Sprintf(buf, "release your %s",
                    is_sword(uwep) ? c_sword : (uwep->otyp == BATTLE_AXE)
                                                   ? c_axe
                                                   : c_weapon);
            why = uwep;
        }
        if (why) {
            You("cannot %s to take off %s.", buf, the(xname(otmp)));
            why->bknown = TRUE;
            return 0;
        }
    }
    /* basic curse check */
    if (otmp == uquiver || (otmp == uswapwep && !u.twoweap)) {
        ; /* some items can be removed even when cursed */
    } else {
        /* otherwise, this is fundamental */
        if (cursed(otmp))
            return 0;
    }

    if (otmp == uarm)
        context.takeoff.mask |= WORN_ARMOR;
    else if (otmp == uarmc)
        context.takeoff.mask |= WORN_CLOAK;
    else if (otmp == uarmf)
        context.takeoff.mask |= WORN_BOOTS;
    else if (otmp == uarmg)
        context.takeoff.mask |= WORN_GLOVES;
    else if (otmp == uarmh)
        context.takeoff.mask |= WORN_HELMET;
    else if (otmp == uarms)
        context.takeoff.mask |= WORN_SHIELD;
    else if (otmp == uarmu)
        context.takeoff.mask |= WORN_SHIRT;
    else if (otmp == uleft)
        context.takeoff.mask |= LEFT_RING;
    else if (otmp == uright)
        context.takeoff.mask |= RIGHT_RING;
    else if (otmp == uamul)
        context.takeoff.mask |= WORN_AMUL;
    else if (otmp == ublindf)
        context.takeoff.mask |= WORN_BLINDF;
    else if (otmp == uwep)
        context.takeoff.mask |= W_WEP;
    else if (otmp == uswapwep)
        context.takeoff.mask |= W_SWAPWEP;
    else if (otmp == uquiver)
        context.takeoff.mask |= W_QUIVER;

    else
        impossible("select_off: %s???", doname(otmp));

    return (0);
}

STATIC_OVL struct obj *
do_takeoff()
{
    struct obj *otmp = (struct obj *) 0;
    struct takeoff_info *doff = &context.takeoff;

    if (doff->what == W_WEP) {
        if (!cursed(uwep)) {
            setuwep((struct obj *) 0);
            You("are empty %s.", body_part(HANDED));
            u.twoweap = FALSE;
        }
    } else if (doff->what == W_SWAPWEP) {
        setuswapwep((struct obj *) 0);
        You("no longer have a second weapon readied.");
        u.twoweap = FALSE;
    } else if (doff->what == W_QUIVER) {
        setuqwep((struct obj *) 0);
        You("no longer have ammunition readied.");
    } else if (doff->what == WORN_ARMOR) {
        otmp = uarm;
        if (!cursed(otmp))
            (void) Armor_off();
    } else if (doff->what == WORN_CLOAK) {
        otmp = uarmc;
        if (!cursed(otmp))
            (void) Cloak_off();
    } else if (doff->what == WORN_BOOTS) {
        otmp = uarmf;
        if (!cursed(otmp))
            (void) Boots_off();
    } else if (doff->what == WORN_GLOVES) {
        otmp = uarmg;
        if (!cursed(otmp))
            (void) Gloves_off();
    } else if (doff->what == WORN_HELMET) {
        otmp = uarmh;
        if (!cursed(otmp))
            (void) Helmet_off();
    } else if (doff->what == WORN_SHIELD) {
        otmp = uarms;
        if (!cursed(otmp))
            (void) Shield_off();
    } else if (doff->what == WORN_SHIRT) {
        otmp = uarmu;
        if (!cursed(otmp))
            (void) Shirt_off();
    } else if (doff->what == WORN_AMUL) {
        otmp = uamul;
        if (!cursed(otmp))
            Amulet_off();
    } else if (doff->what == LEFT_RING) {
        otmp = uleft;
        if (!cursed(otmp))
            Ring_off(uleft);
    } else if (doff->what == RIGHT_RING) {
        otmp = uright;
        if (!cursed(otmp))
            Ring_off(uright);
    } else if (doff->what == WORN_BLINDF) {
        if (!cursed(ublindf))
            Blindf_off(ublindf);
    } else {
        impossible("do_takeoff: taking off %lx", doff->what);
    }

    return (otmp);
}

/* occupation callback for 'A' */
STATIC_PTR
int
take_off(VOID_ARGS)
{
    register int i;
    register struct obj *otmp;
    struct takeoff_info *doff = &context.takeoff;

    if (doff->what) {
        if (doff->delay > 0) {
            doff->delay--;
            return (1); /* still busy */
        } else {
            if ((otmp = do_takeoff()))
                off_msg(otmp);
        }
        doff->mask &= ~doff->what;
        doff->what = 0L;
    }

    for (i = 0; takeoff_order[i]; i++)
        if (doff->mask & takeoff_order[i]) {
            doff->what = takeoff_order[i];
            break;
        }

    otmp = (struct obj *) 0;
    doff->delay = 0;

    if (doff->what == 0L) {
        You("finish %s.", doff->disrobing);
        return 0;
    } else if (doff->what == W_WEP) {
        doff->delay = 1;
    } else if (doff->what == W_SWAPWEP) {
        doff->delay = 1;
    } else if (doff->what == W_QUIVER) {
        doff->delay = 1;
    } else if (doff->what == WORN_ARMOR) {
        otmp = uarm;
        /* If a cloak is being worn, add the time to take it off and put
         * it back on again.  Kludge alert! since that time is 0 for all
         * known cloaks, add 1 so that it actually matters...
         */
        if (uarmc)
            doff->delay += 2 * objects[uarmc->otyp].oc_delay + 1;
    } else if (doff->what == WORN_CLOAK) {
        otmp = uarmc;
    } else if (doff->what == WORN_BOOTS) {
        otmp = uarmf;
    } else if (doff->what == WORN_GLOVES) {
        otmp = uarmg;
    } else if (doff->what == WORN_HELMET) {
        otmp = uarmh;
    } else if (doff->what == WORN_SHIELD) {
        otmp = uarms;
    } else if (doff->what == WORN_SHIRT) {
        otmp = uarmu;
        /* add the time to take off and put back on armor and/or cloak */
        if (uarm)
            doff->delay += 2 * objects[uarm->otyp].oc_delay;
        if (uarmc)
            doff->delay += 2 * objects[uarmc->otyp].oc_delay + 1;
    } else if (doff->what == WORN_AMUL) {
        doff->delay = 1;
    } else if (doff->what == LEFT_RING) {
        doff->delay = 1;
    } else if (doff->what == RIGHT_RING) {
        doff->delay = 1;
    } else if (doff->what == WORN_BLINDF) {
        doff->delay = 2;
    } else {
        impossible("take_off: taking off %lx", doff->what);
        return 0; /* force done */
    }

    if (otmp)
        doff->delay += objects[otmp->otyp].oc_delay;

    /* Since setting the occupation now starts the counter next move, that
     * would always produce a delay 1 too big per item unless we subtract
     * 1 here to account for it.
     */
    if (doff->delay > 0)
        doff->delay--;

    set_occupation(take_off, doff->disrobing, 0);
    return (1); /* get busy */
}

/* clear saved context to avoid inappropriate resumption of interrupted 'A' */
void
reset_remarm()
{
    context.takeoff.what = context.takeoff.mask = 0L;
    context.takeoff.disrobing[0] = '\0';
}

/* the 'A' command -- remove multiple worn items */
int
doddoremarm()
{
    int result = 0;

    if (context.takeoff.what || context.takeoff.mask) {
        You("continue %s.", context.takeoff.disrobing);
        set_occupation(take_off, context.takeoff.disrobing, 0);
        return 0;
    } else if (!uwep && !uswapwep && !uquiver && !uamul && !ublindf && !uleft
               && !uright && !wearing_armor()) {
        You("are not wearing anything.");
        return 0;
    }

    add_valid_menu_class(0); /* reset */
    if (flags.menu_style != MENU_TRADITIONAL
        || (result = ggetobj("take off", select_off, 0, FALSE,
                             (unsigned *) 0)) < -1)
        result = menu_remarm(result);

    if (context.takeoff.mask) {
        /* default activity for armor and/or accessories,
           possibly combined with weapons */
        (void) strncpy(context.takeoff.disrobing, "disrobing", CONTEXTVERBSZ);
        /* specific activity when handling weapons only */
        if (!(context.takeoff.mask & ~(W_WEP | W_SWAPWEP | W_QUIVER)))
            (void) strncpy(context.takeoff.disrobing, "disarming",
                           CONTEXTVERBSZ);
        (void) take_off();
    }
    /* The time to perform the command is already completely accounted for
     * in take_off(); if we return 1, that would add an extra turn to each
     * disrobe.
     */
    return 0;
}

STATIC_OVL int
menu_remarm(retry)
int retry;
{
    int n, i = 0;
    menu_item *pick_list;
    boolean all_worn_categories = TRUE;

    if (retry) {
        all_worn_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_worn_categories = FALSE;
        n = query_category("What type of things do you want to take off?",
                           invent, WORN_TYPES | ALL_TYPES, &pick_list,
                           PICK_ANY);
        if (!n)
            return 0;
        for (i = 0; i < n; i++) {
            if (pick_list[i].item.a_int == ALL_TYPES_SELECTED)
                all_worn_categories = TRUE;
            else
                add_valid_menu_class(pick_list[i].item.a_int);
        }
        free((genericptr_t) pick_list);
    } else if (flags.menu_style == MENU_COMBINATION) {
        all_worn_categories = FALSE;
        if (ggetobj("take off", select_off, 0, TRUE, (unsigned *) 0) == -2)
            all_worn_categories = TRUE;
    }

    n = query_objlist("What do you want to take off?", invent,
                      SIGNAL_NOMENU | USE_INVLET | INVORDER_SORT, &pick_list,
                      PICK_ANY,
                      all_worn_categories ? is_worn : is_worn_by_type);
    if (n > 0) {
        for (i = 0; i < n; i++)
            (void) select_off(pick_list[i].item.a_obj);
        free((genericptr_t) pick_list);
    } else if (n < 0 && flags.menu_style != MENU_COMBINATION) {
        There("is nothing else you can remove or unwield.");
    }
    return 0;
}

/* hit by destroy armor scroll/black dragon breath/monster spell */
int
destroy_arm(atmp)
register struct obj *atmp;
{
    register struct obj *otmp;
#define DESTROY_ARM(o)                            \
    ((otmp = (o)) != 0 && (!atmp || atmp == otmp) \
             && (!obj_resists(otmp, 0, 90))       \
         ? (otmp->in_use = TRUE)                  \
         : FALSE)

    if (DESTROY_ARM(uarmc)) {
        if (donning(otmp))
            cancel_don();
        Your("%s crumbles and turns to dust!", cloak_simple_name(uarmc));
        (void) Cloak_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarm)) {
        if (donning(otmp))
            cancel_don();
        Your("armor turns to dust and falls to the %s!", surface(u.ux, u.uy));
        (void) Armor_gone();
        useup(otmp);
    } else if (DESTROY_ARM(uarmu)) {
        if (donning(otmp))
            cancel_don();
        Your("shirt crumbles into tiny threads and falls apart!");
        (void) Shirt_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarmh)) {
        if (donning(otmp))
            cancel_don();
        Your("%s turns to dust and is blown away!", helm_simple_name(uarmh));
        (void) Helmet_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarmg)) {
        if (donning(otmp))
            cancel_don();
        Your("gloves vanish!");
        (void) Gloves_off();
        useup(otmp);
        selftouch("You");
    } else if (DESTROY_ARM(uarmf)) {
        if (donning(otmp))
            cancel_don();
        Your("boots disintegrate!");
        (void) Boots_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarms)) {
        if (donning(otmp))
            cancel_don();
        Your("shield crumbles away!");
        (void) Shield_off();
        useup(otmp);
    } else {
        return 0; /* could not destroy anything */
    }

#undef DESTROY_ARM
    stop_occupation();
    return (1);
}

void
adj_abon(otmp, delta)
register struct obj *otmp;
register schar delta;
{
    if (uarmg && uarmg == otmp && otmp->otyp == GAUNTLETS_OF_DEXTERITY) {
        if (delta) {
            makeknown(uarmg->otyp);
            ABON(A_DEX) += (delta);
        }
        context.botl = 1;
    }
    if (uarmh && uarmh == otmp && otmp->otyp == HELM_OF_BRILLIANCE) {
        if (delta) {
            makeknown(uarmh->otyp);
            ABON(A_INT) += (delta);
            ABON(A_WIS) += (delta);
        }
        context.botl = 1;
    }
}

/* decide whether a worn item is covered up by some other worn item,
   used for dipping into liquid and applying grease;
   some criteria are different than select_off()'s */
boolean
inaccessible_equipment(obj, verb, only_if_known_cursed)
struct obj *obj;
const char *verb; /* "dip" or "grease", or null to avoid messages */
boolean only_if_known_cursed; /* ignore covering unless known to be cursed */
{
    static NEARDATA const char need_to_take_off_outer_armor[] =
        "need to take off %s to %s %s.";
    char buf[BUFSZ];
    boolean anycovering = !only_if_known_cursed; /* more comprehensible... */
#define BLOCKSACCESS(x) (anycovering || ((x)->cursed && (x)->bknown))

    if (!obj || !obj->owornmask)
        return FALSE; /* not inaccessible */

    /* check for suit covered by cloak */
    if (obj == uarm && uarmc && BLOCKSACCESS(uarmc)) {
        if (verb) {
            Strcpy(buf, yname(uarmc));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* check for shirt covered by suit and/or cloak */
    if (obj == uarmu
        && ((uarm && BLOCKSACCESS(uarm)) || (uarmc && BLOCKSACCESS(uarmc)))) {
        if (verb) {
            char cloaktmp[QBUFSZ], suittmp[QBUFSZ];
            /* if sameprefix, use yname and xname to get "your cloak and suit"
               or "Manlobbi's cloak and suit"; otherwise, use yname and yname
               to get "your cloak and Manlobbi's suit" or vice versa */
            boolean sameprefix = (uarm && uarmc
                                  && !strcmp(shk_your(cloaktmp, uarmc),
                                             shk_your(suittmp, uarm)));

            *buf = '\0';
            if (uarmc)
                Strcat(buf, yname(uarmc));
            if (uarm && uarmc)
                Strcat(buf, " and ");
            if (uarm)
                Strcat(buf, sameprefix ? xname(uarm) : yname(uarm));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* check for ring covered by gloves */
    if ((obj == uleft || obj == uright) && uarmg && BLOCKSACCESS(uarmg)) {
        if (verb) {
            Strcpy(buf, yname(uarmg));
            You(need_to_take_off_outer_armor, buf, verb, yname(obj));
        }
        return TRUE;
    }
    /* item is not inaccessible */
    return FALSE;
}

/*do_wear.c*/
