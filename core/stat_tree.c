#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Copied and modified from http://pine.cs.yale.edu/pinewiki/C/AvlTree google cache */

#include "stat_tree.h"
long g_tw_min_bin = 0;
long g_tw_max_bin = 0;


/* implementation of an AVL tree with explicit heights */

/* find max bin in tree */
stat_node *find_stat_max(stat_node *t)
{
    if (t == AVL_EMPTY)
        return NULL;

    if (t->child[1])
        return find_stat_max(t->child[1]);
    else 
        return t;
}

/* recursive function for creating a tree for stats collection. 
 * takes in the current node, uses in order traversal.
 */
//TODO need to handle small special cases
static long create_tree(stat_node *current_node, tw_stat key, int height)
{
    if (height > 1)
    {
        current_node->child[0] = tw_calloc(TW_LOC, "statistics collection (tree)", sizeof(struct stat_node), 1);
        current_node->child[1] = tw_calloc(TW_LOC, "statistics collection (tree)", sizeof(struct stat_node), 1);
        //current_node->child[0] = calloc(1, sizeof(struct stat_node));
        //current_node->child[1] = calloc(1, sizeof(struct stat_node));
        int k1 = create_tree(current_node->child[0], key, height-1);
        current_node->key = k1 + g_tw_time_interval;
        current_node->height = height;
        create_tree(current_node->child[1], (current_node->key) + g_tw_time_interval, height-1);
        stat_node *maxptr = find_stat_max(current_node);
        return maxptr->key;
    }
    else if (height == 1)
    {
        current_node->key = key;
        current_node->height = height;
        return key;
    }
    return -1;
}

/* initialize the tree with some number of bins.
 * currently about 10% of bins expected based on end time and time interval
 * returns pointer to the root
 */
stat_node *init_stat_tree(tw_stat start)
{
    // TODO num_bins should actually just be set to some number
    int height = 6;
    //tw_stat end = num_bins * g_tw_time_interval;
    stat_node *root = tw_calloc(TW_LOC, "statistics collection (tree)", sizeof(struct stat_node), 3);

    root->child[0] = root + 1;
    root->child[1] = root + 2;
    long k = create_tree(root->child[0], start, height-1);
    root->key = k + g_tw_time_interval;
    root->height = height;
    create_tree(root->child[1], (root->key) + g_tw_time_interval, height-1);
    return root;
}

/* return nonzero if key is present in tree */
// TODO need to make sure that original caller adds bins if it gets a return value == 0
stat_node *stat_increment(stat_node *t, long time_stamp, int stat_type, stat_node *root)
{
    // check that there is a bin for this time stamp
    if (time_stamp > g_tw_max_bin + g_tw_time_interval)
    {
        root = init_stat_tree(g_tw_max_bin + g_tw_time_interval);
        stat_node *tmp = find_stat_max(root);
        g_tw_max_bin = tmp->key;
    }

    // find bin this time stamp belongs to
    int key = floor(time_stamp / g_tw_time_interval) * g_tw_time_interval;
    if (t == AVL_EMPTY) {
        return root;
    }
    
    if (key == t->key) 
    {
        t->bin.stats[stat_type] += 1;
        //return 1;
    }

    if (key < t->key)
    {
        if (t->child[0])
        {
            stat_increment(t->child[0], time_stamp, stat_type, root);
            //return 1;
        }
        //else
        //    return 0;
    }
    else if (key > t->key)
    {
        if (t->child[1])
        {
            stat_increment(t->child[1], time_stamp, stat_type, root);
            //return 1;
        }
        //else
        //    return 0;
    }
    return root;
}

/* recompute height of a node */
static void fix_node_height(stat_node *t)
{
    assert(t != AVL_EMPTY);
    
    t->height = 1 + ROSS_MAX(statGetHeight(t->child[0]), statGetHeight(t->child[1]));
}

static void fix_all_heights(stat_node *root)
{
    if (root != AVL_EMPTY)
    {
        if (root->child[0])
            fix_all_heights(root->child[0]);
        if (root->child[1])
            fix_all_heights(root->child[1]);
        fix_node_height(root);
    }
}

/* rotate child[d] to root */
/* assumes child[d] exists */
/* Picture:
 *
 *     y            x
 *    / \   <==>   / \
 *   x   C        A   y
 *  / \              / \
 * A   B            B   C
 *
 */
static stat_node *statRotate(stat_node *root, int d)
{
    stat_node *oldRoot;
    stat_node *newRoot;
    stat_node *oldMiddle;
    
    oldRoot = root;
    newRoot = oldRoot->child[d];
    oldMiddle = newRoot->child[!d];
    
    oldRoot->child[d] = oldMiddle;
    newRoot->child[!d] = oldRoot;
    root = newRoot;
    
    /* update heights */
    fix_node_height((root)->child[!d]);   /* old root */
    fix_node_height(root);                /* new root */
    return root;
}

/* rebalance at node if necessary */
/* also fixes height */
// TODO works correctly, but could probably be better done
// because of the situation, the right subtree usually has more nodes after deletions
// potentially rebalance more often would help solve this
static stat_node *statRebalance(stat_node *t)
{
    fix_all_heights(t);
    int d;
    
    if (t != AVL_EMPTY) {
        for (d = 0; d < 2; d++) {
            /* maybe child[d] is now too tall */
            if (statGetHeight((t)->child[d]) > statGetHeight((t)->child[!d]) + 1) {
                /* imbalanced! */
                /* how to fix it? */
                /* need to look for taller grandchild of child[d] */
                if (statGetHeight((t)->child[d]->child[d]) > statGetHeight((t)->child[d]->child[!d])) {
                    /* same direction grandchild wins, do single rotation */
                    t = statRotate(t, d);
                }
                else {
                    /* opposite direction grandchild moves up, do double rotation */
                    stat_node *temp = statRotate((t)->child[d], !d);
                    t->child[d] = temp;
                    t = statRotate(t, d);
                }
                
                //return;   /* statRotate called fix_node_height */
            }
        }
        
        /* update height */
        fix_all_heights(t);
    }
    return t;
}

/* determined that more bins should be added */
stat_node *add_nodes(stat_node *root)
{
    stat_node *old_root = root;
    stat_node *max_node = statDeleteMax(root, NULL);
    tw_stat start = max_node->key + g_tw_time_interval;
    stat_node *subtree = init_stat_tree(start);

    max_node->child[0] = old_root;
    // connect subtree to tree
    max_node->child[1] = subtree;
    root = max_node;

    fix_all_heights(root);
    root = statRebalance(root);
    return root;
}

/* return height of an AVL tree */
int statGetHeight(stat_node *t)
{
    if (t != AVL_EMPTY) {
        return t->height;
    }
    else {
        return 0;
    }
}

/* assert height fields are correct throughout tree */
void statSanityCheck(stat_node *root)
{
    int i;
    
    if (root != AVL_EMPTY) {
        for (i = 0; i < 2; i++) {
            statSanityCheck(root->child[i]);
        }
        
        assert(root->height == 1 + ROSS_MAX(statGetHeight(root->child[0]), statGetHeight(root->child[1])));
    }
}

static stat_node *write_bins(FILE *log, stat_node *t, tw_stime gvt, stat_node *parent, stat_node *root, int *flag)
{
    if (t != AVL_EMPTY)
    {
        if (t->child[0])
            root = write_bins(log, t->child[0], gvt, t, root, flag);
        if (t->key + g_tw_time_interval <= gvt)
        { // write in order
            char buffer[2048];
            char tmp[32];
            sprintf(tmp, "%ld,%ld,", g_tw_mynode, t->key);
            strcat(buffer, tmp);
            int i;
            for (i = 0; i <= NUM_INTERVAL_STATS; i++)
            {
                sprintf(tmp, "%llu", t->bin.stats[i]);
                strcat(buffer, tmp);
                if (i != NUM_INTERVAL_STATS)
                    sprintf(tmp, ",");
                else
                    sprintf(tmp, "\n");
                strcat(buffer, tmp);
            }
            MPI_File_write(interval_file, buffer, strlen(buffer), MPI_CHAR, MPI_STATUS_IGNORE);
        }
        if (t->child[1] && *flag)
            root = write_bins(log, t->child[1], gvt, t, root, flag);
        
        if (t->key + g_tw_time_interval <= gvt)
        {
            // can delete this node now
            root = statDelete(t, t->key, parent, root);
            //free(t);
            //if (parent != root)
            //    statRebalance(parent);
            *flag = 1;
        }
        else // no need to continue checking nodes
            *flag = 0;
    }

    return root;
}

/* call after GVT; write bins < GVT to file and delete */
stat_node *gvt_write_bins(FILE *log, stat_node *t, tw_stime gvt)
{
    int flag = 1;
    t = write_bins(log, t, gvt, NULL, t, &flag);
    t = statRebalance(t);
    return t;
}

/* free a tree */
void statDestroy(stat_node *t)
{
    if (t != AVL_EMPTY) {
        statDestroy(t->child[0]);
        t->child[0] = AVL_EMPTY;
        statDestroy(t->child[1]);
        t->child[1] = AVL_EMPTY;
        stat_free(t);
    }
}

void stat_free(stat_node *t)
{
    (t)->child[0] = AVL_EMPTY;
    (t)->child[1] = AVL_EMPTY;
    (t)->key = -1;
    (t)->height = 0;
}

/* print all elements of the tree in order */
void statPrintKeys(stat_node *t)
{
    if (t != AVL_EMPTY) {
        statPrintKeys(t->child[0]);
        printf("%ld\t%d\n", t->key, t->height);
        statPrintKeys(t->child[1]);
    }
}

/* delete and return minimum value in a tree */
// Need to make sure to rebalance after calling
stat_node *statDeleteMin(stat_node *t, stat_node *parent)
{
    stat_node *oldroot;
    stat_node *min_node;

    assert(t != AVL_EMPTY);

    if((t)->child[0] == AVL_EMPTY) {
        /* root is min value */
        min_node = t;
        if (parent != AVL_EMPTY)
        {
            if (t->key < parent->key)
                parent->child[0] = t->child[1];
            else
                parent->child[1] = t->child[1];
        }
        //free(oldroot);
    } else {
        /* min value is in left subtree */
        min_node = statDeleteMin((t)->child[0], t);
    }

    return min_node;
}

/* delete and return max node in a tree */
// Need to make sure to rebalance after calling
stat_node *statDeleteMax(stat_node *t, stat_node *parent)
{
    stat_node *max_node;

    assert(t != AVL_EMPTY);

    if((t)->child[1] == AVL_EMPTY) {
        /* root is max value */
        max_node = t;
        parent->child[1] = NULL;
    } else {
        /* max value is in right subtree */
        max_node = statDeleteMax((t)->child[1], t);
    }

    return max_node;
}

/* delete the given value */
// Need to make sure to rebalance after calling
stat_node *statDelete(stat_node *t, long key, stat_node *parent, stat_node *root)
{
    stat_node *oldroot;
    stat_node *newroot = NULL;

    if(t == AVL_EMPTY) {
        return root;
    } else if((t)->key == key) {
        /* do we have a right child? */
        if((t)->child[1] != AVL_EMPTY) {
            /* give root min value in right subtree */
            oldroot = t;
            stat_node *temp = statDeleteMin(oldroot->child[1], oldroot);
            if (parent != AVL_EMPTY){
                if (oldroot->key < parent->key)
                    parent->child[0] = temp; 
                else
                    parent->child[1] = temp;
            }
            else
                newroot = t; 
            t->child[1] = oldroot->child[1];
            //free(oldroot);
        } else {
            /* splice out root */
            if(parent != AVL_EMPTY){
                if (t->key < parent->key)
                    parent->child[0] = NULL;
                else 
                    parent->child[1] = NULL;
            }
            else
                newroot = t;
            oldroot = (t);
            t = (t)->child[0];
            
            //free(oldroot);
        }
    } else {
        statDelete((t)->child[key > (t)->key], key, t, root);
    }
    if (!newroot)
        newroot = root;
    //newroot = statRebalance(newroot);
    return newroot;
}