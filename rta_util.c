

static int compint (const void *a, const void *b)
{
    return *(int *) a - *(int *) b;
}

/* generate k random indices out of 0..n-1 in vector sample(k) 
   time complexity: O(k log k), space complexity O(k) 

   todo: reimplement using permutation + hashing for O(k) complexity 
*/
void rta_choose_k_from_n (int k, int n, int *sample)
{
    int doubles = 99;
    int i;
    
    if (k >= n)
    {	/* error: non-specified case */
	for (i = 0; i < k; i++)
	    sample[i] = i % n;
	fts_post("illegal parameters for choose %d from %d!!!\n", k, n);
	return;
    }

    /* generate k random numbers with possible repetition */
    for (i = 0; i < k; i++)
	sample[i] = random() % n;

    while (doubles > 0)
    {
	/* sort and check for uniqueness */
	qsort(sample, k, sizeof(int), compint);
	
	for (i = 1, doubles = 0; i < k; i++)
	    if (sample[i - 1] == sample[i])
	    {   /* repetition: generate new random value */
		sample[i] = random() % n;
		doubles++;
	    }
	fts_post("choose %d from %d -> doubles %d\n", k, n, doubles);
    }
}


/* return index i such that i is the highest i for which x < arr[i]
   num !> 0 */
int rta_find_int (int x, int num, int *arr)
{
    int i = 0;

    while (i < num  &&  x >= arr[i])
	i++;

    return i;
}
