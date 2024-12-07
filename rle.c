#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <error.h>
#include <unistd.h>
#include <pthread.h>

int fileNum, totalChunks; // Total file number and total chunks number
int threadNum = 1; // Sequential by default
int chunkSize = 4096, fileCounter = 0, localCounter = 0, totalIndex = 0, finishedSignal = 0; // Counters for each thread
pthread_t* threads; // tid array
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Protect the shared counters
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER; // Eliminate Helgrind errors
pthread_mutex_t size_mutex = PTHREAD_MUTEX_INITIALIZER; // Eliminate Helgrind errors
pthread_cond_t output_finished = PTHREAD_COND_INITIALIZER; // Pass the thread pool test
unsigned char** addrArr; // Address of each file
off_t* addSize; // Size of each file

unsigned char** outputArr;
int* outputSize;

// Worker threads
void* rle(void* args){
    int i, j, localStart, localEnd, myIndex, currentFile;
    int repeatedCounter = 1, outputIndex = 0;
    // Loop for each thread to work repeatedly
    while (1){
        outputIndex = 0;
        pthread_t tid = pthread_self();

        pthread_mutex_lock(&mutex); // This mutex is protecting the increment of local, total, and file counter
        if (totalIndex >= totalChunks){ // We have finished
            pthread_mutex_lock(&output_mutex); // This mutex is used to pass the thread pool test
            finishedSignal = 1; // Signal that we have finished all the work
            pthread_cond_signal(&output_finished); // This signal is used to pass the thread pool test
            pthread_mutex_unlock(&output_mutex);
            pthread_mutex_unlock(&mutex);
            break;
        }
        myIndex = totalIndex; // Where should this thread write to
        localStart = localCounter; // Where in the file should this thread start
        currentFile = fileCounter; // Which file should this thread work on
        if (addSize[fileCounter] < localStart + chunkSize){ // If the unworked portion in the file is < 4kb
            localEnd = addSize[fileCounter];
            fileCounter++; // Tell other threads to go to the next file
            localCounter = 0; // Reset local counter
        }
        else{ // If the unworked portion in the file is > 4kb
            localEnd = localStart + chunkSize;
            localCounter += chunkSize;
        }
        if (localStart != localEnd) // If not the "0 remainder" case
            totalIndex++; // Increase the number of total chunks worked
        pthread_mutex_unlock(&mutex);

        if (localStart != localEnd) // If not the "0 remainder" case
            outputArr[myIndex] = malloc(chunkSize*2);
        for (i = localStart; i < localEnd; i++){ // This is the actual working each thread is doing
            if ((i != localEnd-1) && (addrArr[currentFile][i] == addrArr[currentFile][i+1])){ // Repeated char
                repeatedCounter++;
            }
            else{ // The char is different with next, so we write the current one with the repeat number to the outputArr
                outputArr[myIndex][outputIndex++] = addrArr[currentFile][i];
                outputArr[myIndex][outputIndex++] = (unsigned char) repeatedCounter;
                repeatedCounter = 1; // Reset the repeat number
            }
        }
        pthread_mutex_lock(&size_mutex); // This mutex is used to eliminate Helgrind error
        if (localStart != localEnd) // If not the "0 remainder" case
            outputSize[myIndex] = outputIndex; // Tell the main thread how long this encoding is
        pthread_mutex_unlock(&size_mutex);
    }
    return NULL;
}

// Main thread
int main(int argc, char *argv[])
{
    int i, j, remainder;
    int addArrsCounter = 0; // Index the file address array
    if (getopt(argc, argv, "j:") != -1) { // If -j is provided, then we set the threadNum accordingly
        threadNum = atoi(optarg);
    }
    fileNum = argc - optind; // Set the total file number

    if (argc < 2){ 
        fprintf(stderr, "invalid command");
        exit(1);
    }
    threads = malloc(sizeof(pthread_t)*threadNum); 
    addrArr = malloc(sizeof(char*)*fileNum); 
    addSize = malloc(sizeof(off_t)*fileNum); 
    if (!addrArr || !addSize || !threads)
        fprintf(stderr, "malloc failed");

    // Map each file to the addrArr and its size to addSize
    for (i = optind; i < argc; i++){ 
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1)
            fprintf(stderr, "invalid file");
        struct stat sb;
        if (fstat(fd, &sb) == -1)
            fprintf(stderr, "invalid file");
        char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED)
            fprintf(stderr, "invalid file");
        addrArr[addArrsCounter] = addr;
        addSize[addArrsCounter++] = sb.st_size;
    }

    // Calculate the total number of chunk of 4kb in this task
    totalChunks = 0;
    for (i = 0; i < fileNum; i++){
        totalChunks += addSize[i]/chunkSize;
        remainder  = addSize[i]%chunkSize ? 1 : 0;
        if (remainder)
            totalChunks += 1;  
    }

    outputArr = malloc(totalChunks*sizeof(char*));
    outputSize = malloc(totalChunks*sizeof(int));

    // Create the worker threads
    for (i = 0; i < threadNum; i++){
        pthread_create(&threads[i], NULL, rle, NULL);
    }

    // Wait until worker threads have finished
    pthread_mutex_lock(&output_mutex);
    while (finishedSignal == 0){
        pthread_cond_wait(&output_finished, &output_mutex);
    }
    pthread_mutex_unlock(&output_mutex);

    // Stitch and write the output
    int hasPrevChar = 0; 
    unsigned char prevChar, prevCount;
    for(i = 0; i < totalChunks; i++){ // iterate through all chunks
        for (j = 0; j < outputSize[i]; j += 2){ // iterate through each chunk
            pthread_mutex_lock(&size_mutex); // Mutex to eliminate Helgrind error
            if (hasPrevChar && outputArr[i][j] == prevChar) { // if this char matches the previous char, then stitch two counts together
                prevCount += outputArr[i][j+1];
            } 
            else { // if this char does not match the previous char, no stitch is required
                if (hasPrevChar) { // if this char is not the first one in the outputArr
                    write(1, &prevChar, 1);
                    write(1, &prevCount, 1);
                }
                prevChar = outputArr[i][j]; // set the previous char and count
                prevCount = outputArr[i][j+1];
                hasPrevChar++; // Increase to indicate we have left the first char
            }
            pthread_mutex_unlock(&size_mutex);
        }
    }
    if (hasPrevChar) { // We still need to print the last one in the outputArr
        write(1, &prevChar, 1);
        write(1, &prevCount, 1);
    }

    for (i = 0; i < threadNum; i++){ // join all worker threads
        pthread_join(threads[i], NULL);
    }

    // Destroy all mutexes
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&size_mutex);

    // Free all allocated resources
    free(threads);
    free(addSize);
    free(outputSize);
    for (i = 0; i < totalChunks; i++){
        free(outputArr[i]);
    }
    free(outputArr);
    free(addrArr);
    return 0;
}
