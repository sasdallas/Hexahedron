/**
 * @file hexahedron/fs/vfs.c
 * @brief Virtual filesystem handler
 * 
 * @todo refcounts implemented
 * 
 * @warning Some code in here can be pretty messy.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/vfs.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <kernel/panic.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/misc/spinlock.h>

#include <structs/tree.h>
#include <structs/hashmap.h>
#include <structs/list.h>


/* Main VFS tree */
tree_t *vfs_tree = NULL;

/* Hashmap of filesystems (quick access) */
hashmap_t *vfs_filesystems = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:VFS", __VA_ARGS__)

/* Locks */
spinlock_t *vfs_lock;

/* Old reduceOS implemented a CWD system, but that was just for the kernel CLI */

/**
 * @brief Standard POSIX open call
 * @param node The node to open
 * @param flags The open flags
 */
void fs_open(fs_node_t *node, unsigned int flags) {
    if (!node) return;

    if (node->open) {
        return node->open(node, flags);
    }
}

/**
 * @brief Standard POSIX close call that also frees the node
 * @param node The node to close
 */
void fs_close(fs_node_t *node) {
    if (!node) return;

    if (node->close) {
        node->close(node);
    }

    // Free the node
    kfree(node);
}

/**
 * @brief Standard POSIX read call
 * @param node The node to read from
 * @param offset The offset to read at
 * @param size The amount of bytes to read
 * @param buffer The buffer to store the bytes in
 * @returns The amount of bytes read
 */
ssize_t fs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!node) return 0;

    if (node->read) {
        return node->read(node, offset, size, buffer);
    }

    return 0;
}

/**
 * @brief Standard POSIX write call
 * @param node The node to write to
 * @param offset The offset to write at
 * @param size The amount of bytes to write
 * @param buffer The buffer of the bytes to write
 * @returns The amount of bytes written
 */
ssize_t fs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!node) return 0;

    if (node->write) {
        return node->write(node, offset, size, buffer);
    }

    return 0;
}

/**
 * @brief Read directory
 * @param node The node to read the directory of
 * @param index The index to read from
 * @returns A directory entry structure or NULL
 */
struct dirent *fs_readdir(fs_node_t *node, unsigned long index) {
    if (!node) return NULL;

    if (node->flags & VFS_DIRECTORY && node->readdir) {
        return node->readdir(node, index);
    }

    return NULL;
}

/**
 * @brief Find directory
 * @param node The node to find the path in
 * @param name The name of the file to look for
 * @returns The node of the file found or NULL
 */
fs_node_t *fs_finddir(fs_node_t *node, char *path) {
    if (!node) return NULL;

    if (node->flags & VFS_DIRECTORY && node->finddir) {
        return node->finddir(node, path);
    }

    return NULL;
}

/**
 * @brief Make directory
 * @param path The path of the directory
 * @param mode The mode of the directory created
 * @returns Error code
 */
int fs_mkdir(char *path, mode_t mode) { return -ENOTSUP; }

/**
 * @brief Unlink file
 * @param name The name of the file to unlink
 * @returns Error code
 */
int fs_unlink(char *name) { return -ENOTSUP; }



/**** VFS TREE FUNCTIONS ****/

/**
 * @brief Dump VFS tree system (recur)
 * @param node The current tree node to search
 * @param depth The depth of the tree node
 */
static void vfs_dumpRecursive(tree_node_t *node, int depth) {
    if (!node) return;

    // Calculate spaces
    char spaces[256] = { 0 };
    for (int i = 0; i < ((depth > 256) ? 256 : depth); i++) {
        spaces[i] = ' ';
    }
    
    if (node->value) {
        vfs_tree_node_t *tnode = (vfs_tree_node_t*)node->value;
        if (tnode->node) {
            LOG(DEBUG, "%s%s (filesystem %s, %p) -> file %s (%p)\n", spaces, tnode->name, tnode->fs_type, tnode, tnode->node->name, tnode->node);
        } else {
            LOG(DEBUG, "%s%s (filesystem %s, %p) -> NULL\n", spaces, tnode->name, tnode->fs_type, tnode);
        }
    } else {
        LOG(DEBUG, "%s(node %p has NULL value)\n", spaces, node);
    }

    foreach (child, node->children) {
        vfs_dumpRecursive((tree_node_t*)child->value, depth +  1);
    }

} 

/**
 * @brief Dump VFS tree system
 */
void vfs_dump() {
    LOG(DEBUG, "VFS tree dump:\n");
    vfs_dumpRecursive(vfs_tree->root, 0);
}

/**
 * @brief Initialize the virtual filesystem with no root node.
 */
void vfs_init() {
    // Create the tree
    vfs_tree = tree_create("VFS");

    // Now create a blank root node.
    vfs_tree_node_t *root_node = kmalloc(sizeof(vfs_tree_node_t));
    root_node->fs_type = strdup("N/A");
    root_node->name = strdup("/");
    root_node->node = 0;
    tree_set_parent(vfs_tree, root_node);
    
    // Create the filesystem hashmap
    vfs_filesystems = hashmap_create("VFS filesystems", 10);

    // Load spinlocks
    vfs_lock = spinlock_create("vfs lock");

    LOG(INFO, "VFS initialized\n");
}


/**
 * @brief Canonicalize a path based off a CWD and an addition.
 * 
 * This basically will turn /home/blah (CWD) + ../other_directory/gk (addition) into
 * /home/other_directory/gk
 * 
 * @param cwd The current working directory
 * @param addition What to add to the current working directory
 */
char *vfs_canonicalizePath(char *cwd, char *addition) {
    char *canonicalize_path;

    // Is the first character of addition a slash? If so, that means the path we want
    // to canonicalize is just addition.
    if (*addition == '/') {
        canonicalize_path = strdup(addition);
        goto _canonicalize;
    }

    // We can just combine the paths now.
    if (cwd[strlen(cwd)-1] == '/') {
        // cwd ends in a slash (note that normally this shouldn't happen)
        canonicalize_path = kmalloc(strlen(cwd) + strlen(addition));
        sprintf(canonicalize_path, "%s%s", cwd, addition); // !!!: unsafe
    } else {
        // cwd does not end in a slash
        canonicalize_path = kmalloc(strlen(cwd) + strlen(addition) + 1);
        sprintf(canonicalize_path, "%s/%s", cwd, addition); // !!!: unsafe
    }


_canonicalize: ;
    // At this point canonicalize_path holds a raw path to parse.
    // Something like: /home/blah/../other_directory/gk
    // We'll pull a trick from old coding and parse it into a list, iterate each element and go.
    list_t *list = list_create("canonicalize list");

    // Now add all the elements to the list, parsing each one.  
    size_t path_size = 0;  
    char *pch;
    char *save;
    pch = strtok_r(canonicalize_path, "/", &save);
    while (pch) {
        if (!strcmp(pch, "..")) {
            // pch = "..", go up one.
            node_t *node = list_pop(list);
            if (node) {
                kfree(node);
                kfree(node->value);
            }
        } else if (!strcmp(pch, ".")) {
            // Don't add it to the list, it's just a "."
        } else if (*pch) {
            // Normal path, add to list
            list_append(list, strdup(pch));
            path_size += strlen(pch) + 1; // +1 for the /
        }

        pch = strtok_r(NULL, "/", &save);
    }

    // Now we can free the previous path.
    kfree(canonicalize_path);

    // Allocate the new output path (add 1 for the extra \0)
    char *output = kmalloc(path_size + 1); // Return value
    memset(output, 0, path_size + 1);

    if (!path_size) {
        // The list was empty? No '/'s? Assume root directory
        LOG(WARN, "Empty path_size after canonicalization - assuming root directory.\n");
        kfree(output); // realloc is boring 
        output = strdup("/");
    } else {
        // Append each element together
        foreach(i, list) {
            // !!!: unsafe call to sprintf
            sprintf(output, "%s/%s", output, i->value);
        }
    }

    list_destroy(list, true); // Destroy the list & free contents
    return output;
}

/**
 * @brief Mount a specific node to a directory
 * @param node The node to mount
 * @param path The path to mount to
 * @returns A pointer to the tree node or NULL
 */
tree_node_t *vfs_mount(fs_node_t *node, char *path) {
    // Sanity checks
    if (!node) return NULL;
    if (!vfs_tree) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "vfs", "*** vfs_mount before init\n");
        __builtin_unreachable();
    }

    if (!path || path[0] != '/') {
        // lol
        LOG(WARN, "vfs_mount bad path argument - cannot be relative\n");
        return NULL;
    }

    tree_node_t *parent_node = vfs_tree->root; // We start at the root node

    spinlock_acquire(vfs_lock);

    // If the path strlen is 0, then we're trying to set the root node
    if (strlen(path) == 1) {
        // We don't need to allocate a new node. There's a perfectly good one already!
        vfs_tree_node_t *root = vfs_tree->root->value;
        root->node = node;
        goto _cleanup;
    }

    // Ok we still have to do work :(
    // We can use strtok_r and iterate through each part of the path, creating new nodes when needed.

    char *pch;
    char *saveptr;
    char *strtok_path = strdup(path); // strtok_r messes with the string

    pch = strtok_r(strtok_path, "/", &saveptr);
    while (pch) {
        int found = 0; // Did we find the node?

        foreach(child, parent_node->children) {
            vfs_tree_node_t *childnode = ((tree_node_t*)child->value)->value; // i hate trees
            if (!strcmp(childnode->name, pch)) {
                // Found it
                found = 1;
                parent_node = child->value;
                break;
            }
        }
    
        if (!found) {
            // LOG(INFO, "Creating node at %s\n", pch);

            vfs_tree_node_t *newnode = kmalloc(sizeof(vfs_tree_node_t)); 
            newnode->name = strdup(pch);
            newnode->fs_type = NULL;
            newnode->node = NULL;
            parent_node = tree_insert_child(vfs_tree, parent_node, newnode);
        }

        pch = strtok_r(NULL, "/", &saveptr);
    } 

    // Now parent_node should point to the newly created directory
    vfs_tree_node_t *entry = parent_node->value;
    entry->node = node;

    kfree(strtok_path);

_cleanup:
    spinlock_release(vfs_lock);
    return parent_node;
}

/**
 * @brief Register a filesystem in the hashmap
 * @param name The name of the filesystem
 * @param fs_callback The callback to use
 */
int vfs_registerFilesystem(char *name, mount_callback mount) {
    if (!vfs_filesystems) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "vfs", "*** vfs_registerFilesystem before init\n");
        __builtin_unreachable();
    }
    
    vfs_filesystem_t *fs = kmalloc(sizeof(vfs_filesystem_t));
    fs->name = strdup(name); // no, filesystems cannot unregister themselves.
    fs->mount = mount;

    hashmap_set(vfs_filesystems, fs->name, fs);

    return 0;
}

/**
 * @brief Try to mount a specific filesystem type
 * @param name The name of the filesystem
 * @param argp The argument you wish to provide to the mount method (fs-specific)
 * @param mountpoint Where to mount the filesystem (leave as NULL if the driver takes care of this)
 * @returns The node created or NULL if it failed
 */
fs_node_t *vfs_mountFilesystemType(char *name, char *argp, char *mountpoint) {
    if (!name) return NULL;
    
    vfs_filesystem_t *fs = hashmap_get(vfs_filesystems, name);
    if (!fs) {
        LOG(WARN, "VFS tried to mount unknown filesystem type: %s\n", name);
        return NULL;
    }

    if (!fs->mount) {
        LOG(WARN, "VFS found invalid filesystem '%s' when trying to mount\n", fs->name);
        return NULL;
    }

    fs_node_t *node = fs->mount(argp, mountpoint);
    if (!node) {
        return NULL;
    }

    // Quick hack to allow mounting by the device itself
    if (!mountpoint) {
        return node;
    }

    tree_node_t *tnode = vfs_mount(node, mountpoint);
    if (!tnode) {
        LOG(WARN, "VFS failed to mount filesystem '%s' - freeing node\n");
        kfree(node);
        return NULL;    
    }
    
    vfs_tree_node_t *vfsnode = (vfs_tree_node_t*)tnode->value;

    // Copy fs_type
    vfsnode->fs_type = strdup(name);

    // All done
    return node;
}

/**
 * @brief Get the mountpoint of a specific node
 * 
 * The VFS tree does not contain files part of an actual filesystem.
 * Rather it's just a collection of mountpoints.
 * 
 * Files/directories that are present on the root partition do not exist within
 * our tree - instead, finddir() is used to get them (by talking to the fs driver)
 * 
 * Therefore, the first thing that needs to be done is to get the mountpoint of a specific node.
 * 
 * @param path The path to get the mountpoint of
 * @param remainder An output of the remaining path left to search
 * @returns A pointer to the mountpoint or NULL if it could not be found.
 */
static fs_node_t *vfs_getMountpoint(const char *path, char **remainder) {
    // Last node in the tree
    tree_node_t *last_node = vfs_tree->root;
    
    // We're going to tokenize the path, and find each token.
    char *pch;
    char *save;
    char *path_clone = strdup(path); // strtok_r trashes path

    int _remainder_depth = 0;

    pch = strtok_r(path_clone, "/", &save);
    while (pch) {
        // We have to search until we don't find a match in the tree.
        int node_found = 0; // If still 0 after foreach then we found the mountpoint.

        foreach(childnode, last_node->children) {
            tree_node_t *child = (tree_node_t*)childnode->value;
            vfs_tree_node_t *vnode = (vfs_tree_node_t*)child->value;

            if (!strcmp(vnode->name, pch)) {
                // Match found, update variables
                last_node = child;
                node_found = 1;
                break;
            }
        }

        if (!node_found) {
            break; // We found our last node.
        }
    
        _remainder_depth += strlen(pch) + 1; // + 1 for the /

        pch = strtok_r(NULL, "/", &save);
    }

    *remainder = (char*)path + _remainder_depth;
    vfs_tree_node_t *vnode = (vfs_tree_node_t*)last_node->value;
    kfree(path_clone);

    return vnode->node;
}



/**
 * @brief Kernel open method but relative
 * 
 * This is just an internal method used by @c kopen - it will take in the next part of the path,
 * and find the next node.
 * 
 * @todo    This needs some symlink support but that sounds like hell to implement.
 *          The only purpose of this function is to handle symlinks (and be partially recursive in doing so)
 * 
 * @param current_node The parent node to search
 * @param next_token Next token/part of the path
 * @param flags The opening flags of the file
 * @returns A pointer to the next node, ready to be fed back into this function. NULL is returned on a failure.
 */
static fs_node_t *kopen_relative(fs_node_t *current_node, char *path, unsigned int flags) {
    if (!path || !current_node) {
        LOG(WARN, "Bad arguments to kopen_recur\n");
        return NULL;
    }

    // TODO: Symlinks, main bulk part of this
    return fs_finddir(current_node, path);
}


/**
 * @brief Kernel open method 
 * @param path The path of the file to open (always absolute in kmode)
 * @param flags The opening flags - see @c fcntl.h
 * 
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen(const char *path, unsigned int flags) {
    if (!path) return NULL;
    
    // First get the mountpoint of path.
    char *path_offset = (char*)path;
    fs_node_t *node = vfs_getMountpoint(path, &path_offset);

    if (!(*path_offset)) {
        // Usually this means the user got what they want, the mountpoint, so I guess just open that and call it a da.
        goto _finish_node;
    }

    // Now we can enter a kopen_relative loop
    char *pch;
    char *save;
    pch = strtok_r(path_offset, "/", &save);

    while (pch) {
        if (!node) break;
        node = kopen_relative(node, pch, flags);

        if (node && node->flags == VFS_FILE) {
            // TODO: What if the user has a REALLY weird filesystem?
            break;
        }
        
        pch = strtok_r(NULL, "/", &save); 
    }

    if (node == NULL) {
        // Not found
        return NULL;
    }

_finish_node: ;
    // Always clone the node to prevent mucking around with datastructures.
    fs_node_t *retnode = kmalloc(sizeof(fs_node_t));
    memcpy(retnode, node, sizeof(fs_node_t));
    fs_open(retnode, flags);
    return retnode;
}

/**
 * @brief Kernel open method for usermode (uses current process' working directory)
 * @param path The path of the file to open
 * @param flags The opening flags - see @c fcntl.h
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen_user(const char *path, unsigned int flags) {
    return NULL; // unimplemented
}
