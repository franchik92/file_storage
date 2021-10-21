#include <fsp_opened_files_bst.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void transplant(struct opened_file** root, struct opened_file* node_1, struct opened_file* node_2);
static struct opened_file* min(struct opened_file* node);

int fsp_opened_files_bst_insert(struct opened_file** root, const char* filename, int flags) {
    if(root == NULL || filename == NULL) return -1;
    
    struct opened_file* parent_node = NULL;
    struct opened_file* child_node = *root;
    
    while(child_node != NULL) {
        parent_node = child_node;
        int cmp = strcmp(filename, child_node->filename);
        if(cmp == 0) {
            return -2;
        } else if(cmp > 0) {
            child_node = child_node->right;
        } else {
            child_node = child_node->left;
        }
    }
    
    struct opened_file* file;
    if(strlen(filename) >= FSP_OPENED_FILES_ABR_FILE_MAX_LEN) return -3;
    if((file = malloc(sizeof(struct opened_file))) == NULL) return -4;
    strcpy(file->filename, filename);
    file->flags = flags;
    file->left = NULL;
    file->right = NULL;
    file->parent = parent_node;
    if(parent_node == NULL) {
        *root = file;
    } else if(strcmp(filename, child_node->filename) > 0) {
        child_node->right = file;
    } else {
        child_node->left = file;
    }
    
    return 0;
}

int fsp_opened_files_bst_delete(struct opened_file** root, const char* filename) {
    if(root == NULL || filename == NULL) return -1;
    struct opened_file* file;
    if((file = fsp_opened_files_bst_search(*root, filename)) == NULL) return -2;
    
    if(file->left == NULL) {
        transplant(root, file, file->right);
    } else if(file->right == NULL) {
        transplant(root, file, file->left);
    } else {
        struct opened_file* min_node = min(file->right);
        if(min_node->parent != file) {
            transplant(root, min_node, min_node->right);
            min_node->right = file->right;
            (min_node->right)->parent = min_node;
        }
        transplant(root, file, min_node);
        min_node->left = file->left;
        (min_node->left)->parent = min_node;
    }
    
    free(file);
    return 0;
}

void fsp_opened_files_bst_deleteAll(struct opened_file* root, void (*handler) (const char* filename)) {
    if(root == NULL) return;
    fsp_opened_files_bst_deleteAll(root->left, handler);
    fsp_opened_files_bst_deleteAll(root->right, handler);
    if(handler != NULL) handler(root->filename);
    free(root);
}

struct opened_file* fsp_opened_files_bst_search(struct opened_file* root, const char* filename) {
    if(root == NULL || filename == NULL) return root;
    int cmp = strcmp(filename, root->filename);
    if(cmp == 0) {
        return root;
    } else if(cmp > 0) {
        return fsp_opened_files_bst_search(root->right, filename);
    } else {
        return fsp_opened_files_bst_search(root->left, filename);
    }
}

static void transplant(struct opened_file** root, struct opened_file* node_1, struct opened_file* node_2) {
    if(node_1->parent == NULL) {
        *root = node_2;
    } else if(node_1 == (node_1->parent)->left) {
        node_1->parent->left = node_2;
    } else {
        node_1->parent->right = node_2;
    }
    if(node_2 != NULL) {
        node_2->parent = node_1->parent;
    }
}

static struct opened_file* min(struct opened_file* node) {
    while(node->left != NULL) {
        node = node->left;
    }
    return node;
}
