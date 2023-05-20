#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

#define PATH_MAX_LEN 256

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
//proj5
struct dir *parse_path(char *name, char *file_name);

struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock lock;
  };

struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  buffer_cache_init();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  thread_current()->cur_dir = dir_open_root();
  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_terminate();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  //proj5
  char *parse_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
  struct dir *dir = parse_path(name, parse_name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)//proj5
                  && dir_add (dir, parse_name, inode_sector));//proj5
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  //proj5
  free(parse_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //proj5
  char *parse_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
  struct dir *dir = parse_path(name, parse_name);
  struct inode *inode = NULL;

  if (dir != NULL)//proj5
    dir_lookup (dir, parse_name, &inode);
  dir_close (dir);
  //proj5
  free(parse_name);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //proj5
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  //proj5
  struct dir *dir = dir_open(inode_open(ROOT_DIR_SECTOR));
  dir_add(dir, ".", ROOT_DIR_SECTOR);
  dir_add(dir, "..", ROOT_DIR_SECTOR);
  dir_close(dir);

  free_map_close ();
  printf ("done.\n");
}

//proj5
struct dir *parse_path(char *name, char *file_name)
{
    struct dir *dir = NULL;
    if (name == NULL || file_name == NULL || strlen(name) == 0)
        return NULL;
    char *path = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    strlcpy(path, name, PATH_MAX_LEN);
    if (path[0] == '/')
        dir = dir_open_root();
    else{
      if(thread_current()->cur_dir == NULL)
        thread_current()->cur_dir = dir_open_root();
      dir = dir_reopen(thread_current()->cur_dir);
    }
    if (dir == NULL || inode_is_dir(dir_get_inode(dir)) == false)
      return NULL;
    char* save_point;
    char* token = strtok_r(path, "/", &save_point);
    char* next_token = strtok_r(NULL, "/", &save_point);
    while(token != NULL && next_token != NULL){
      struct inode *temp_inode = NULL;
        if (dir_lookup(dir, token, &temp_inode) == false || inode_is_dir(temp_inode) == false){
            dir_close(dir);
            return NULL;
        }
        dir_close(dir);
        dir = dir_open(temp_inode);
        token = next_token;
        next_token = strtok_r(NULL, "/", &save_point);
    }
    if (token == NULL)
        strlcpy(file_name, ".", PATH_MAX_LEN);
    else
        strlcpy(file_name, token, PATH_MAX_LEN);
    free(path);
    return dir;
}

//proj5
bool create_dir(char *name)
{
    block_sector_t inode_sector = 0;
    char *parse_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    struct dir *dir = parse_path(name, parse_name);
    bool success = ((dir != NULL) && free_map_allocate(1, &inode_sector) && dir_create(inode_sector, 16) && dir_add(dir, parse_name, inode_sector));
    if (success == true){
        dir_add(dir, ".", inode_sector);
        dir_add(dir, "..", inode_get_inumber(dir_get_inode(dir)));
    }
    else
        if (inode_sector)
          free_map_release(inode_sector, 1);
    free(parse_name);
    dir_close(dir);
    return success;
}

//proj5
bool change_dir(char *path)
{
    char *name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    char *path_temp = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    path_temp[0] = '\0';
    strlcat(path_temp, path, PATH_MAX_LEN);
    strlcat(path_temp,"/0", PATH_MAX_LEN);
    struct dir *dir = parse_path(path_temp, name);
    free(path_temp);
    free(name);
    if(dir == NULL)
      return false;
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = dir;
    return true;
}
