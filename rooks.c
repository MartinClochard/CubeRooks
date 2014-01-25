#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>

//#define ROOKS_MONITOR

#ifdef ROOKS_MONITOR
//For Robin's monitor.
#include <pthread.h>
#include <unistd.h>
#endif

static uint64_t conf_counter = 0;

//bit-set type. Do not forget
//to change the type size accordingly or
//you might have a bad surprise...
typedef uint8_t bitfield;
//type for dimension, idem.
typedef uint8_t dims;

//Backtracking structure.
typedef struct grid {
  //Size of the grid.
  dims len;
  //Frozen block description.
  //Idea: reordering of rows/column is a free parameter,
  //freeze this reordering by increments.
  dims xd;
  dims yd;
  //Current filling level (I add rooks floor per floor).
  dims lv;
  //Number of rooks in the grid.
  int rooks;
  //Height buffer (not strictly needed except for pretty-printing).
  dims * buffer;
  //Projection of rooks on the x/y plane.
  //x -> y -> bool.
  bitfield * gridx;
  //y -> x -> bool.
  bitfield * gridy;
  //Projection of rooks on the x/z axis,
  //x -> level -> bool
  bitfield * xproj;
  //Projection of rooks on the y/z axis.
  //y -> level -> bool
  bitfield * yproj;
  //Hard to describe precisely.
  //Let say we have rooks at heights i and j
  //on the same horizontal, x coordinates xi and xj (not necessarily different)
  //Then xj will be forbidden at level i and
  //xi at level j.
  //The ones for max i/j will be noted in xproj_split
  //so that xproj_split is correct at maximum level (and only at maximum
  //level--it is filled in level order so we do not care).
  //Other point of view: projection of "forbidden" constraints.
  //level -> x -> bool.
  bitfield * xproj_split;
  //level -> y -> bool.
  bitfield * yproj_split;
  //Do the current floor contain a rook ?
  uint8_t filled_floor;
  //Min coordinates for floor start.
  //Maintained to get rid of one more freedom parameter:
  //the order of the rooks heights.
  //It is fixed by ensuring that the firsts (lex-order) occurences
  //of a rook at each level are lex-order sorted.
  //Ok for the algorithm since it fill each level in lex-order...
  dims xs;
  dims ys;
} grid;

//Save changed values during update (add/remove rook).
typedef struct saved {
  bitfield xproj_splitp;
  bitfield yproj_splitp;
  //Saving.
  uint8_t old_filled_floor;
} saved;

void print_conf(grid * g) {
  int len = g->len;
  int i;
  int j;
  printf("size %d:\n\n",g->rooks);
  for(i = 0;i != len;++i) {
    for(j = 0;j != len;++j) {
      if((g->gridx)[i] & (1 << j)) {
        printf("%d ",(g->buffer)[i+len*j]);
      } else {
        printf("* ");
      }
    }
    printf("\n");
  }
  printf("\n");
}

//Addition of a rook at the maximum level.
//Fill a saving structure.
static inline void add_rook(dims x,dims y,grid * g,saved * s) {
  dims level = g->lv;
  ++(g->rooks);
  bitfield bx = 1 << x;
  bitfield by = 1 << y;
  bitfield bl = 1 << level;
  bitfield * gx = (g->gridx) + x;
  bitfield vgx = *gx | by;
  *gx = vgx;
  bitfield * gy = (g->gridy) + y;
  bitfield vgy = *gy | bx;
  *gy = vgy;
  (g->buffer)[x+(g->len)*y] = level;
  (g->xproj)[x] |= bl;
  (g->yproj)[y] |= bl;
  bitfield * pxpl = (g->xproj_split) + level;
  bitfield vxpl = *pxpl;
  s->xproj_splitp = vxpl;
  *pxpl = vxpl | vgy;
  bitfield * pypl = (g->yproj_split) + level;
  bitfield vypl = *pypl;
  s->yproj_splitp = vypl;
  *pypl = vypl | vgx;
  uint8_t fl = g->filled_floor;
  s->old_filled_floor = fl;
  g->filled_floor = 1;
  if(!fl) {
    g->xs = x;
    g->ys = y;
  }
  return;
}

//Backtracking of a rook addition.
static inline void rm_rook(dims x,dims y,grid * g,saved * s) {
  dims level = g->lv;
  --(g->rooks);
  bitfield bl = (1 << level);
  (g->gridx)[x] ^= (1 << y);
  (g->gridy)[y] ^= (1 << x);
  (g->xproj)[x] ^= bl;
  (g->yproj)[y] ^= bl;
  (g->xproj_split)[level] = s->xproj_splitp;
  (g->yproj_split)[level] = s->yproj_splitp;
  g->filled_floor = s->old_filled_floor;
  return;
}

//boilerplate...
void fill_bitfield(bitfield * b,int len) {
  memset(b,0,len * sizeof(bitfield));
}

bitfield * make_bitfield(int len) {
  bitfield * b = malloc(sizeof(bitfield)*len);
  fill_bitfield(b,len);
  return(b);
}

void fill_grid(grid * g,int len) {
  g->len = len;
  g->xd = 0;
  g->yd = 0;
  g->lv = 0;
  g->rooks = 0;
  g->gridx = make_bitfield(len);
  g->gridy = make_bitfield(len);
  g->xproj = make_bitfield(len);
  g->yproj = make_bitfield(len);
  g->xproj_split = make_bitfield(len);
  g->yproj_split = make_bitfield(len);
  g->buffer = malloc(sizeof(dims)*len*len);
  g->filled_floor = 0;
  g->xs = 0;
  g->ys = 0;
}

void free_grid(grid * g) {
  free(g->gridx);
  free(g->gridy);
  free(g->xproj);
  free(g->yproj);
  free(g->xproj_split);
  free(g->yproj_split);
  free(g->buffer);
}

//forbidden testing.
//Constant on the vertical(y) axis.
static int is_forbidden_v(dims x,grid * g) {
  return((g->xproj_split)[g->lv] & (1 << x));
}

//Constant on the horizontal(x) axis.
static int is_forbidden_h(dims y,grid * g) {
  return((g->yproj_split)[g->lv] & (1 << y));
}

//Constant on the z axis.
static int is_forbidden_z(dims x,dims y,grid * g) {
  return(((g->gridx)[x] & (1 << y))
    || ((g->xproj)[x] & (g->yproj)[y]));
}

/*
 * Smarter algorihm:
 * External loop: Fill floor by floor
 * Internal loop: 2 piece.
 * First piece: iterate INSIDE the frozen block,
 * add the decreasing sequence of elements on the left wings
 * only when 
 * Second piece: iterate OUTSIDE the frozen block,
 * Essentially by chosing how much/where
 * to put elements on the three "wings"
 */

static void backtrack_start_level(grid * g,int * max,dims xc,dims yc);
static void backtrack_start_line(grid * g,int * max,dims oldxd,dims xc,dims yc);
static void backtrack_inside(grid * g,int * max,dims oldxd,dims xc,dims yc);
static void backtrack_next_x(grid * g,int * max,dims oldxd,dims xc,dims yc);
static void backtrack_next_y(grid * g,int * max,dims oldxd,dims yc);
static void backtrack_left_wing(grid * g,int * max,dims oldxd,dims xc);
static void backtrack_corner(grid * g,int * max);
static void backtrack_next_lv(grid * g,int * max);

//Start to fill a level, starting from xc/yc.
//Suppose the frozen block is non-empty.
static void backtrack_start_level(grid * g,int * max,dims xc,dims yc) {
  backtrack_start_line(g,max,g->xd,xc,yc);
}

//Start to fill a line from xc/yc.
static void backtrack_start_line(grid * g,int * max,dims oldxd,dims xc,dims yc) {
  //So we do not have to do this test anymore.
  if(is_forbidden_h(yc,g)) {
    backtrack_next_y(g,max,oldxd,yc);
  } else {
    backtrack_inside(g,max,oldxd,xc,yc);
  }
}

//Try to fill a given cell inside the frozen block.
static void backtrack_inside(grid * g,int * max,dims oldxd,dims xc,dims yc) {
  if(!is_forbidden_v(xc,g) && !is_forbidden_z(xc,yc,g)) {
    saved s;
    add_rook(xc,yc,g,&s);
    //current line become impossible.
    backtrack_next_y(g,max,oldxd,yc);
    rm_rook(xc,yc,g,&s);
  }
  backtrack_next_x(g,max,oldxd,xc,yc);
}

//Try to go to the next cell by incrementing x.
static void backtrack_next_x(grid * g,int * max,dims oldxd,dims xc,dims yc) {
  ++xc;
  dims xd0 = g->xd;
  if(xc == xd0) {
    //We might want to extend the right wing (x frontier)
    //now (because we fill in lex_order).
    if(xd0 != g->len) {
      //Note: very important, is_forbidden_v and is_forbidden_z are impossible
      //on right wing--and here, !is_forbidden_h is already enforced.
      saved s;
      g->xd = xd0 + 1;
      add_rook(xd0,yc,g,&s);
      backtrack_next_y(g,max,oldxd,yc);
      rm_rook(xd0,yc,g,&s);
      g->xd = xd0;
    }
    backtrack_next_y(g,max,oldxd,yc);
  } else {
    backtrack_inside(g,max,oldxd,xc,yc);
  }
}

//Try to go to the next line by incrementing y.
static void backtrack_next_y(grid * g,int * max,dims oldxd,dims yc) {
  ++yc;
  if(yc == g->yd) {
    backtrack_left_wing(g,max,oldxd,0);
  } else {
    backtrack_start_line(g,max,oldxd,0,yc);
  }
}

//Put rooks on the "left" wing (y frontier).
static void backtrack_left_wing(grid * g,int * max,dims oldxd,dims xc) {
  if(g->yd != g->len) {
    dims x = xc;
    //Note: is_forbidden_h and is_forbidden_z always
    //fail here, similar to right wing extension.
    dims yd_save = g->yd;
    g->yd = yd_save + 1;
    for(;x != oldxd;++x) {
      if(!is_forbidden_v(x,g)) {
        saved s;
        add_rook(x,yd_save,g,&s);
        backtrack_left_wing(g,max,oldxd,x+1);
        rm_rook(x,yd_save,g,&s);
      }
    }
    g->yd = yd_save;
    backtrack_corner(g,max);
  } else {
    backtrack_next_lv(g,max);
  }
}

//Put rooks on the 4^th "corner" block,
//e.g the one that only contact the corner of the frozen block.
static void backtrack_corner(grid * g,int * max) {
  dims xd0 = g->xd;
  dims yd0 = g->yd;
  dims l0 = g->len;
  if(xd0 != l0 && yd0 != l0) {
    saved s;
    g->xd = xd0 + 1;
    g->yd = yd0 + 1;
    add_rook(xd0,yd0,g,&s);
    backtrack_corner(g,max);
    rm_rook(xd0,yd0,g,&s);
    g->xd = xd0;
    g->yd = yd0;
  }
  backtrack_next_lv(g,max);
}

//Try to go to the next level.
static void backtrack_next_lv(grid * g,int * max) {
  if(g->filled_floor) {
    dims lv = g->lv;
    ++lv;
    if(lv == g->len) {
      int rooks = g->rooks;
      ++conf_counter;
      if(rooks > *max) {
        printf("---------- BEST ----------");
        print_conf(g);
        *max = rooks;
      }
    } else {
      g->filled_floor = 0;
      g->lv = lv;
      dims xs0 = g->xs;
      dims ys0 = g->ys;
      backtrack_start_level(g,max,xs0,ys0);
      g->xs = xs0;
      g->ys = ys0;
      g->filled_floor = 1;
      --(g->lv);
    }
  }
}
#ifdef ROOKS_MONITOR
static void * monitor(void *g) {
        for(;;) {
                sleep(1);
                print_conf((grid *) g);
        }
        return(NULL);
}
#endif

int main(int argc,const char * argv[]) {
  //Robin's monitor.
  #ifdef ROOKS_MONITOR
  pthread_t t;
  #endif
  int max = 0;
  //DO NOT PUT 0 HERE ;)
  int len = 8;
  grid g;
  fill_grid(&g,len);
  #ifdef ROOKS_MONITOR
  pthread_create(&t, NULL, monitor, &g);
  #endif
  backtrack_corner(&g,&max);
  #ifdef ROOKS_MONITOR
  pthread_cancel(t);
  pthread_join(t,NULL);
  #endif
  printf("Result: %d\n",max);
  printf("Explored confs: %" PRIu64 "\n",conf_counter);
  free_grid(&g);
  return(0);
}

