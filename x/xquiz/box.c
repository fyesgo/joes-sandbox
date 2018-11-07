/* Basic list management functions */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "box.h"

static NUM *freenums = 0;
static LST *freelsts = 0;
static SYM *freesyms = 0;

/* Hack for getting aligned blocks */

C *amalloc(U amnt)
{
	C *blk = (C *)malloc(amnt * 2);
	if ((U) blk % amnt)
		return blk + amnt - ((U) blk % amnt);
	else
		return blk;
}

/* Create a new number */

NUM *newnum()
{
	NUM *n;
	if (!freenums) {
		n = (NUM *) amalloc(ALOCSIZE);
		// printf("newnum: %p %d\n", n, sizeof(NUM));
		*(ptrdiff_t *)(&n->n) = tNUM;
		freenums = ++n;
		while (n + 1 != freenums + ALOCSIZE / sizeof(NUM) - 1)
			*(NUM **)&(n->n) = n + 1, ++n;
		*(NUM **)&(n->n) = 0;
		// for (n = freenums; n; n = *(NUM **)&(n->n)) {
		// 	printf("%p\n", n);
		// }
	}
	// printf("Freenum ");
	// for (n = freenums; n; n = *(NUM **)&(n->n)) {
	//	printf(" %p", n);
	//}
	//printf("\n");

	//printf("newnum->%p\n", freenums);
	n = freenums;
	freenums = *(NUM **)&(n->n);
	n->n = 0.0;
	return (NUM *)n;
}

NUM *newn(double d)
{
	NUM *n = newnum();
	n->n = d;
	return n;
}

/* Create a new list */

LST *newlst()
{
	LST *n;
	if (!freelsts) {
		n = (LST *) amalloc(ALOCSIZE);
		//printf("newlst: %p\n", n);
		*(ptrdiff_t *)(&n->r) = tLST;
		freelsts = ++n;
		while (n + 1 != freelsts + ALOCSIZE / sizeof(LST) - 1)
			n->r = n + 1, ++n;
		n->r = 0;
	}
	n = freelsts;
	freelsts = n->r;
	n->r = 0;
	n->d = 0;
	return n;
}

/* Create a new symbol */

SYM *newsym()
{
	SYM *n;
	if (!freesyms) {
		n = (SYM *) amalloc(ALOCSIZE);
		printf("newsym: %p\n", n);
		*(ptrdiff_t *)&(n->r) = tSYM;
		freesyms = ++n;
		while (n + 1 != freesyms + ALOCSIZE / sizeof(SYM) - 1)
			n->r = n + 1, ++n;
		n->r = 0;
	}
	n = freesyms;
	freesyms = n->r;
	n->r = 0;
	n->s = 0;
	n->cnt = 1;
	n->prec = 0;
	n->type = tSYM;
	n->bind = 0;
	return n;
}

/* Reverse a list */

LST *reverse(LST *box)
{
	LST *prv = 0, *tmp, *nxt;
	if (!box)
		return 0;
	nxt = box->r;
	while (nxt)
		tmp = nxt->r, box->r = prv, prv = box, box = nxt, nxt = tmp;
	box->r = prv;
	return box;
}

/* Discard a list */

void discard(LST *box)
{
	LST *nxt, *tmp, *prv;
	return;
	if (!box)
		return;
	switch (typ(box)) {
		case tSYM:
			discardsym((SYM *)box);
			return;
		case tNUM:
			discardnum((NUM *)box);
			return;
		case tLST:
			do {
				discard(box->d);
				nxt = box->r;
				box->r = freelsts;
				freelsts = box;
			} while (box = nxt);
/*
 down:
 prv=0, nxt=box->d;
 while(nxt?typ(nxt)==tLST:0)
  tmp=nxt->d, box->d=prv, prv=box, box=nxt, nxt=tmp;
 box->d=prv;
 discard(nxt);
 right:
 prv=0, nxt=box->r;
 while(nxt)
  tmp=nxt->r, box->r=prv, prv=box, box=nxt, nxt=tmp;
 box->r=prv;
 while(box->r)
  {
  if(typ(box->d)==tLST) goto down;
  else discard(box->d);
  tmp=box->r;
  box->r=freelsts;
  freelsts=box;
  box=tmp;
  }
 tmp=box->d;
 box->r=freelsts;
 freelsts=box;
 if(box=tmp) goto right;
*/
	}
}

void discardnum(NUM *num)
{
	return;
	if (!num)
		return;
	*(NUM **)&(num->n) = freenums;
	freenums = num;
}

void discardsym(SYM *sym)
{
	if (!sym)
		return;
	if (sym->cnt) {
		if (!--sym->cnt) {
			/* 
			sym->r = freesyms;
			freesyms = sym; */
		}
	} else {
		printf("Double free of sym?\n");
	}
}

/* Duplicate a list */

LST *dup(LST *box)
{
	LST *n, *f;
	if (!box)
		return 0;
	switch (typ(box)) {
		case tSYM:
			return (LST *)dupsym((SYM *)box);
		case tNUM:
			return (LST *)dupnum((NUM *)box);
		case tLST:
			/* This shouldn't be recursive if d is a tLST */
			f = n = newlst();
			n->d = dup(box->d);
			while (box->r) {
				box = box->r;
				n = n->r = newlst();
				n->d = dup(box->d);
			}
			return f;
	}
	fprintf(stderr, "Unknown box type in dup?\n");
	exit(-1);
}

NUM *dupnum(NUM *num)
{
	NUM *n = newnum();
	n->n = num->n;
	return n;
}

SYM *dupsym(SYM *sym)
{
	if (sym) {
		++sym->cnt;
	}
	return sym;
}

/* Replace all occurences of the element 'ff' in the list 'box' with duplicates
   of 'with'.  The replaced elements are discarded.  The updated version of
   'box' is returned */

LST *subst(LST *box, SYM *ff, LST *with)
{
	LST *n, *f;
	if (!box && !ff)
		return with;
	if (!box)
		return 0;
	switch (typ(box)) {
		case tSYM:
			if ((SYM *)box == ff)
				return dup(with);
			else
				return dup(box);
		case tNUM:
			return dup(box);
		case tLST:
			/* This shouldn't be recursive if d is a tLST */
			f = n = newlst();
			n->d = subst(box->d, ff, with);
			while (box->r) {
				box = box->r;
				n = n->r = newlst();
				n->d = subst(box->d, ff, with);
			}
			return f;
	}
	fprintf(stderr, "Unknown box type in subst?\n");
	exit(-1);
}

/* Construct a list of n possibly empty elements */

LST *ncons(int sz, ...)
{
	va_list va;
	LST *n, *nf;
	if (!sz)
		return 0;
	va_start(va, sz);
	--sz;
	(n = nf = newlst())->d = va_arg(va, LST *);
	while (sz)
		--sz, (n = n->r = newlst())->d = va_arg(va, LST *);
	va_end(va);
	return nf;
}

/* Concatenate n possibly empty lists together */

LST *ncat(int sz,...)
{
	va_list va;
	LST *n, *nf = 0, *t;
	va_start(va, sz);
	while (sz) {
		--sz;
		t = va_arg(va, LST *);
		if (t) {
			if (nf)
				n = n->r = t;
			else
				n = nf = t;
			while (n->r)
				n = n->r;
		}
	}
	va_end(va);
	return nf;
}

/* Get size of list */

U len(LST *l)
{
	U x;
	for (x = 0; l; ++x, l = l->r) ;
	return x;
}

/* Get address of nth element of list */
/* 0 is first element of list */

LST *nth(LST *l, int n)
{
	while (n)
		--n, l = l->r;
	return l->d;
}

/* Replace nth element of list.  Returns old element */

LST *replace(LST *l, int n, LST *e)
{
	LST *o;
	while (n)
		--n, l = l->r;
	o = l->d;
	l->d = e;
	return o;
}

/* Delete nth element of list.  Returns the deleted element.  l is a pointer
   to a list pointer which is potentially updated. */

LST *delete(LST **l, int n)
{
	LST *o, *p, *q;
	if (!n) {
		LST *t = (*l)->r;
		o = (*l)->d;
		(*l)->r = 0;
		(*l)->d = 0;
		discard(*l);
		*l = t;
		return o;
	}
	--n;
	p = *l;
	q = p->r;
	while (n)
		--n, p = p->r, q = q->r;
	p->r = q->r;
	o = q->d;
	q->d = 0;
	q->r = 0;
	discard(q);
	return o;
}

/* Insert element before nth list element.  Return the new list */

LST *insert(LST *l, int n, int e)
{
	LST *p, *q, *j;
	j = newlst();
	*(int *)&j->d = e;
	if (!n) {
		j->r = l;
		return j;
	}
	--n;
	p = (LST *)&l;
	q = p->r;
	while (n)
		--n, p = p->r, q = q->r;
	p->r = j;
	j->r = q;
	return l;
}

/* Return the nth rest of list */

LST *rest(LST *l, int n)
{
	while (n)
		--n, l = l->r;
	return l;
}

/* Split off the nth rest of list */

LST *split(LST **l, int n)
{
	LST *p, *q;
	if (n) {
		p = *l;
		*l = 0;
		return p;
	}
	--n;
	p = *l;
	q = p->r;
	while (n)
		--n, p = p->r, q = q->r;
	p->r = 0;
	return q;
}

/* Return last rest of list */

LST *last(LST *l)
{
	if (l)
		while (l->r)
			l = l->r;
	return l;
}
