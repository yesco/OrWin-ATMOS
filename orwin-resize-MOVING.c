// TODO: title not moved...
// TODO: colors messed up...
//   (because rewrite doesnpt restore fb bg only r c)
#ifdef MOWIN
void mowin(signed char dx, signed char dy, signed char dw, signed char dh) {
  Window* wf= wins + wfocus;
  char x= wf->x, y= wf->y, w= wf->w, h= wf->h, b= wf->bg, f= wf->fg;

#ifdef OPTMOV
  // MOVING only smoothly
  if (!dw && !dh) {
    char i, *t, *p, W = w + (4 + 2 + 1)+1, H = h + 2 + 2;
    char *tmp, *s = SCREENXY(x - 3, y - 2);

    // Bounds checking
    if (x + dx < 3) return;
    if (x + w + dx >= SCREENCOLS - 3) return;
    if (y + dy < 2) return;
    if (y + h + dy >= SCREENROWS - 1) return;

    // Collision/Edge detection
    // TODO: tmp should be adjustested up and left?
    if (dy) {
      tmp = s + (dy < 0 ? -SCREENCOLS : SCREENCOLS * H);
      for (i = W; i--;) if (tmp[i] != 126) return;
    } else {
      tmp = s + (dx < 0 ? -1 : W);
      for (i = H; i--;) {
        if (*tmp != 126) return;
        tmp += SCREENCOLS;
      }
    }
      
    // Allocate temporary buffer
    tmp = malloc(W * H);
    if (!tmp) return;

    // 1. Copy original state from screen to tmp
    p = s; t = tmp;
    for (i = H; i--;) {
      memcpy(t, p, W);
      p += SCREENCOLS; 
      t += W;
    }

    // 2. Update window coordinates
    wf->x += dx;
    wf->y += dy;

    // 3. Copy back from tmp to the NEW screen position
    p = SCREENXY(wf->x - 3, wf->y - 2);
    t = tmp;
    for (i = H; i--;) {
      memcpy(p, t, W);
      p += SCREENCOLS; 
      t += W;
    }

    // 4. Clean up only the exposed trailing edge left behind
    if (dy < 0) {
      // Clear bottom row that was left exposed
      fill(x - 3, y - 2 + H - 1, W, 1, 126);
    } else if (dy > 0) {
      // Clear top row that was left exposed
      fill(x - 3, y - 2, W, 1, 126);
    } else if (dx < 0) {
      // Clear right column that was left exposed
      fill(x - 3 + W - 1, y - 2, 1, H, 126);
    } else if (dx > 0) {
      // Clear left column that was left exposed
      fill(x - 3, y - 2, 1, H, 126);
    }

    free(tmp);
  } else
#endif // OPTWIN

  // TODO: RESIZE crashes!
  // Resize by capture text, undraw, and redraw
  {
    char i, j;
    int z= (w+3)*y+1; // 3= bg+fg+\n
    char *tmp= malloc(z), *t= tmp-1;

    // save text w newlines
    for(j= y; j<y+h; ++j) {
      // TODO: -2 to include bg,fg of each line!
      // but printing it may take space, lol
      // Adjust printing after NL
      for(i= x; i<x+w; ++i) 
        *++t= *SCREENXY(i, j);
      // TODO: edgecases...
      while(*t==' ') t--,cputc(8);
      *++t= '\n';
    }
    *++t= 0;

    winerase(wf);

    // only move if not overlap
    if (overlap(wf->x+dx, wf->y+dy, wf->w+dw, wf->h+dh)) {
      windraw(wf); return;
    } else {
      char iw= winp - wins;
    
      wf->x+= dx; wf->y+= dy; wf->w+= dw; wf->h+= dh;
      i= wf->c; j= wf->r;

      // Pretend to be in wfocus
      setwin(wf - wins);
      updatewinptr();

      // Draw new pusition + text
      windraw(wf);
      t= tmp;
      // not using wputs as it will yield()
      while(*t) wputc(*t++);

      // restore cursor position & color
      wf->c= i > wf->w? wf->w: i;
      wf->r= j > wf->h? wf->h: j;
      wf->bg= b; wf->fg= f;
      updatewinptr();

      setwin(iw);
    }

    free(tmp);
  }

}
#endif // MOWIN
