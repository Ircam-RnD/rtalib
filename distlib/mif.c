/**
@file	mif.c
@author	Diemo Schwarz
@date	30.11.2008
@version 0.1 

@brief	Metric Inverted File Index Structure

Copyright (C) 2008 - 2009 by IRCAM-Centre Georges Pompidou, Paris, France.
*/



#ifdef WIN32
#include <malloc.h>
static double log2(double x){ return log(x)/log(2);}
#else
#include <alloca.h>
#endif

#include <assert.h>
#include <string.h>

#include "mif.h"
#include "rta_math.h"
#include "rta_util.h"


#define MAX_FLOAT 0x7FFFFFFF


/*
 * posting list handling 
 */

void mif_pl_init(mif_postinglist_t *pl, int ki)
{
    pl->size     = 0;
    pl->entries  = rta_zalloc(ki * sizeof(*pl->entries));
}

void mif_pl_free(mif_postinglist_t *pl, int ki)
{
    int i;

    pl->size     = 0;

    for (i = 0; i < ki; i++)
    {
	mif_pl_entry_t *entry = pl->entries[i], *next;

	while (entry)
	{
	    next = entry->next;
	    rta_free(entry);
	    entry = next;
	}
    }

    rta_free(pl->entries);
}

/* insert object into correct bin of posting list pl of a reference object with order k */
void mif_pl_insert (mif_postinglist_t *pl, mif_object_t *newobj, int k)
{
    /* create new entry */
    mif_pl_entry_t *newentry = rta_malloc(sizeof(mif_pl_entry_t));
    
    newentry->obj  = *newobj;
    newentry->next = pl->entries[k];

    /* prepend entry to bin list */
    pl->entries[k] = newentry;
    pl->size++;
}



/*
 *  index structure handling
 */

/** initialise index structure */
void mif_init (mif_index_t *self, mif_distance_function_t distfunc, int nr, int ki)
{
    int i;

    self->distance = distfunc;
    self->numobj = 0;
    self->numref = nr;
    self->ki     = ki;
    self->ks     = 0;
    self->refobj = rta_malloc(nr * sizeof(mif_object_t));
    self->pl     = rta_zalloc(nr * sizeof(mif_postinglist_t));

    /* init posting lists */
    for (i = 0; i < nr; i++)
	mif_pl_init(&self->pl[i], ki);

    mif_profile_clear(&self->profile);
}


/** free allocated memory */
void mif_free (mif_index_t *self)
{
    int i;

    /* free posting lists */
    for (i = 0; i < self->numref; i++)
	mif_pl_free(&self->pl[i], self->ki);

    rta_free(self->refobj); 
    rta_free(self->pl);
}


void mif_print_pl(mif_index_t *self, int i)
{
    mif_postinglist_t *pl = &self->pl[i];
    int k;

    rta_post("pl %d  refobj %d  size %d:\n", i, self->refobj[i].index, pl->size);

    for (k = 0; k < self->ki; k++)
    {
	mif_pl_entry_t *entry = pl->entries[k];

	rta_post("  <%d: ", k);
	while (entry)
	{
	    rta_post("%d ", entry->obj.index);
	    //rta_post("%p.%d ", entry->obj.base, entry->obj.index);
	    entry = entry->next;
	}
	rta_post(">\n");
    }
    rta_post("\n");
}

void mif_print (mif_index_t *self, int verb)
{
    rta_post("\nMIF index info:\n");
    rta_post("n_obj    = %d\n", self->numobj);
    rta_post("n_ref    = %d\n", self->numref);
    rta_post("k_i      = %d\n", self->ki);
    rta_post("k_s      = %d\n", self->ks);
    rta_post("mpd      = %d\n", self->mpd);
 
    {	/* space used */
	int i, npe = 0;
	unsigned long spl     = self->numref * sizeof(*self->pl);
	unsigned long srefobj = self->numref * sizeof(mif_object_t);
	unsigned long stotal;

	for (i = 0; i < self->numref; i++)
	    npe += self->pl[i].size;

	spl += npe * sizeof(*self->pl[i].entries);
	stotal = srefobj + spl;

	rta_post("\nspace for struct              = %14lu B\n", sizeof(mif_index_t));
	rta_post("%9d ref.obj.            = %14lu B\n", self->numref, srefobj);
	rta_post("%9d postinglist entries = %14lu B", npe, spl);
	rta_post(" (%d #/refobj, %f #/bin)\n", 
		 npe / self->numref, (float) npe / self->numref / self->ki);
	rta_post("TOTAL %20f MB = %14lu B (%lu B/refobj, %f B/obj)\n", 
		 stotal / 1e6, stotal, stotal / self->numref, (float) stotal / self->numobj);
    }

    if (verb >= 1)
    {
	int i;

	rta_post("\npostinglists: ro -> <order: object...>  i.e.  for object, ro is order-closest ref.obj\n");

	for (i = 0; i < self->numref; i++)
	    mif_print_pl(self, i);
    }
}

/** set all counters in mif_index_t#profile to zero */
void mif_profile_clear (mif_profile_t *t)
{
    t->o2o = 0;
    t->searches = 0;
    t->placcess = 0;
    t->indexaccess = 0;
}

/** print profile info */
void mif_profile_print (mif_profile_t *t)
{
    rta_post("\nProfile:\n"
	     "#dist:        %d\n"
	     "#placcess:    %d \t(%ld bytes each)\n"
	     "#indexaccess: %d \t(%ld bytes each)\n"
	     "#searches:    %d\n", 
	     t->o2o, t->placcess, sizeof(mif_postinglist_t), 
	     t->indexaccess, sizeof(mif_pl_entry_t), t->searches);
}


/*
 *  build index
 */

/*choose reference objects and fill refobj array in mif struct */
static int mif_choose_refobj (mif_index_t *self, int numbase, void **base, int *numbaseobj)
{	
    int numobj = 0;
    int *cumobj = alloca((numbase + 1) * sizeof(int));
    int *sample = alloca(self->numref * sizeof(int));
    int i;

    /* how many elements are given */
    for (i = 0, cumobj[0] = 0; i < numbase; i++)
    {
	cumobj[i + 1] = numobj += numbaseobj[i];
    }
	
    /* choose reference objects: draw numref random indices */
    rta_choose_k_from_n(self->numref, numobj, sample);
	
    /* lookup base and relative index */
    for (i = 0; i < self->numref; i++)
    {
	int objind  = sample[i];
	int baseind = rta_find_int(objind, numbase, cumobj + 1);
	int relind  = objind - cumobj[baseind];
	    
	self->refobj[i].base  = base[baseind];
	self->refobj[i].index = relind;
    }

    return numobj;
}

/** Find ki closest reference objects for new object <base[b], index> and their ordering 

  newobj	object to index
  indx[ki]	out: ref. obj. index is list of ref.obj. index ordered by distance dist
  dist[ki]	out: distance between obj and refobj indx[i]

  @return	number k of reference objects found (k <= ki)
*/
static int mif_index_object (mif_index_t *self, mif_object_t *newobj, int k,
			      /*out*/ int *indx, rta_real_t *dist)
{
    int r, kmax = 0; /* numfound */

    /* init distances */ 
    for (r = 0; r < k; r++) 
	dist[r] = MAX_FLOAT;

    for (r = 0; r < self->numref; r++)
    {
	rta_real_t d = (*self->distance)(&self->refobj[r], newobj);
	    
	if (d <= dist[kmax]) 
	{   /* return original index in data and distance */
	    int pos = kmax;	/* where to insert */

	    if (kmax < k - 1)
	    {   /* first move or override */
		dist[kmax + 1] = dist[kmax];
		indx[kmax + 1] = indx[kmax];
		kmax++;
	    }

	    /* insert into sorted list of distance */
	    while (pos > 0  &&  d < dist[pos - 1])
	    {   /* move up */
		dist[pos] = dist[pos - 1];
		indx[pos] = indx[pos - 1];
		pos--;
	    }

	    indx[pos] = r;
	    dist[pos] = d;
	}
    }

    /* return actual number of found objects, can be less than k,
       then kmax is the index of the next one to find */
    return kmax + (dist[kmax] < MAX_FLOAT);
}


/* index data objects by reference objects, build postinglists */
static void mif_build_index (mif_index_t *self, int numbase, void **base, int *numbaseobj)
{  
    int          *indx = alloca(self->ki * sizeof(*indx));	/* ref. obj. index */
    rta_real_t   *dist = alloca(self->ki * sizeof(*dist));	/* distance to obj */
    mif_object_t  newobj;	/* object to index */
    int b, i, k, kfound;

    /* for each object */
    for (b = 0; b < numbase; b++)
    {
	newobj.base = base[b];

	for (i = 0; i < numbaseobj[b]; i++)
	{
	    newobj.index = i;
	    	    	
	    /* find ki closest reference objects for object <base[b], i> and their ordering */
	    kfound = mif_index_object(self, &newobj, self->ki, indx, dist);
	    /* now indx is list of ref.obj. ordered by distance dist */

	    /* insert object into posting lists of ki closest reference objects with order k */
	    for (k = 0; k < kfound; k++)
	    {
		mif_pl_insert(&self->pl[indx[k]], &newobj, k);
	    }

#if MIF_PROFILE_BUILD
	    self->profile.o2o += self->numref;
#endif
	}
    }
}


/** bulk load new data and index it
    
    @param self		mif structure
    @param numbase	number of base data blocks
    @param base		array[numbase] of base pointer to data
    @param numobj	array[numbase] of number of data objects on this base pointer

    (@return the number of nodes the tree will build)
*/
int mif_add_data (mif_index_t *self, int numbase, void **base, int *numbaseobj)
{
    /* choose numref reference objects and fill refobj array in mif struct */
    self->numobj = mif_choose_refobj(self, numbase, base, numbaseobj);

    /* index data */
    mif_build_index(self, numbase, base, numbaseobj);

    return self->numobj;
}



/******************************************************************************
 *
 *	Searching
 *
 */

#include <search.h>
#define MAXLEN   (sizeof(void *) * 2 + 4 + 1) /* 32 bit pointer + 16 bit index in hex */

typedef struct _mif_hash_entry_t 
{
    mif_object_t *obj;
    int		  accumulator;
} mif_hash_entry_t;


static void mif_object_hash_create(mif_index_t *self)
{
    hcreate(self->numobj);	/* todo: ki * ks */
}

static void mif_object_hash_destroy(mif_index_t *self)
{
    hdestroy();
}

static void mif_object_hash_makekey(mif_object_t *obj, char *key)
{
    int len = sprintf(key, "%lx%x", (long unsigned int) obj->base, obj->index);
    assert(len < MAXLEN - 1);
}

static int mif_object_hash_new(char *key, int entry)
{
    ENTRY item, *ret;

    item.key  = strdup(key);
    item.data = (void *) entry;

    ret = hsearch(item, ENTER);
    return (ret  ?  (int) ret->data  :  -1);
}

static int mif_object_hash_get(char *key)
{
    ENTRY item, *ret;

    item.key  = key;
    ret = hsearch(item, FIND);
    return (ret  ?  (int) ret->data  :  -1);
}

#define MIF_DEBUG_SEARCH 0

/* out:    indx[K] = index of the Kth nearest neighbour
   return: actual number of found neighbours */
int mif_search_knn (mif_index_t *self, mif_object_t *query, int k, 
		    /* out */ mif_object_t *obj, int *dist) 
{
    rta_real_t *qdist = alloca(self->ks * sizeof(*qdist));	/* distance of query to refobj */
    int        *qind  = alloca(self->ks * sizeof(*qind));	/* index of closest refobj */
    int		numhashobj = 0, hashobjalloc = self->ks * self->ki * 8;
    mif_hash_entry_t *hashobj = rta_malloc(hashobjalloc * sizeof(*hashobj));
    int r, kq, kmax = 0;

    /* index query object by ks-closest ref. objects, sorted by
       distance to query object. */
    kq = mif_index_object(self, query, self->ks, qind, qdist);

#if MIF_DEBUG_SEARCH
    rta_post("query %d indexed %d: (indx, dist) ", query->index, kq);
    for (r = 0; r < kq; r++)
	rta_post("(ro %d = obj %d, %f) ", 
		 qind[r], self->refobj[qind[r]].index, qdist[r]);
    rta_post("\n");
#endif

    /* hash of objects seen in ks closest ref. objects posting lists */
    mif_object_hash_create(self);

    /* induced limited spearman footrule distance:
       for all ks-closest ref. obj. (sort order position r) */
    for (r = 0; r < kq; r++)
    {   /* */
#undef rta_max
#undef rta_min
#define rta_max(a,b) (((a)>(b))? (a) : (b))
#define rta_min(a,b) (((a)<(b))? (a) : (b))

	int minp = rta_max(0,            r - self->mpd);
	int maxp = rta_min(self->ki - 1, r + self->mpd);
	mif_postinglist_t *pl = &self->pl[qind[r]];
	int p;

//	rta_post("  r %d  refobj #%d=obj %d:  MPD range %d..%d\n", 
//		 r, qind[r], self->refobj[qind[r]].index, minp, maxp);

#if MIF_PROFILE_SEARCH
	self->profile.placcess++;
#endif
	/* go through posting list order range between minp and maxp */
	for (p = minp; p <= maxp; p++)
	{
	    mif_pl_entry_t *bin = pl->entries[p];
	    
	    while (bin)
	    {
		char keybuf[MAXLEN];
		int hind;

		mif_object_hash_makekey(&bin->obj, keybuf);
		hind = (int) mif_object_hash_get(keybuf);
		
		if (hind < 0)
		{ /* new occurence of obj: insert into accumulator hash */
		    if (numhashobj >= hashobjalloc)
		    {
			hashobjalloc += rta_max(2, self->ks);
			rta_post("reallocate hash object store from %d to %d obj (size %lu)\n", 
				 hashobjalloc / 2, hashobjalloc, hashobjalloc * sizeof(*hashobj));
			hashobj = rta_realloc(hashobj, hashobjalloc * sizeof(*hashobj));
		    }
		    assert(hashobj);
		    hashobj[numhashobj].obj = &bin->obj;
		    hashobj[numhashobj].accumulator =  self->ki * self->ks;

		    hind = mif_object_hash_new(keybuf, numhashobj++);
		}

		hashobj[hind].accumulator += abs(r - p) - self->ki;
		//rta_post("  dist %d  accu %s = %d\n", p, keybuf, *accu);

		bin = bin->next;

#if MIF_PROFILE_SEARCH
		self->profile.indexaccess++;
#endif
	    }
	}
    }


    /* init distances */ 
    for (r = 0; r < k; r++) 
	dist[r] = MAX_FLOAT;

#if MIF_DEBUG_SEARCH    
    rta_post("  dist of %d hashed obj: ", numhashobj);
#endif

    /* iterate through hash and pick k lowest distances */
    for (r = 0; r < numhashobj; r++)	/*!!!! preliminary stupid iteration */
    {
	int  dtrans;

	dtrans = hashobj[r].accumulator;

#if MIF_DEBUG_SEARCH    
	rta_post("(%d, %d)  ", hashobj[r].obj->index, dtrans);
#endif
	    
	if (dtrans <= dist[kmax]) 
	{   /* return original index in data and distance */
	    int pos = kmax;	/* where to insert */

	    if (kmax < k - 1)
	    {   /* first move or override */
		dist[kmax + 1] = dist[kmax];
		obj [kmax + 1] = obj [kmax];
		kmax++;
	    }

	    /* insert into sorted list of distance */
	    while (pos > 0  &&  dtrans < dist[pos - 1])
	    {   /* move up */
		dist[pos] = dist[pos - 1];
		obj [pos] = obj [pos - 1];
		pos--;
	    }

	    obj [pos] = *hashobj[r].obj;  /* de-hash */
	    dist[pos] = dtrans;
	}
    }
#if MIF_DEBUG_SEARCH    
    rta_post("\n");
#endif

    mif_object_hash_destroy(self);

#if MIF_PROFILE_SEARCH
    self->profile.o2o += self->ks;
    self->profile.searches++;
#endif

    /* return actual number of found objects, can be less than k,
       then kmax is the index of the next one to find */
    return kmax + (dist[kmax] < MAX_FLOAT);
}


#if MIF_BUILD_TEST

#include "rta_kdtree.h"

#define NDIM 2

static rta_real_t mif_euclidean_distance (mif_object_t *a, mif_object_t *b)
{
/*    return rta_euclidean_distance(a->base + a->index * NDIM, 1,
				  b->base + b->index * NDIM, NDIM);
*/
    rta_real_t* v1 = (rta_real_t *) a->base + a->index * NDIM;
    rta_real_t* v2 = (rta_real_t *) b->base + b->index * NDIM;
    rta_real_t sum = 0;
    int i;

    for (i = 0; i < NDIM; i++) 
    {
	rta_real_t diff = v2[i] - v1[i];
	sum += diff * diff;
    }

    return sum;
}


int main (int argc, char *argv[])
{
#   define      nrow 3
#   define	ki   3
#   define	K    3

    mif_index_t mif;
    int		nref, nobj = 10; 	/* (nrow * 2 + (nrow - 2))*/
    rta_real_t  *data = rta_malloc(sizeof(rta_real_t) * nobj * NDIM);
    int i, j, kfound;

    /* create some test data */
    for (i = 0; i < nobj; i++)
    {
	data[i * NDIM] = i;
	data[i * NDIM + 1] = 0;
    }

    /* num. ref. obj. >= 2 * sqrt(nobj) */
    nref = 2 * sqrt(nobj);

    mif_init(&mif, mif_euclidean_distance, nref, ki);
    mif_add_data(&mif, 1, (void **) &data, &nobj);
    mif.ks  = 3;
    mif.mpd = 3;
    mif_print(&mif, 1);
    mif_profile_print(&mif.profile);
    mif_profile_clear(&mif.profile);

    { /* test searching */
	mif_object_t *obj  = alloca(K * sizeof(*obj));	/* objects */
	int          *dist = alloca(K * sizeof(*dist));	/* transformed distance to obj */
	mif_object_t  query;

	for (i = 0; i < nobj; i++)
	{
	    query.base = data;
	    query.index = i;
	    
	    kfound = mif_search_knn(&mif, &query, K, obj, dist);

	    rta_post("--> %d-NN of query obj %d (found %d):  ", K, i, kfound);
	    for (j = 0; j < kfound; j++)
		rta_post("%d ", obj[j].index);
	    rta_post("\n\n");
	}
    }

    mif_profile_print(&mif.profile);
    mif_free(&mif);

    rta_free(data);

    return 0;
}
#endif