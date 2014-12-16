#include "FairshareTree.h"

#include "globals.h"
#include "misc.h"
#include "api.hpp"
#include "utility.h"
#include <errno.h>
#include <cstdio>
#include <sstream>
#include <stdexcept>
using namespace std;
using namespace Scheduler;
using namespace Logic;

/*
 *
 * new_group_info - allocate a new group_info struct and initalize it
 *
 * returns a ptr to the new group_info
 *
 */
group_info *new_group_info()
  {
  group_info *tmp;  /* the new group */

  if ((tmp = (group_info *) malloc(sizeof(group_info))) == NULL)
    {
    perror("Error allocating memory");
    return NULL ;
    }

  tmp -> name = NULL;

  tmp -> resgroup = UNSPECIFIED;
  tmp -> cresgroup = UNSPECIFIED;
  tmp -> shares = UNSPECIFIED;
  tmp -> percentage = 0.0;
  tmp -> usage = 1;
  tmp -> temp_usage = 1;
  tmp -> parent = NULL;
  tmp -> sibling = NULL;
  tmp -> child = NULL;

  return tmp;
  }

void add_child(group_info *ginfo, group_info *parent)
  {
  if (parent != NULL)
    {
    ginfo -> sibling = parent -> child;
    parent -> child = ginfo;
    ginfo -> parent = parent;
    ginfo -> resgroup = parent -> cresgroup;
    }
  }

void free_group_tree(group_info *root)
  {
  if (root == NULL)
    return;

  free(root->name);
  free_group_tree(root->sibling);
  free_group_tree(root->child);

  free(root);
  }

group_info *init_fairshare_root()
  {
  group_info *root;
  group_info *unknown;

  if ((root = new_group_info()) == NULL)
    return NULL;

  if ((unknown = new_group_info()) == NULL)
    {
    free_group_tree(root);
    return NULL;
    }

  if ((root->name = strdup("root")) == NULL)
    {
    free_group_tree(root);
    free_group_tree(unknown);
    return NULL;
    }

  if ((unknown->name = strdup("unknown")) == NULL)
    {
    free_group_tree(root);
    free_group_tree(unknown);
    return NULL;
    }

  root->resgroup = -1;
  root->cresgroup = 0;
  root->percentage = 1.0;

  unknown->shares = conf.unknown_shares;
  unknown->resgroup = 0;
  unknown->cresgroup = 1;
  unknown->parent = root;
  root->child = unknown;

  return root;
  }

/*
 *
 * count_shares - count the shares in a resource group
 *         a resource group is a group_info and all of its
 *         siblings
 *
 *  grp - The start of a sibling chain
 *
 * returns the number of shares
 *
 */
int count_shares(group_info *grp)
  {
  int shares = 0;  /* accumulator to count the shares */
  group_info *cur_grp;  /* the current group in a sibling chain */

  cur_grp = grp;

  while (cur_grp != NULL)
    {
    shares += cur_grp -> shares;
    cur_grp = cur_grp -> sibling;
    }

  return shares;
  }

int calc_fair_share_perc(group_info *root, int shares)
  {
  int cur_shares;  /* total number of shares in the resgrp */

  if (root == NULL)
    return 0;

  if (shares == UNSPECIFIED)
    cur_shares = count_shares(root);
  else
    cur_shares = shares;

  root -> percentage = (float) root -> shares / (float) cur_shares *
                       root -> parent -> percentage;

  calc_fair_share_perc(root -> sibling, cur_shares);

  calc_fair_share_perc(root -> child, UNSPECIFIED);

  return 1;
  }

FairshareTree::FairshareTree(const std::string& name) : p_name(name), p_count(NULL), p_tree(NULL)
{
  p_count = new int(1);
  p_tree = init_fairshare_root();
  if (p_tree == NULL)
    throw runtime_error("Couldn't initialize fairshare root.");

  this->read_configuration();
  calc_fair_share_perc(this->p_tree->child,UNSPECIFIED);
  this->read_usage();
}

FairshareTree::FairshareTree(const FairshareTree& src) : p_name(src.p_name), p_count(src.p_count), p_tree(src.p_tree)
  {
  if (p_count != NULL)
    ++(*p_count);
  }


FairshareTree::~FairshareTree()
  {
  if (p_count != NULL)
    {
    --(*p_count);
    if (*p_count == 0)
      {
      delete p_count;
      free_group_tree(p_tree);
      }
    }
  }


void FairshareTree::add_unknown(group_info *ginfo)
  {
  group_info *unknown;  /* ptr to the "unknown" group */

  unknown = this->find_ginfo("unknown");
  add_child(ginfo, unknown);
  calc_fair_share_perc(unknown -> child, UNSPECIFIED);
  }


group_info* FairshareTree::find_alloc_ginfo(const char *name)
  {
  group_info *ginfo;  /* the found group or allocated group */

  if ((ginfo = this->find_ginfo(name)) == NULL)
    {
    if ((ginfo = new_group_info()) == NULL)
      return NULL;

    if ((ginfo->name = strdup(name)) == NULL)
      {
      free(ginfo);
      return NULL;
      }

    ginfo -> shares = 1;

    this->add_unknown(ginfo);
    }

  return ginfo;
  }

group_info* FairshareTree::find_alloc_ginfo(const string& name)
  {
  return this->find_alloc_ginfo(name.c_str());
  }

group_info* FairshareTree::find_ginfo(const char *name, group_info *root) const
  {
  group_info *ginfo;  /* the found group */

  if (root == NULL || !strcmp(name, root -> name))
    return root;

  ginfo = this->find_ginfo(name, root -> sibling);

  if (ginfo == NULL)
    ginfo = this->find_ginfo(name, root -> child);

  return ginfo;
  }

group_info *FairshareTree::find_ginfo(const char *name) const
  {
  return this->find_ginfo(name,p_tree);
  }

group_info *FairshareTree::find_ginfo(const string& name) const
  {
  return this->find_ginfo(name.c_str(),p_tree);
  }

void FairshareTree::dump_to_cache(const string& metric, group_info *ginfo) const
  {
  if (ginfo == NULL)
    return;

  if (ginfo->child == NULL) /* found a leaf */
    {
    double prio = ginfo->percentage / ginfo->usage;
    ostringstream s;
    s << prio;
    xcache_store_local(ginfo->name,metric.c_str(),s.str().c_str());
    }

  this->dump_to_cache(metric,ginfo->sibling);
  this->dump_to_cache(metric,ginfo->child);
  }

void FairshareTree::dump_to_cache() const
  {
  string metric_name = string("fairshare.")+this->p_name;
  this->dump_to_cache(metric_name,this->p_tree);
  }

void FairshareTree::read_configuration()
  {
  group_info *ginfo;  /* ptr to parent group */
  group_info *new_ginfo; /* used to add each new group */
  char buf[256];  /* used to read each line from the file */
  char *nametok;  /* strtok: name of new group */
  char *grouptok;  /* strtok: parent group name */
  char *cgrouptok;  /* strtok: resgrp of the children of newgrp */
  char *sharestok;  /* strtok: the amount of shares for newgrp */
  FILE *fp;   /* file pointer to the resource group file */
  int shares;   /* number of shares for the new group */
  int cgroup;   /* resource group of the children of the grp */
  char *endp;   /* used for strtol() */
  int linenum = 0;  /* current line number in the file */

  string filename = string("resource_group.")+this->p_name;

  if ((fp = fopen(filename.c_str(), "r")) == NULL)
    throw runtime_error("Couldn't open resource_group file.");

  while (fgets(buf, 256, fp) != NULL)
    {
    if (buf[strlen(buf)-1] == '\n')
      buf[strlen(buf)-1] = '\0';

    linenum++;

    if (!skip_line(buf))
      {
      nametok = strtok(buf, " \t");
      cgrouptok = strtok(NULL, " \t");
      grouptok = strtok(NULL, " \t");
      sharestok= strtok(NULL,  " \t");

      ginfo = this->find_alloc_ginfo(grouptok);

      if (ginfo != NULL)
        {
        shares = strtol(sharestok, &endp, 10);

        if (*endp == '\0')
          {
          cgroup = strtol(cgrouptok, &endp, 10);

          if (*endp == '\0')
            {
            if ((new_ginfo = new_group_info()) == NULL)
              throw runtime_error("Couldn't allocate group info.");

            new_ginfo -> name = strdup(nametok);

            new_ginfo -> resgroup = ginfo -> cresgroup;

            new_ginfo -> cresgroup = cgroup;

            new_ginfo -> shares = shares;

            add_child(new_ginfo, ginfo);
            }
          else
            {
            throw runtime_error("Error while parsing resource_group file.");
            }
          }
        else
          {
          throw runtime_error("Error while parsing resource_group file.");
          }
        }
      else
        {
        throw runtime_error("Couldn't find parent group while parsing resource_group file.");
        }
      }
    }

  fclose(fp);
  }

void FairshareTree::read_usage()
  {
  if (this->p_tree == NULL)
    throw runtime_error("Fairshare not yet intialized.");

  FILE *fp;    /* file pointer to usage file */
  struct group_node_usage grp;  /* struct used to read in usage info */
  group_info *ginfo;   /* ptr to current group usage */

  string filename = string("usage.")+this->p_name;

  if ((fp = fopen(filename.c_str(), "r")) == NULL) // fallthrough error, file will be created with the first write
    {
    string errtxt = string("Error opening usage file ")+filename+string(" for reading.");
    perror(errtxt.c_str());
    return;
    }

  while (fread(&grp, sizeof(struct group_node_usage), 1, fp))
    {
    ginfo = this->find_alloc_ginfo(grp.name);

    if (ginfo != NULL)
      {
      ginfo -> usage = grp.usage;
      ginfo -> temp_usage = grp.usage;
      }
    }

  fclose(fp);
  }

void FairshareTree::dump_to_file() const
  {
  if (this->p_tree == NULL)
    throw runtime_error("Trying to store empty fairshare.");

  string filename = string("usage.")+this->p_name;

  FILE *fp = fopen(filename.c_str(),"wb");
  if (fp == NULL)
    {
    string errtxt = string("Error while opening usage file for writing: ")+string(strerror(errno));
    throw runtime_error(errtxt);
    return;
    }

  this->dump_to_file(this->p_tree, fp);

  fclose(fp);
  }

void FairshareTree::dump_to_file(group_info *root, FILE *fp) const
  {
  struct group_node_usage grp;  /* used to write out usage info */
  memset(&grp,0,sizeof(struct group_node_usage));

  if (root == NULL)
    return;

  if (root -> usage != 1)   /* usage defaults to 1 */
    {
    strcpy(grp.name, root -> name);
    grp.usage = root -> usage;

    if (!fwrite(&grp, sizeof(struct group_node_usage), 1, fp))
      return;
    }

  this->dump_to_file(root -> sibling, fp);

  this->dump_to_file(root -> child, fp);
  }

void FairshareTree::decay()
  {
  if (this->p_tree == NULL)
    throw runtime_error("Trying to decay empty fairshare.");

  this->decay_tree(this->p_tree);
  }

void FairshareTree::decay_tree(group_info *root)
  {
  if (root == NULL)
    return;

  this->decay_tree(root -> sibling);
  this->decay_tree(root -> child);

  root -> usage /= 2;

  if (root -> usage == 0)
    root -> usage = 1;
  }
