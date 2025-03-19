
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "huff.h"
#include "debug.h"


//helper method to get a sequence of n bits where 0 = leaf and 1 = internal node
//since we will be recursively calling, we will use a pointer to out and count
//to keep track of the actual value
static void postorder(NODE* root, unsigned char* out, int* count) {
    //in postorder traversal, we do left, right, then root
    if(root->left != NULL)
        postorder(root->left, out, count);

    if(root->right != NULL)
        postorder(root->right, out, count);

    //if leaf, shift char left 1 and add 0, then increment count
    if(root->left == NULL && root->right == NULL)   {
        *out = *out << 1;
        *out += 0;
        (*count)++;
    }
    //else, shift char left 1 and add 1, then increment count
    else    {
        *out = *out << 1;
        *out += 1;
        (*count)++;
    }

    //if count = 8, out is filled up so we output it and reset
    if(*count == 8)  {
        fputc(*out, stdout);
        *out = 0;
        *count = 0;
    }
    return;
}


// helper method to postorder the tree and output the symbols left to right
static void symboltraversal(NODE* root) {


    // recursive call on left & right subtrees
    if (root->left != NULL)
        symboltraversal(root->left);
    if (root->right != NULL)
        symboltraversal(root->right);

    // if leaf, we output its symbol
    if (root->left == NULL && root->right == NULL) {
        node_for_symbol[root->symbol] = root;
        // if symbol is 256, it is end block so output FF 00
        if (root->symbol == 256) {
            fputc(0xFF, stdout);
            fputc(0x00, stdout);

        // if symbol is 0xFF, we out put 0xFF & any byte other than 0x00. I will output 0xFF twice
        } else if (root->symbol == 255) {
            fputc(0xFF, stdout);
            fputc(0xFF, stdout);
        // else we output its symbol normally
        } else {
            fputc(root->symbol, stdout);
        }
    }
    return;
}


// emits a description of the huffman tree used to compress the current block
void emit_huffman_tree() {

    // output the number of nodes, given as a 2 byte sequence in big-endian order
    unsigned char msb;
    unsigned char lsb;
    msb = ((num_nodes & 0xFF00) >> 8);
    lsb = (num_nodes & 0x00FF);
    fputc(msb, stdout);
    fputc(lsb, stdout);

    // clean out the node_for_symbol array
    for (int i = 0; i < MAX_SYMBOLS; i++)
    {
        node_for_symbol[i] = NULL;
    }

    // a sequence of n bits from a postorder traversal (0 = leaf, 1 = internal node)
    unsigned char out = 0;
    int count = 0;
    postorder(nodes, &out, &count);
    // after finishing the recursive call, we pad out with 0s if count not 0
    if (count != 0 && count < 8)
    {
        out = out << (8 - count);
        fputc(out, stdout);
    }

    // a sequence of bytes gives the values of the symbols at the (n+1)/2
    // leaves of the tree. each byte is 1 symbol except FF 00 which is end block
    symboltraversal(nodes);
    return;
}


//helper method to traverse tree and set its node's parents
static void set_parents(NODE* root) {
    //if left is not null, set left's parent as root and recursive call set_parents
    if (root->left != NULL) {
        root->left->parent = root;
        set_parents(root->left);
    }
    //if right is not null, set right's parent as root and recursive call set_parents
    if (root->right != NULL) {
        root->right->parent = root;
        set_parents(root->right);
    }
    return;
}


//helper method to traverse tree and set node_for_symbol with its leaves in postorder
static void postordertrackleaf(NODE* root, int* add) {
    // postorder traversal goes left, right, then root
    if(root->left != NULL)
        postordertrackleaf(root->left, add);
    if(root->right != NULL)
        postordertrackleaf(root->right, add);
    // if we are at leaf we put it into the node_for_symbol array
    if (root->left == NULL && root->right == NULL) {
        *(node_for_symbol + *add) = root;
        (*add)++;
    }
}


static int global_eof_flag = 0;
static int global_stack_empty_flag = 0;
// reads description of huffman tree & reconstructs the tree from the description
int read_huffman_tree() {
    // reading 2 bytes first
    unsigned char num_nodes1 = fgetc(stdin);
    if (feof(stdin)) // return if EOF at beginning of block
        return 1;
    num_nodes = (num_nodes1 << 8) ;
    unsigned char num_nodes2 = fgetc(stdin);

    // the first 2 bytes read will be num_nodes
    num_nodes += num_nodes2;
    if (num_nodes < 1) // num_nodes can't be less than 1
        return -1;

    // time to build the huffman tree by reading each bit
    // the next num_nodes/8 bytes will be the postorder traversal bit sequence
    int position = num_nodes - 1;
    NODE temp;
    int last = 0;
    int top = -1; // index of top of stack
    int i = num_nodes;
    while (i > 0) {
        unsigned char a = fgetc(stdin);
        // inner loop to use all 8 bits of the byte
        for (int count = 7; count >= 0; count--, i--){
            char bit = (a >> count);
            bit = bit % 2;

            // if the bit is 0 (leaf), increment top of stack
            if (bit == 0)
                top++;
            //otherwise, we're gunna swap 2 nodes & set children
            else {
                if (top > 0) {

                    // pop top node from stack & move it to current position in the tree
                    // this node becomes right child of new internal node

                    temp = nodes[top];
                    nodes[top--] = nodes[position - last];
                    nodes[position - last] = temp;

                    // pop next node from stack & move it to position next to previous node
                    // this node becomes the left child of new internal node
                    temp = nodes[top];
                    nodes[top] = nodes[position - last - 1];
                    nodes[position - last - 1] = temp;


                    // set right & left pointers of internal node to point to the nodes we just popped
                    nodes[top].right = &nodes[position - last++];
                    nodes[top].left = &nodes[position - last++];


                } else { // otherwise, the stack is empty. error.
                    global_stack_empty_flag = -1;
                    return -1;
                }
            }
        }
    }

    if (feof(stdin) && i > 0) // premature EOF
        return -1;

    // we traverse the tree post order and put leaf pointers in the node_for_symbol array
    int zzz = 0;
    postordertrackleaf(nodes, &zzz);

    // we are uncertain of the bytes representing leaves because end block and 0xFF
    // bot are represented by 2 bytes. this we fgetc as we go
    for(int i = 0; i < zzz; i++)    {
        unsigned char asd = fgetc(stdin);
        if(feof(stdin) || ferror(stdin))
            return -1;
        //if next char is 0xFF
        if(asd == 0xFF) {
            //get another char
            unsigned char qwe = fgetc(stdin);
            if(feof(stdin) || ferror(stdin))
                return -1;
            //if 0xFF is followed by 0x00 it is the end block symbol
            if(qwe == 0x00)
                (*(node_for_symbol+i))->symbol = 256;
            //if it is followed by anything else, it is 0xFF
            else    {
                (*(node_for_symbol+i))->symbol = 0xFF;
            }
        }
        //else the next leaf's symbol is the gotten char
        else
            (*(node_for_symbol+i))->symbol = asd;
    }


    // then we set the parents in the tree
    set_parents(nodes);

    return 0;
}


// helper method to construct a histogram form the block
// also put symbol nodes into nodes_for_symbol array
static void construct_histogram(int blocksize) {
   // use this as the current index in nodes array
   num_nodes = 0;

   // use count to track symbol count
   int count = 0;

   // current symbol we're looking at
   unsigned char current_symbol;

   // loops through current symbols & if there is, increment its weight
   while (count < blocksize)
   {
       current_symbol = current_block[count];
       if (node_for_symbol[current_symbol] == NULL)
       {
           // create new node for symbol
           nodes[num_nodes].left = NULL;
           nodes[num_nodes].right = NULL;
           nodes[num_nodes].parent = NULL;
           nodes[num_nodes].symbol = current_symbol;
           nodes[num_nodes].weight = 1;
           // we use current_symbol as the index of node_for_symbol at the same time
           node_for_symbol[current_symbol] = &nodes[num_nodes++];
        }

       else
           // if node already exists, just increment its weight. we can use current_symbol as index
           node_for_symbol[current_symbol]->weight++;
       // increment count & go to next symbol
       count++;
   }


   // node to represent end of file
   nodes[num_nodes].left = NULL;
   nodes[num_nodes].right = NULL;
   nodes[num_nodes].parent = NULL;
   nodes[num_nodes].symbol = 256;
   nodes[num_nodes].weight = 0;

   node_for_symbol[256] = &nodes[num_nodes++];

}


// helper function to sort/heapify the nodes in the nodes array
static void min_heapify(int start, int end, int i) {

    int left = (i*2)+1;
    int right = (i*2)+2;
    int min = i;

    // if right smaller than parent, index of minimum is now right
    if (nodes[right].weight < nodes[min].weight && right < end && right >= start)
        min = right;

    // if left smaller than parent, index of minimum is now left
    if (nodes[left].weight < nodes[i].weight && left < end && left >= start)
        min = left;

    // if current subtree min index not the index parameter, swap the two & heapify/re-sort again
    if (min != i) {
        NODE temp = nodes[min];
        nodes[min] = nodes[i];
        nodes[i] = temp;

        // recursively heapify the subtree
        min_heapify(start, end, min);
    }
}


// helper method to construct a huffman tree from the histogram
static void construct_huffmantree() {
    // gets the total number of nodes using our leaves
    int totalnodes = 2 * num_nodes - 1;

    // initialize at 0 rather than totalnodes - 1. b/c we using a heap
    int position = 0;

    // current number of nodes whose position is not decided in the huffman tree
    int currunsorted = num_nodes;

    int index;
    NODE temp;

    // heapify the initial set of leaves. this primes us to begin constructing the tree
    for (int i = ((num_nodes / 2) - 1); i >= 0; i--) {
        min_heapify(0, num_nodes, i);
    }

    // now start building the huffman tree by combining nodes
    while (currunsorted > 1)
    {

        // take the smallest node (root of the heap) & move it to the final position in the tree array
        temp = nodes[0];
        index = totalnodes - position - 1;
        nodes[0] = nodes[index];
        nodes[index] = temp;
        // decrement the number of unsorted nodes since we've placed one in its final position
        currunsorted--;

        // now fix the heap
        // shift all the remaining unsorted nodes to the left by 1 position to maintain the heap structure
        for (int i = 0; i < currunsorted; i++)
        {
            temp = nodes[i];
            nodes[i] = nodes[i + 1];
            nodes[i + 1] = temp;
        }

        // re-sort the heap to restore the minheap
        for (int i = ((currunsorted / 2) - 1); i >= 0; i--)
        {
            min_heapify(0, currunsorted, i);
        }

        // store a pointer to the node we just removed from the heap
        NODE *node1 = (nodes + index);
        position++;


        // repeat the process to extract the second smallest node
        temp = nodes[0];
        index = totalnodes - position - 1;
        nodes[0] = nodes[index];
        nodes[index] = temp;
        // decrement currunsorted
        currunsorted--;

        // now fix the heap
        // shift the remaining unsorted nodes again
        for (int i = 0; i < currunsorted; i++)
        {
            temp = nodes[i];
            nodes[i] = nodes[i + 1];
            nodes[i + 1] = temp;
        }

        // re-sort the heap again
        for (int i = ((currunsorted / 2) - 1); i >= 0; i--)
        {
            min_heapify(0, currunsorted, i);
        }

        // store a pointer to the second smallest node we just removed
        NODE *node2 = (nodes + index);
        position++;


        // now create a new internal node combining node1 and node2
        nodes[currunsorted].left = node1;
        nodes[currunsorted].right = node2;
        nodes[currunsorted].parent = NULL;
        nodes[currunsorted].weight = node1->weight + node2->weight; // new internal node weight is sum of its children's weights
        nodes[currunsorted].symbol = -1;
        currunsorted++;

        // after inserting the new node into the heap, re-sort the heap again
        for (int i = ((currunsorted / 2) - 1); i >= 0; i--)
        {
            min_heapify(0, currunsorted, i);
        }
    }

    // set num_nodes to the total number of nodes, including both the leaf nodes and the internal nodes
    num_nodes = totalnodes;
    return;
}


// helper method for outputcompressed. sets the codes at the nodes
static void set_codes(NODE* leaf) {
    // traverse up the tree from the leaf node to the root
    while (leaf != NULL && leaf->parent != NULL) {
        // if the current node is the left child of its parent, assign 0 to parent's weight
        if (leaf->parent->left == leaf)
            leaf->parent->weight = 0;
        // if the current node is the right child of its parent, assign 1 to parent's weight
        else if (leaf->parent->right == leaf)
            leaf->parent->weight = 1;
        // move up the tree. setting codes at each step until root is reached
        leaf = leaf->parent;
    }
}


// helper method for outputcompressed. uses the already set codes to write out the codes by recursively going thru the tree
static void write_codes(NODE* root, int* count, unsigned char* out) {
    if ((root->left == NULL && root->right == NULL))
        return;
    // we put the bits into char out one by one & increment count
    *out = *out << 1;
    (*count)++;
    if (root->weight)
        *out = *out | 0x1;
    // if count = 8 then we filled all 8 bits of out so we output it and reset count to 0
    if (*count == 8) {
        fputc(*out, stdout);
        *out = 0;
        *count = 0;
    }
    // depending on whether weight is 0 or 1, recursively go through the roots children
    if (root->weight == 0)
        write_codes(root->left, count, out);
    if (root->weight == 1)
        write_codes(root->right, count, out);
}


// helper method to convert the block of data to compressed data & output
static int outputcompressed(int blocksize) {
    // out is 1 byte to be printed while count keeps track of when out reaches 8 bits
    unsigned char out = 0;
    int count = 0;
    int index = 0;

    // compression to be repeated for every element in buffer
    while (index < blocksize)
    {
        // node_for_symbol[current_block[i]] represents the current leaf node
        // move up the leaf node setting the 0s and 1s
        set_codes(node_for_symbol[current_block[index++]]);
        // then recursively go down from the root write out the bytes
        write_codes(&nodes[0], &count, &out);
    }

    // do this one more time for the end of block symbol
    set_codes(node_for_symbol[256]);
    write_codes(&nodes[0], &count, &out);


    // write padding if needed
    if (count > 0 && count < 8)
    {
        out = out << (8 - count);
        fputc(out, stdout);
    }

    return 0;
}


// reads one block of data from standard input and emits corresponding compressed data to standard output
int compress_block() {
    // get blocksize from global_options upper 16 bits
   int blocksize = ((unsigned int)global_options >> 16);
   blocksize++;
   int read = 0;
   int c;


   // clear out current_block to get it ready to compress current block
   for (int i = 0; current_block[i] != '\0'; i++)
   {
        current_block[i] = '\0';
   }

   // clear out nodes to get it ready to compress current block
   for (int i = 0; i < 2 * MAX_SYMBOLS - 1; i++)
   {
        nodes[i].parent = NULL;
        nodes[i].left = NULL;
        nodes[i].right = NULL;
        nodes[i].weight = -1;
        nodes[i].symbol = -1;
   }

   // clear out node_for_symbol to get it ready to compress current block
   for (int i = 0; i < MAX_SYMBOLS; i++)
   {
       node_for_symbol[i] = NULL;
   }

   // read 1 block of data from stdin to array current_block
   do
   {
       // get char then put in array block_size
       c = fgetc(stdin);

       if (c == EOF)
       {
           // if an error occurs, return -1
           if (ferror(stdin))
               return -1;
       }
       else
           // otherwise, add the next byte to the current_block buffer
           current_block[read++] = (unsigned char)c;
       // if end of file, exit loop
       if (feof(stdin))
           break;
   } while (1);

   if (ferror(stdin))
       return -1;


   // no data was read
   if (read == 0)
       return 0;

   // read will be the same as blocksize or end block
   blocksize = read;
   // if blocksize <= 0, it is an error so return -1
   if(blocksize <= 0)
       return -1;

   // we use blocksize to keep track of EOF or end of block
   // constructs a histogram from the buffer
   construct_histogram(blocksize);

   // uses histogram to construct huffmantree
   construct_huffmantree();

   // traverses through huffman tree & sets parents
   set_parents(&nodes[0]);

   // node_for_symbol array is set with leaves in construct_histogram

   // emits description of tree
   emit_huffman_tree();

   // compress the data & output it
   return outputcompressed(blocksize);
}


// reads one block of compressed data from standard input and writes the corresponding uncompressed data to standard output
int decompress_block() {

    // clean out node_for_symbol
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        node_for_symbol[i] = NULL;
    }

    // clean out nodes
    for (int i = 0; i < 2 * MAX_SYMBOLS - 1; i++) {
        nodes[i].weight = -1;
        nodes[i].symbol = -1;
        nodes[i].left = NULL;
        nodes[i].right = NULL;
        nodes[i].parent = NULL;
    }

    int q = read_huffman_tree();
    if (q)
        return -1;

    NODE *a = nodes;
    int c;
    int end_block_flag = 0;
    int count = 0;

    // read the first character before entering the loop
    c = fgetc(stdin);

    // loop to traverse the Huffman tree based on bits from stdin
    while (1) {
        if (feof(stdin) || ferror(stdin)) {
            global_eof_flag = -1;
            return -1;
        }

        // process each bit in the current character
        for (int i = 7; i >= 0 && count < 8; i--) {
            count++;
            char bit = (c >> i) % 2;

            // traverse left if bit is 0, right if bit is 1
            if (bit == 0)
                a = a->left;
            else
                a = a->right;

            // check if a leaf node is encountered
            if (a->left == NULL && a->right == NULL) {
                if (a->symbol == 256) {  // end block symbol.
                    end_block_flag = 1;
                    return 0;  // end of block found. exit.
                }

                // output the symbol and reset to the root
                fputc(a->symbol, stdout);
                a = nodes;  // go back to the root for the next symbol
            }
        }

        // read the next character after processing the previous one
        if (count == 8) {
            c = fgetc(stdin);
            count = 0;
        }
    }

    if (!end_block_flag) // end block symbol was not found
        return -1;


    return 0;
}


// reads raw data from standard input, writes compressed data to standard output
int compress() {
   int error = 0;
   // while not at EOF, call compress_block
   while (1)
   {
       if (feof(stdin) || ferror(stdin)) // if we reach end of block OR an error, stop compressing blocks
           break;
       // if compress_block returns -1 & we did not compress any blocks return -1
       error = compress_block();
       if (error == -1)
           return -1;
   }
   // if error occurs, return -1
   if (ferror(stdin))
       return -1;
   fflush(stdout);
   return 0;
}


// reads compressed data from standard input, writes uncompressed data to standard output
int decompress() {
    int error = 0;
    while (feof(stdin) == 0) {
        // while not at EOF, call decompress_block
        error = decompress_block();
        if (error && global_eof_flag)
            return -1;
        else if (error && global_stack_empty_flag)
            return -1;
        else if(ferror(stdin))
            return -1;
    }
    // if error return -1
    if(ferror(stdin))
        return -1;
    fflush(stdout);
    return 0;
}


// helper method strequal to compare if 2 strings are equal
static int strequal(char* first, char* second) {
   while (*first != '\0' && *second != '\0')
   {
       if (*first != *second)
           return 0;
       first++;
       second++;
   }
   return (*first == '\0' && *second == '\0'); // both strings should end at the same time
}


// validates command line arguments passed to the program
int validargs(int argc, char** argv) {


   // initialize to 0. this will store the flags we set based on arguments.
   global_options = 0;


   // only argument is file name, return -1
   if (argc == 1)
   {
       fprintf(stderr, "error. no arguments");
       return -1;
   }


   // if the first argument is -h then we set global_options to 0x1 & return 0
   if (strequal(*(argv + 1), "-h"))
   {
       global_options |= 0x1;
       return 0;
   }


   // if there are 2 arguments, one is bin/huff & the other must be -c or -d
   if (argc == 2)
   {
       // for compression, we set global_options as 2 with the default ffff0000
       // size and return with success
       if (strequal(*(argv + 1), "-c"))
       {
           global_options |= 0xffff0002;
           return 0;
       }
       // for decompression, we set global_options as 4 with the default ffff0000
       // size and returnw ith success
       else if (strequal(*(argv + 1), "-d"))
       {
           global_options |= 0xffff0004;
           return 0;
       }
       // if the argument is neither -c or -d, then return with -1
       else
           return -1;
   }


   // if there are 4 arguments the last 3 should be -c, -b, then a number
   if (argc == 4)
   {
       // if the 2nd argument is -c and the 3rd argument is -b
       if (strequal(*(argv + 1), "-c") && strequal(*(argv + 2), "-b"))
       {
           // while the 4th argument is not null & is between 0 & 9
           int blocksize = 0;
           char *blocksize_str = *(argv + 3); // FIXED: rather than incrementing argument itself, create variable to iterate over the characters
           while (*blocksize_str != '\0')
           {
               if (*blocksize_str < '0' || *blocksize_str > '9')
                   return -1;
               // multiply blocksize by 10 and add the new number to it by
               // by subtracting the ascii value of '0' from it
               blocksize = blocksize * 10 + (*blocksize_str - '0');
               blocksize_str++;
           }


           // if: blocksize is not within bounds, return -1
           if (blocksize < MIN_BLOCK_SIZE || blocksize > MAX_BLOCK_SIZE)
               return -1;


           // else: set global_options's upper 16 bits as blocksize - 1
           // then increment by 2 representing compression & return 0
           else
           {
               blocksize -= 1;
               global_options |= blocksize << 16;
               global_options |= 0x2;
               return 0;
           }
       }
   }


   // if no options match, return -1
   return -1;
}
